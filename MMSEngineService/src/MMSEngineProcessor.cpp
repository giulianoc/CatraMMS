
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "FFMpeg.h"
#include "GEOInfoTimes.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineProcessor.h"
#include "PersistenceLock.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
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

void MMSEngineProcessor::handleCheckIngestionEvent()
{

	try
	{
		if (isMaintenanceMode())
		{
			SPDLOG_INFO(
				string() +
				"Received handleCheckIngestionEvent, not managed it because of "
				"MaintenanceMode" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			return;
		}

		vector<tuple<int64_t, string, shared_ptr<Workspace>, string, string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>>
			ingestionsToBeManaged;

		try
		{
			// getIngestionsToBeManaged
			//	- in case we reached the max number of threads in MMS Engine,
			//		we still have to call getIngestionsToBeManaged
			//		but it has to return ONLY tasks that do not involve creation
			// of threads 		(a lot of important tasks 		do not involve
			// threads in MMS Engine) 	That is to avoid to block every thing in
			// case we reached the max number of threads 	in MMS Engine
			bool onlyTasksNotInvolvingMMSEngineThreads = false;

			int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
			if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
			{
				_logger->warn(
					string() +
					"Not enough available threads to manage Tasks involving "
					"more threads" +
					", _processorIdentifier: " + to_string(_processorIdentifier) +
					", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count()) +
					", _processorThreads + maxAdditionalProcessorThreads: " + to_string(_processorThreads + maxAdditionalProcessorThreads)
				);

				onlyTasksNotInvolvingMMSEngineThreads = true;
			}

			_mmsEngineDBFacade->getIngestionsToBeManaged(
				ingestionsToBeManaged, _processorMMS, _maxIngestionJobsPerEvent, _timeBeforeToPrepareResourcesInMinutes,
				onlyTasksNotInvolvingMMSEngineThreads
			);

			SPDLOG_INFO(
				string() + "getIngestionsToBeManaged result" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionsToBeManaged.size: " + to_string(ingestionsToBeManaged.size())
			);
		}
		catch (AlreadyLocked &e)
		{
			_logger->warn(
				string() + "getIngestionsToBeManaged failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			return;
			// throw e;
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "getIngestionsToBeManaged failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "getIngestionsToBeManaged failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			throw e;
		}

		for (tuple<int64_t, string, shared_ptr<Workspace>, string, string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>
				 ingestionToBeManaged : ingestionsToBeManaged)
		{
			int64_t ingestionJobKey;
			try
			{
				string ingestionJobLabel;
				shared_ptr<Workspace> workspace;
				string ingestionDate;
				string metaDataContent;
				string sourceReference;
				MMSEngineDBFacade::IngestionType ingestionType;
				MMSEngineDBFacade::IngestionStatus ingestionStatus;

				tie(ingestionJobKey, ingestionJobLabel, workspace, ingestionDate, metaDataContent, ingestionType, ingestionStatus) =
					ingestionToBeManaged;

				SPDLOG_INFO(
					string() + "json to be processed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", ingestionJobLabel: " + ingestionJobLabel +
					", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", ingestionDate: " + ingestionDate +
					", ingestionType: " + MMSEngineDBFacade::toString(ingestionType) +
					", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", metaDataContent: " + metaDataContent
				);

				try
				{
					if (ingestionType != MMSEngineDBFacade::IngestionType::RemoveContent)
					{
						_mmsEngineDBFacade->checkWorkspaceStorageAndMaxIngestionNumber(workspace->_workspaceKey);
					}
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						string() + "checkWorkspaceStorageAndMaxIngestionNumber failed" + ", _processorIdentifier: " +
						to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
					);
					string errorMessage = e.what();

					SPDLOG_INFO(
						string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) +
						", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + e.what()
					);
					try
					{
						_mmsEngineDBFacade->updateIngestionJob(
							ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedMaxStorageOrIngestionNumber, e.what()
						);
					}
					catch (runtime_error &re)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + re.what()
						);
					}
					catch (exception &ex)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + ex.what()
						);
					}

					throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						string() + "checkWorkspaceStorageAndMaxIngestionNumber failed" + ", _processorIdentifier: " +
						to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
					);
					string errorMessage = e.what();

					SPDLOG_INFO(
						string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) +
						", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + e.what()
					);
					try
					{
						_mmsEngineDBFacade->updateIngestionJob(
							ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedMaxStorageOrIngestionNumber, e.what()
						);
					}
					catch (runtime_error &re)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + re.what()
						);
					}
					catch (exception &ex)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + ex.what()
						);
					}

					throw e;
				}

				if (ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
				{
					// source binary download or uploaded terminated

					string sourceFileName = to_string(ingestionJobKey) + "_source";

					{
						shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
							_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
								MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
							);

						localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

						localAssetIngestionEvent->setExternalReadOnlyStorage(false);
						localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
						localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
						localAssetIngestionEvent->setMMSSourceFileName("");
						localAssetIngestionEvent->setWorkspace(workspace);
						localAssetIngestionEvent->setIngestionType(ingestionType);
						localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

						localAssetIngestionEvent->setMetadataContent(metaDataContent);

						shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
						_multiEventsSet->addEvent(event);

						SPDLOG_INFO(
							string() + "addEvent: EVENT_TYPE (INGESTASSETEVENT)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
							", getEventKey().second: " + to_string(event->getEventKey().second)
						);
					}
				}
				else // Start_TaskQueued
				{
					json parametersRoot;
					try
					{
						parametersRoot = JSONUtils::toJson(metaDataContent);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string("metadata json is not well format") +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", metaDataContent: " + metaDataContent;
						SPDLOG_ERROR(string() + errorMessage);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
							", errorMessage: " + errorMessage + ", processorMMS: " + ""
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}

					vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies;

					try
					{
						Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
						if (ingestionType == MMSEngineDBFacade::IngestionType::GroupOfTasks)
							validator.validateGroupOfTasksMetadata(workspace->_workspaceKey, parametersRoot);
						else
							dependencies = validator.validateSingleTaskMetadata(workspace->_workspaceKey, ingestionType, parametersRoot);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "validateMetadata failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
						);

						string errorMessage = e.what();

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
							", errorMessage: " + errorMessage + ", processorMMS: " + ""
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
						);

						string errorMessage = e.what();

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
							", errorMessage: " + errorMessage + ", processorMMS: " + ""
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}

					{
						if (ingestionType == MMSEngineDBFacade::IngestionType::GroupOfTasks)
						{
							try
							{
								manageGroupOfTasks(ingestionJobKey, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::AddContent)
						{
							MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
							string mediaSourceURL;
							string mediaFileFormat;
							string md5FileCheckSum;
							int fileSizeInBytes;
							bool externalReadOnlyStorage;
							try
							{
								tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool> mediaSourceDetails =
									getMediaSourceDetails(ingestionJobKey, workspace, ingestionType, parametersRoot);

								tie(nextIngestionStatus, mediaSourceURL, mediaFileFormat, md5FileCheckSum, fileSizeInBytes, externalReadOnlyStorage) =
									mediaSourceDetails;
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}

							try
							{
								if (externalReadOnlyStorage)
								{
									shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
										_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
											MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
										);

									localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
									localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
									localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

									localAssetIngestionEvent->setExternalReadOnlyStorage(true);
									localAssetIngestionEvent->setExternalStorageMediaSourceURL(mediaSourceURL);
									localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
									// localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
									// localAssetIngestionEvent->setMMSSourceFileName("");
									localAssetIngestionEvent->setWorkspace(workspace);
									localAssetIngestionEvent->setIngestionType(ingestionType);
									localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

									localAssetIngestionEvent->setMetadataContent(metaDataContent);

									shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
									_multiEventsSet->addEvent(event);

									SPDLOG_INFO(
										string() +
										"addEvent: EVENT_TYPE "
										"(INGESTASSETEVENT)" +
										", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
										to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
										", getEventKey().second: " + to_string(event->getEventKey().second)
									);
								}
								else
								{
									// 0: no m3u8
									// 1: m3u8 by .tar.gz
									// 2: m3u8 by streaming (it will be saved as
									// .mp4)
									int m3u8TarGzOrM3u8Streaming = 0;
									if (mediaFileFormat == "m3u8-tar.gz")
										m3u8TarGzOrM3u8Streaming = 1;
									else if (mediaFileFormat == "m3u8-streaming")
										m3u8TarGzOrM3u8Streaming = 2;

									if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress)
									{
										/* 2021-02-19: check on threads is
										 *already done in
										 *handleCheckIngestionEvent 2021-06-19:
										 *we still have to check the thread
										 *limit because, in case
										 *handleCheckIngestionEvent gets 20
										 *events, we have still to postpone all
										 *the events overcoming the thread limit
										 */
										int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
										if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
										{
											_logger->warn(
												string() +
												"Not enough available threads "
												"to manage "
												"downloadMediaSourceFileThread,"
												" activity is postponed" +
												", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", "
												"_processorsThreadsNumber.use_"
												"count(): " +
												to_string(_processorsThreadsNumber.use_count()) +
												", _processorThreads + "
												"maxAdditionalProcessorThreads:"
												" " +
												to_string(_processorThreads + maxAdditionalProcessorThreads)
											);

											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);

											// 2021-09-02: regenerateTimestamps
											// is used only
											//	in case of m3u8-streaming
											//	(see
											// docs/TASK_01_Add_Content_JSON_Format.txt)
											bool regenerateTimestamps = false;
											if (mediaFileFormat == "m3u8-streaming")
												regenerateTimestamps = JSONUtils::asBool(parametersRoot, "regenerateTimestamps", false);

											thread downloadMediaSource(
												&MMSEngineProcessor::downloadMediaSourceFileThread, this, _processorsThreadsNumber, mediaSourceURL,
												regenerateTimestamps, m3u8TarGzOrM3u8Streaming, ingestionJobKey, workspace
											);
											downloadMediaSource.detach();
										}
									}
									else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress)
									{
										/* 2021-02-19: check on threads is
										 *already done in
										 *handleCheckIngestionEvent 2021-06-19:
										 *we still have to check the thread
										 *limit because, in case
										 *handleCheckIngestionEvent gets 20
										 *events, we have still to postpone all
										 *the events overcoming the thread limit
										 */
										int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
										if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
										{
											_logger->warn(
												string() +
												"Not enough available threads "
												"to manage "
												"moveMediaSourceFileThread, "
												"activity is postponed" +
												", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", "
												"_processorsThreadsNumber.use_"
												"count(): " +
												to_string(_processorsThreadsNumber.use_count()) +
												", _processorThreads + "
												"maxAdditionalProcessorThreads:"
												" " +
												to_string(_processorThreads + maxAdditionalProcessorThreads)
											);

											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);

											thread moveMediaSource(
												&MMSEngineProcessor::moveMediaSourceFileThread, this, _processorsThreadsNumber, mediaSourceURL,
												m3u8TarGzOrM3u8Streaming, ingestionJobKey, workspace
											);
											moveMediaSource.detach();
										}
									}
									else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress)
									{
										/* 2021-02-19: check on threads is
										 *already done in
										 *handleCheckIngestionEvent 2021-06-19:
										 *we still have to check the thread
										 *limit because, in case
										 *handleCheckIngestionEvent gets 20
										 *events, we have still to postpone all
										 *the events overcoming the thread limit
										 */
										int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
										if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
										{
											_logger->warn(
												string() +
												"Not enough available threads "
												"to manage "
												"copyMediaSourceFileThread, "
												"activity is postponed" +
												", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", "
												"_processorsThreadsNumber.use_"
												"count(): " +
												to_string(_processorsThreadsNumber.use_count()) +
												", _processorThreads + "
												"maxAdditionalProcessorThreads:"
												" " +
												to_string(_processorThreads + maxAdditionalProcessorThreads)
											);

											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);

											thread copyMediaSource(
												&MMSEngineProcessor::copyMediaSourceFileThread, this, _processorsThreadsNumber, mediaSourceURL,
												m3u8TarGzOrM3u8Streaming, ingestionJobKey, workspace
											);
											copyMediaSource.detach();
										}
									}
									else // if (nextIngestionStatus ==
										 // MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
									{
										string errorMessage = "";
										string processorMMS = "";

										SPDLOG_INFO(
											string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											", ingestionJobKey: " + to_string(ingestionJobKey) +
											", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
											", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
										);
										_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);
									}
								}
							}
							catch (exception &e)
							{
								string errorMessage = string("Downloading media source or update "
															 "Ingestion job failed") +
													  ", _processorIdentifier: " + to_string(_processorIdentifier) +
													  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what();
								SPDLOG_ERROR(string() + errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::RemoveContent)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/*
								removeContentTask(
										ingestionJobKey,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage removeContentThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread removeContentThread(
										&MMSEngineProcessor::removeContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									removeContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FTPDelivery)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/*
								ftpDeliveryContentTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage ftpDeliveryContentThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread ftpDeliveryContentThread(
										&MMSEngineProcessor::ftpDeliveryContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									ftpDeliveryContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "ftpDeliveryContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "ftpDeliveryContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LocalCopy)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								if (!_localCopyTaskEnabled)
								{
									string errorMessage = string("Local-Copy Task is not enabled "
																 "in this MMS deploy") +
														  ", _processorIdentifier: " + to_string(_processorIdentifier) +
														  ", ingestionJobKey: " + to_string(ingestionJobKey);
									SPDLOG_ERROR(string() + errorMessage);

									throw runtime_error(errorMessage);
								}

								/*
								// threads check is done inside
								localCopyContentTask localCopyContentTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage localCopyContent, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread localCopyContentThread(
										&MMSEngineProcessor::localCopyContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									localCopyContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::HTTPCallback)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/*
								// threads check is done inside httpCallbackTask
								httpCallbackTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage http callback, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread httpCallbackThread(
										&MMSEngineProcessor::httpCallbackThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									httpCallbackThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "httpCallbackThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "httpCallbackThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
						{
							try
							{
								manageEncodeTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageEncodeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageEncodeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::VideoSpeed)
						{
							try
							{
								manageVideoSpeedTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::PictureInPicture)
						{
							try
							{
								managePictureInPictureTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::IntroOutroOverlay)
						{
							try
							{
								manageIntroOutroOverlayTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::AddSilentAudio)
						{
							try
							{
								manageAddSilentAudioTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageAddSilentAudioTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageAddSilentAudioTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Frame ||
								 ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
								 ingestionType == MMSEngineDBFacade::IngestionType::IFrames ||
								 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
								 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
									ingestionType == MMSEngineDBFacade::IngestionType::IFrames ||
									ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
									ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
								{
									// adds an encoding job
									manageGenerateFramesTask(ingestionJobKey, workspace, ingestionType, parametersRoot, dependencies);
								}
								else // Frame
								{
									/* 2021-02-19: check on threads is already
									 *done in handleCheckIngestionEvent
									 * 2021-06-19: we still have to check the
									 *thread limit because, in case
									 *handleCheckIngestionEvent gets 20 events,
									 *		we have still to postpone all the
									 *events overcoming the thread limit
									 */
									int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
									if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
									{
										_logger->warn(
											string() +
											"Not enough available threads to "
											"manage changeFileFormatThread, "
											"activity is postponed" +
											", _processorIdentifier: " + to_string(_processorIdentifier) +
											", ingestionJobKey: " + to_string(ingestionJobKey) +
											", "
											"_processorsThreadsNumber.use_"
											"count(): " +
											to_string(_processorsThreadsNumber.use_count()) +
											", _processorThreads + "
											"maxAdditionalProcessorThreads: " +
											to_string(_processorThreads + maxAdditionalProcessorThreads)
										);

										string errorMessage = "";
										string processorMMS = "";

										SPDLOG_INFO(
											string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											", ingestionJobKey: " + to_string(ingestionJobKey) +
											", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
											", processorMMS: " + processorMMS
										);
										_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
									}
									else
									{
										thread generateAndIngestFrameThread(
											&MMSEngineProcessor::generateAndIngestFrameThread, this, _processorsThreadsNumber, ingestionJobKey,
											workspace, ingestionType, parametersRoot,
											// it cannot be passed as reference
											// because it will change soon by
											// the parent thread
											dependencies
										);
										generateAndIngestFrameThread.detach();
										/*
										generateAndIngestFramesTask(
											ingestionJobKey,
											workspace,
											ingestionType,
											parametersRoot,
											dependencies);
										*/
									}
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "generateAndIngestFramesTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "generateAndIngestFramesTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Slideshow)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageSlideShowTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::ConcatDemuxer)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage manageConcatThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread manageConcatThread(
										&MMSEngineProcessor::manageConcatThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,

										// it cannot be passed as reference
										// because it will change soon by the
										// parent thread
										dependencies
									);
									manageConcatThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Cut)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage manageCutMediaThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread manageCutMediaThread(
										&MMSEngineProcessor::manageCutMediaThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									manageCutMediaThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							/*
							try
							{
								generateAndIngestCutMediaTask(
										ingestionJobKey,
										workspace,
										parametersRoot,
										dependencies);
							}
							catch(runtime_error& e)
							{
								SPDLOG_ERROR(string() +
							"generateAndIngestCutMediaTask failed"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(string() + "Update
							IngestionJob"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
									+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
									+ ", IngestionStatus: " +
							"End_IngestionFailure"
									+ ", errorMessage: " + errorMessage
									+ ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob
							(ingestionJobKey,
										MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
										errorMessage
										);
								}
								catch(runtime_error& re)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + re.what()
									);
								}
								catch(exception& ex)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch(exception& e)
							{
								SPDLOG_ERROR(string() +
							"generateAndIngestCutMediaTask failed"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(string() + "Update
							IngestionJob"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
									+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
									+ ", IngestionStatus: " +
							"End_IngestionFailure"
									+ ", errorMessage: " + errorMessage
									+ ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob
							(ingestionJobKey,
										MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
										errorMessage
										);
								}
								catch(runtime_error& re)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + re.what()
									);
								}
								catch(exception& ex)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							*/
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::ExtractTracks)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage extractTracksContentThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread extractTracksContentThread(
										&MMSEngineProcessor::extractTracksContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									extractTracksContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "extractTracksContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "extractTracksContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayImageOnVideo)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageOverlayImageOnVideoTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayTextOnVideo)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageOverlayTextOnVideoTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::EmailNotification)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage email notification, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread emailNotificationThread(
										&MMSEngineProcessor::emailNotificationThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									emailNotificationThread.detach();
								}
								/*
								manageEmailNotificationTask(
										ingestionJobKey,
										workspace,
										parametersRoot,
										dependencies);
								*/
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "emailNotificationThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "emailNotificationThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::CheckStreaming)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage check streaming, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread checkStreamingThread(
										&MMSEngineProcessor::checkStreamingThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot
									);
									checkStreamingThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "checkStreamingThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "checkStreamingThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::MediaCrossReference)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageMediaCrossReferenceTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnFacebook)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage post on facebook, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread postOnFacebookThread(
										&MMSEngineProcessor::postOnFacebookThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									postOnFacebookThread.detach();
								}
								/*
								postOnFacebookTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "postOnFacebookThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "postOnFacebookThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnYouTube)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage post on youtube, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread postOnYouTubeThread(
										&MMSEngineProcessor::postOnYouTubeThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									postOnYouTubeThread.detach();
								}
								/*
								postOnYouTubeTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FaceRecognition)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageFaceRecognitionMediaTask(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FaceIdentification)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageFaceIdentificationMediaTask(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceIdentificationMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceIdentificationMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
						{
							try
							{
								manageLiveRecorder(ingestionJobKey, ingestionJobLabel, ingestionStatus, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveRecorder failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveRecorder failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy)
						{
							try
							{
								manageLiveProxy(ingestionJobKey, ingestionStatus, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::VODProxy)
						{
							try
							{
								manageVODProxy(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Countdown)
						{
							try
							{
								manageCountdown(ingestionJobKey, ingestionStatus, ingestionDate, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveGrid)
						{
							try
							{
								manageLiveGrid(ingestionJobKey, ingestionStatus, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveGrid failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveGrid failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveCut)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage manageLiveCutThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									string segmenterType = "hlsSegmenter";
									// string segmenterType = "streamSegmenter";
									if (segmenterType == "hlsSegmenter")
									{
										thread manageLiveCutThread(
											&MMSEngineProcessor::manageLiveCutThread_hlsSegmenter, this, _processorsThreadsNumber, ingestionJobKey,
											ingestionJobLabel, workspace, parametersRoot
										);
										manageLiveCutThread.detach();
									}
									else
									{
										thread manageLiveCutThread(
											&MMSEngineProcessor::manageLiveCutThread_streamSegmenter, this, _processorsThreadsNumber, ingestionJobKey,
											workspace, parametersRoot
										);
										manageLiveCutThread.detach();
									}
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::YouTubeLiveBroadcast)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 * in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread
								 *limit because, in case
								 *handleCheckIngestionEvent gets 20 events, we
								 *have still to postpone all the events
								 *overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage YouTubeLiveBroadcast, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread youTubeLiveBroadcastThread(
										&MMSEngineProcessor::youTubeLiveBroadcastThread, this, _processorsThreadsNumber, ingestionJobKey,
										ingestionJobLabel, workspace, parametersRoot
									);
									youTubeLiveBroadcastThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FacebookLiveBroadcast)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 * in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread
								 *limit because, in case
								 *handleCheckIngestionEvent gets 20 events, we
								 *have still to postpone all the events
								 *overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage facebookLiveBroadcastThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread facebookLiveBroadcastThread(
										&MMSEngineProcessor::facebookLiveBroadcastThread, this, _processorsThreadsNumber, ingestionJobKey,
										ingestionJobLabel, workspace, parametersRoot
									);
									facebookLiveBroadcastThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::ChangeFileFormat)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage changeFileFormatThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread changeFileFormatThread(
										&MMSEngineProcessor::changeFileFormatThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									changeFileFormatThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "changeFileFormatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "changeFileFormatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else
						{
							string errorMessage = string("Unknown IngestionType") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) +
												  ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
							SPDLOG_ERROR(string() + errorMessage);

							SPDLOG_INFO(
								string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
								", errorMessage: " + errorMessage + ", processorMMS: " + ""
							);
							try
							{
								_mmsEngineDBFacade->updateIngestionJob(
									ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
								);
							}
							catch (runtime_error &re)
							{
								SPDLOG_INFO(
									string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + re.what()
								);
							}
							catch (exception &ex)
							{
								SPDLOG_INFO(
									string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + ex.what()
								);
							}

							throw runtime_error(errorMessage);
						}
					}
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "Exception managing the Ingestion entry" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "Exception managing the Ingestion entry" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
				);
			}
		}

		if (ingestionsToBeManaged.size() >= _maxIngestionJobsPerEvent)
		{
			shared_ptr<Event2> event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTIONEVENT);

			event->setSource(MMSENGINEPROCESSORNAME);
			event->setDestination(MMSENGINEPROCESSORNAME);
			event->setExpirationTimePoint(chrono::system_clock::now());

			_multiEventsSet->addEvent(event);

			SPDLOG_DEBUG(
				string() + "addEvent: EVENT_TYPE" + ", MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION" + ", getEventKey().first: " +
				to_string(event->getEventKey().first) + ", getEventKey().second: " + to_string(event->getEventKey().second)
			);
		}
	}
	catch (...)
	{
		SPDLOG_ERROR(string() + "handleCheckIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier));
	}
}

void MMSEngineProcessor::handleLocalAssetIngestionEventThread(
	shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent
)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleLocalAssetIngestionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		localAssetIngestionEvent.getIngestionJobKey()
	);

	try
	{
		// 2023-11-23: inizialmente handleLocalAssetIngestionEvent era inclusa
		// in handleLocalAssetIngestionEventThread. Poi le funzioni sono state
		// divise perche handleLocalAssetIngestionEvent viene chiamata da
		// diversi threads e quindi non poteva istanziare ThreadStatistic in
		// quanto si sarebbe utilizzato lo stesso threadId per due istanze di
		// ThreadStatistic e avremmo avuto errore quando, all'interno di
		// ThreadStatistic, si sarebbe cercato di inserire il threadId nella
		// mappa
		handleLocalAssetIngestionEvent(processorsThreadsNumber, localAssetIngestionEvent);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", localAssetIngestionEvent.getMetadataContent(): " + localAssetIngestionEvent.getMetadataContent() + ", exception: " + e.what()
		);

		// throw e;
		return; // return because it is a thread
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		// throw e;
		return; // return because it is a thread
	}
}

void MMSEngineProcessor::handleLocalAssetIngestionEvent(shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent)
{

	SPDLOG_INFO(
		string() + "handleLocalAssetIngestionEvent" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", ingestionSourceFileName: " +
		localAssetIngestionEvent.getIngestionSourceFileName() + ", metadataContent: " + localAssetIngestionEvent.getMetadataContent() +
		", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
	);

	json parametersRoot;
	try
	{
		string sMetadataContent = localAssetIngestionEvent.getMetadataContent();

		// LF and CR create problems to the json parser...
		while (sMetadataContent.size() > 0 && (sMetadataContent.back() == 10 || sMetadataContent.back() == 13))
			sMetadataContent.pop_back();

		parametersRoot = JSONUtils::toJson(sMetadataContent);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "parsing parameters failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", localAssetIngestionEvent.getMetadataContent(): " + localAssetIngestionEvent.getMetadataContent() + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}

	fs::path binaryPathName;
	string externalStorageRelativePathName;
	try
	{
		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			string workspaceIngestionBinaryPathName;

			workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(localAssetIngestionEvent.getWorkspace());
			workspaceIngestionBinaryPathName.append("/").append(localAssetIngestionEvent.getIngestionSourceFileName());

			string field = "fileFormat";
			string fileFormat = JSONUtils::asString(parametersRoot, field, "");
			if (fileFormat == "m3u8-streaming")
			{
				// .mp4 is used in
				// 1. downloadMediaSourceFileThread (when the m3u8-streaming is
				// downloaded in a .mp4 file
				// 2. here, handleLocalAssetIngestionEvent (when the
				// IngestionRepository file name
				//		is built "consistent" with the above step no. 1)
				// 3. handleLocalAssetIngestionEvent (when the MMS file name is
				// generated)
				binaryPathName = workspaceIngestionBinaryPathName + ".mp4";
			}
			else if (fileFormat == "m3u8-tar.gz")
			{
				// 2023-03-19: come specificato in
				// TASK_01_Add_Content_JSON_Format.txt, in caso di
				//	fileFormat == "m3u8-tar.gz" ci sono due opzioni:
				//	1. in case of copy:// or move:// sourceURL, the tar.gz file
				// name will be the same name 		of the internal directory.
				// In questo caso
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments è stato
				// già chiamato dai metodi
				// MMSEngineProcessor::moveMediaSourceFileThread 		e
				// MMSEngineProcessor::copyMediaSourceFileThread. Era importante
				// che i due precedenti 		metodi chiamassero
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments perchè
				// solo 		loro sapevano il nome del file .tar.gz e quindi
				// il nome della directory contenuta 		nel file .tar.gz.
				// In questo scenario quindi,
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments è stato
				// già 		chiamato e workspaceIngestionBinaryPathName è la
				// directory <ingestionJobKey>_source
				//	2. in caso di <download> o <upload tramite PUSH>, come
				// indicato 		in TASK_01_Add_Content_JSON_Format.txt, il
				// .tar.gz conterrà una directory dal nome
				// "content". 		2.1 In caso di <download>, il metodo
				// MMSEngineProcessor::downloadMediaSourceFileThread
				// chiama lui stesso
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments, per
				// cui, anche 			in questo caso,
				// workspaceIngestionBinaryPathName è la directory
				//<ingestionJobKey>_source 		2.2 In caso di <upload tramite
				// PUSH>, abbiamo evitato che API::uploadedBinary
				// chiamasse
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments perchè
				// manageTar... 			potrebbe impiegare anche parecchi
				// minuti e l'API non puo' rimanere appesa 			per diversi
				// minuti, avremmo timeout del load balancer e/o dei clients in
				// generale. 			Quindi, solo per questo scenario,
				// chiamiamo qui il metodo
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments
				// Possiamo distinguere questo caso dagli altri perchè, non
				// essendo stato chiamato 			il metodo
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments, avremmo
				// il file 			<ingestionJobKey>_source.tar.gz e non, come
				// nei casi precedenti, 			la directory
				// <ingestionJobKey>_source.

				// i.e.:
				// /var/catramms/storage/IngestionRepository/users/8/2848783_source.tar.gz
				string localWorkspaceIngestionBinaryPathName = workspaceIngestionBinaryPathName + ".tar.gz";
				if (fs::exists(localWorkspaceIngestionBinaryPathName) && fs::is_regular_file(localWorkspaceIngestionBinaryPathName))
				{
					// caso 2.2 sopra
					try
					{
						string localSourceBinaryPathFile = "/content.tar.gz";

						_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
							localAssetIngestionEvent.getIngestionJobKey(), localWorkspaceIngestionBinaryPathName,
							_mmsStorage->getWorkspaceIngestionRepository(localAssetIngestionEvent.getWorkspace()), localSourceBinaryPathFile
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments "
													 "failed") +
											  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
											  ", localWorkspaceIngestionBinaryPathName: " + localWorkspaceIngestionBinaryPathName;
						SPDLOG_ERROR(string() + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				// i.e.:
				// /var/catramms/storage/IngestionRepository/users/8/2848783_source
				binaryPathName = workspaceIngestionBinaryPathName;
			}
			else
				binaryPathName = workspaceIngestionBinaryPathName;
		}
		else
		{
			string mediaSourceURL = localAssetIngestionEvent.getExternalStorageMediaSourceURL();

			string externalStoragePrefix("externalStorage://");
			if (!(mediaSourceURL.size() >= externalStoragePrefix.size() &&
				  0 == mediaSourceURL.compare(0, externalStoragePrefix.size(), externalStoragePrefix)))
			{
				string errorMessage =
					string("mediaSourceURL is not an externalStorage reference") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mediaSourceURL: " + mediaSourceURL;

				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
			externalStorageRelativePathName = mediaSourceURL.substr(externalStoragePrefix.length());
			binaryPathName = _mmsStorage->getMMSRootRepository() / ("ExternalStorage_" + localAssetIngestionEvent.getWorkspace()->_directoryName);
			binaryPathName /= externalStorageRelativePathName;
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "binaryPathName initialization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "binaryPathName initialization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}

	SPDLOG_INFO(
		string() + "binaryPathName" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
	);

	string metadataFileContent;
	Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
	try
	{
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies;

		dependencies = validator.validateSingleTaskMetadata(
			localAssetIngestionEvent.getWorkspace()->_workspaceKey, localAssetIngestionEvent.getIngestionType(), parametersRoot
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", localAssetIngestionEvent.getMetadataContent(): " + localAssetIngestionEvent.getMetadataContent() + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
	string mediaFileFormat;
	string md5FileCheckSum;
	int fileSizeInBytes;
	bool externalReadOnlyStorage;
	try
	{
		string mediaSourceURL;

		tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool> mediaSourceDetails = getMediaSourceDetails(
			localAssetIngestionEvent.getIngestionJobKey(), localAssetIngestionEvent.getWorkspace(), localAssetIngestionEvent.getIngestionType(),
			parametersRoot
		);

		tie(nextIngestionStatus, mediaSourceURL, mediaFileFormat, md5FileCheckSum, fileSizeInBytes, externalReadOnlyStorage) = mediaSourceDetails;

		// in case of youtube url, the real URL to be used has to be calcolated
		// Here the mediaFileFormat is retrieved
		{
			string youTubePrefix1("https://www.youtube.com/");
			string youTubePrefix2("https://youtu.be/");
			if ((mediaSourceURL.size() >= youTubePrefix1.size() && 0 == mediaSourceURL.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
				(mediaSourceURL.size() >= youTubePrefix2.size() && 0 == mediaSourceURL.compare(0, youTubePrefix2.size(), youTubePrefix2)))
			{
				FFMpeg ffmpeg(_configurationRoot, _logger);
				pair<string, string> streamingURLDetails =
					ffmpeg.retrieveStreamingYouTubeURL(localAssetIngestionEvent.getIngestionJobKey(), mediaSourceURL);

				tie(ignore, mediaFileFormat) = streamingURLDetails;

				SPDLOG_INFO(
					string() + "Retrieve streaming YouTube URL" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", initial YouTube URL: " + mediaSourceURL
				);
			}
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	try
	{
		validateMediaSourceFile(
			localAssetIngestionEvent.getIngestionJobKey(), binaryPathName.string(), mediaFileFormat, md5FileCheckSum, fileSizeInBytes
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "validateMediaSourceFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "validateMediaSourceFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	string mediaSourceFileName;
	string mmsAssetPathName;
	string relativePathToBeUsed;
	long mmsPartitionUsed;
	try
	{
		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			mediaSourceFileName = localAssetIngestionEvent.getMMSSourceFileName();
			if (mediaSourceFileName == "")
			{
				mediaSourceFileName = localAssetIngestionEvent.getIngestionSourceFileName();
				// .mp4 is used in
				// 1. downloadMediaSourceFileThread (when the m3u8-streaming is
				// downloaded in a .mp4 file
				// 2. handleLocalAssetIngestionEvent (when the
				// IngestionRepository file name
				//		is built "consistent" with the above step no. 1)
				// 3. here, handleLocalAssetIngestionEvent (when the MMS file
				// name is generated)
				if (mediaFileFormat == "m3u8-streaming")
					mediaSourceFileName += ".mp4";
				else if (mediaFileFormat == "m3u8-tar.gz")
					; // mediaSourceFileName is like "2131450_source"
				else
					mediaSourceFileName += ("." + mediaFileFormat);
			}

			relativePathToBeUsed = _mmsEngineDBFacade->nextRelativePathToBeUsed(localAssetIngestionEvent.getWorkspace()->_workspaceKey);

			bool isDirectory = fs::is_directory(binaryPathName);

			unsigned long mmsPartitionIndexUsed;
			bool deliveryRepositoriesToo = true;
			mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
				localAssetIngestionEvent.getIngestionJobKey(), binaryPathName, localAssetIngestionEvent.getWorkspace()->_directoryName,
				mediaSourceFileName, relativePathToBeUsed, &mmsPartitionIndexUsed,
				// &sourceFileType,
				deliveryRepositoriesToo, localAssetIngestionEvent.getWorkspace()->_territories
			);
			mmsPartitionUsed = mmsPartitionIndexUsed;

			// if (mediaFileFormat == "m3u8")
			if (isDirectory)
				relativePathToBeUsed += (mediaSourceFileName + "/");
		}
		else
		{
			mmsAssetPathName = binaryPathName.string();
			mmsPartitionUsed = -1;

			size_t fileNameIndex = externalStorageRelativePathName.find_last_of("/");
			if (fileNameIndex == string::npos)
			{
				string errorMessage = string() + "No fileName found in externalStorageRelativePathName" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
									  ", externalStorageRelativePathName: " + externalStorageRelativePathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			relativePathToBeUsed = externalStorageRelativePathName.substr(0, fileNameIndex + 1);
			mediaSourceFileName = externalStorageRelativePathName.substr(fileNameIndex + 1);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	string m3u8FileName;
	if (mediaFileFormat == "m3u8-tar.gz")
	{
		// in this case mmsAssetPathName refers a directory and we need to find
		// out the m3u8 file name

		try
		{
			for (fs::directory_entry const &entry : fs::directory_iterator(mmsAssetPathName))
			{
				try
				{
					if (!entry.is_regular_file())
						continue;

					// string m3u8Suffix(".m3u8");
					// if (entry.path().filename().string().size() >= m3u8Suffix.size() &&
					// 	0 == entry.path().filename().string().compare(
						// 		 entry.path().filename().string().size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix
							//  ))
					if (StringUtils::endWith(entry.path().filename().string(), ".m3u8"))
					{
						m3u8FileName = entry.path().filename().string();

						break;
					}
				}
				catch (runtime_error &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
				catch (exception &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
			}

			if (m3u8FileName == "")
			{
				string errorMessage = string() + "m3u8 file not found" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
									  ", mmsAssetPathName: " + mmsAssetPathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			mediaSourceFileName = m3u8FileName;
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "retrieving m3u8 file failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove directory" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "retrieving m3u8 file failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
	}

	MMSEngineDBFacade::ContentType contentType;

	tuple<int64_t, long, json> mediaInfoDetails;
	vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
	vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;

	int imageWidth = -1;
	int imageHeight = -1;
	string imageFormat;
	int imageQuality = -1;
	if (validator.isVideoAudioFileFormat(mediaFileFormat))
	{
		try
		{
			FFMpeg ffmpeg(_configurationRoot, _logger);
			// tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
			// mediaInfo;

			int timeoutInSeconds = 20;
			bool isMMSAssetPathName = true;
			if (mediaFileFormat == "m3u8-tar.gz")
				mediaInfoDetails = ffmpeg.getMediaInfo(
					localAssetIngestionEvent.getIngestionJobKey(), isMMSAssetPathName, timeoutInSeconds, mmsAssetPathName + "/" + m3u8FileName,
					videoTracks, audioTracks
				);
			else
				mediaInfoDetails = ffmpeg.getMediaInfo(
					localAssetIngestionEvent.getIngestionJobKey(), isMMSAssetPathName, timeoutInSeconds, mmsAssetPathName, videoTracks, audioTracks
				);

			int64_t durationInMilliSeconds = -1;
			long bitRate = -1;
			tie(durationInMilliSeconds, bitRate, ignore) = mediaInfoDetails;

			SPDLOG_INFO(
				string() + "ffmpeg.getMediaInfo" + ", mmsAssetPathName: " + mmsAssetPathName +
				", durationInMilliSeconds: " + to_string(durationInMilliSeconds) + ", bitRate: " + to_string(bitRate) +
				", videoTracks.size: " + to_string(videoTracks.size()) + ", audioTracks.size: " + to_string(audioTracks.size())
			);

			/*
			tie(durationInMilliSeconds, bitRate,
				videoCodecName, videoProfile, videoWidth, videoHeight,
			videoAvgFrameRate, videoBitRate, audioCodecName, audioSampleRate,
			audioChannels, audioBitRate) = mediaInfo;
			*/

			/*
			 * 2019-10-13: commented because I guess the avg frame rate returned
			by ffmpeg is OK
			 * avg frame rate format is: total duration / total # of frames
			if (localAssetIngestionEvent.getForcedAvgFrameRate() != "")
			{
				SPDLOG_INFO(string() + "handleLocalAssetIngestionEvent.
			Forced Avg Frame Rate"
					+ ", current avgFrameRate: " + videoAvgFrameRate
					+ ", forced avgFrameRate: " +
			localAssetIngestionEvent.getForcedAvgFrameRate()
				);

				videoAvgFrameRate =
			localAssetIngestionEvent.getForcedAvgFrameRate();
			}
			*/

			if (videoTracks.size() == 0)
				contentType = MMSEngineDBFacade::ContentType::Audio;
			else
				contentType = MMSEngineDBFacade::ContentType::Video;
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "EncoderVideoAudioProxy::getMediaInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);
					fs::remove_all(mmsAssetPathName);
					/*
					size_t fileNameIndex = mmsAssetPathName.find_last_of("/");
					if (fileNameIndex == string::npos)
					{
						string errorMessage = string() + "No fileName found
					in mmsAssetPathName"
							+ ", _processorIdentifier: " +
					to_string(_processorIdentifier)
							+ ", ingestionJobKey: " +
					to_string(localAssetIngestionEvent.getIngestionJobKey())
							+ ", mmsAssetPathName: " + mmsAssetPathName
						;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourcePathName = mmsAssetPathName;
					string destBinaryPathName =
					"/var/catramms/storage/MMSWorkingAreaRepository/Staging" +
					mmsAssetPathName.substr(fileNameIndex);
					SPDLOG_INFO(string() + "Moving"
						+ ", _processorIdentifier: " +
					to_string(_processorIdentifier)
						+ ", ingestionJobKey: " +
					to_string(localAssetIngestionEvent.getIngestionJobKey())
						+ ", sourcePathName: " + sourcePathName
						+ ", destBinaryPathName: " + destBinaryPathName
					);
					int64_t elapsedInSeconds =
					MMSStorage::move(localAssetIngestionEvent.getIngestionJobKey(),
					sourcePathName, destBinaryPathName, _logger);
					*/
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() +
				"EncoderVideoAudioProxy::getVideoOrAudioDurationInMilliSeconds "
				"failed" +
				", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
	}
	else if (validator.isImageFileFormat(mediaFileFormat))
	{
		try
		{
			SPDLOG_INFO(
				string() + "Processing through Magick" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
			);
			Magick::Image imageToEncode;

			imageToEncode.read(mmsAssetPathName.c_str());

			imageWidth = imageToEncode.columns();
			imageHeight = imageToEncode.rows();
			imageFormat = imageToEncode.magick();
			imageQuality = imageToEncode.quality();

			contentType = MMSEngineDBFacade::ContentType::Image;
		}
		catch (Magick::WarningCoder &e)
		{
			// Process coder warning while loading file (e.g. TIFF warning)
			// Maybe the user will be interested in these warnings (or not).
			// If a warning is produced while loading an image, the image
			// can normally still be used (but not if the warning was about
			// something important!)
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (Magick::Warning &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (Magick::ErrorFileOpen &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (Magick::Error &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
	}
	else
	{
		string errorMessage = string("Unknown mediaFileFormat") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
							  ", mmsAssetPathName: " + mmsAssetPathName;

		SPDLOG_ERROR(string() + errorMessage);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
			", errorMessage: " + errorMessage
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw runtime_error(errorMessage);
	}

	// int64_t mediaItemKey;
	try
	{
		unsigned long long sizeInBytes;
		if (mediaFileFormat == "m3u8-tar.gz")
		{
			sizeInBytes = 0;
			// recursive_directory_iterator, by default, does not follow sym
			// links
			for (fs::directory_entry const &entry : fs::recursive_directory_iterator(mmsAssetPathName))
			{
				if (entry.is_regular_file())
					sizeInBytes += entry.file_size();
			}
		}
		else
			sizeInBytes = fs::file_size(mmsAssetPathName);

		int64_t variantOfMediaItemKey = -1;
		{
			string variantOfMediaItemKeyField = "variantOfMediaItemKey";
			string variantOfUniqueNameField = "variantOfUniqueName";
			string variantOfIngestionJobKeyField = "VariantOfIngestionJobKey";
			if (JSONUtils::isMetadataPresent(parametersRoot, variantOfMediaItemKeyField))
			{
				variantOfMediaItemKey = JSONUtils::asInt64(parametersRoot, variantOfMediaItemKeyField, -1);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, variantOfUniqueNameField))
			{
				bool warningIfMissing = false;

				string variantOfUniqueName = JSONUtils::asString(parametersRoot, variantOfUniqueNameField, "");

				pair<int64_t, MMSEngineDBFacade::ContentType> mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					localAssetIngestionEvent.getWorkspace()->_workspaceKey, variantOfUniqueName, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena
					// aggiunto
					true
				);
				tie(variantOfMediaItemKey, ignore) = mediaItemKeyDetails;
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, variantOfIngestionJobKeyField))
			{
				int64_t variantOfIngestionJobKey = JSONUtils::asInt64(parametersRoot, variantOfIngestionJobKeyField, -1);
				vector<tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType>> mediaItemsDetails;
				bool warningIfMissing = false;

				_mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
					localAssetIngestionEvent.getWorkspace()->_workspaceKey, variantOfIngestionJobKey, -1, mediaItemsDetails, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);

				if (mediaItemsDetails.size() != 1)
				{
					string errorMessage = string("IngestionJob does not refer the correct media "
												 "Items number") +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", variantOfIngestionJobKey: " + to_string(variantOfIngestionJobKey) +
										  ", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
										  ", mediaItemsDetails.size(): " + to_string(mediaItemsDetails.size());
					SPDLOG_ERROR(string() + errorMessage);

					throw runtime_error(errorMessage);
				}

				tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType> mediaItemsDetailsReturn = mediaItemsDetails[0];
				tie(variantOfMediaItemKey, ignore, ignore) = mediaItemsDetailsReturn;
			}
		}

		// 2022-12-30: indipendentemente se si tratta di una variante o di un
		// source,
		//	è possibile indicare encodingProfileKey
		//	Ad esempio: se si esegue un Task OverlayText dove si specifica
		// l'encoding profile, 	il file generato e ingestato in MMS è un source
		// ed ha anche uno specifico profilo.
		int64_t encodingProfileKey = -1;
		{
			string field = "encodingProfileKey";
			encodingProfileKey = JSONUtils::asInt64(parametersRoot, field, -1);
		}

		if (variantOfMediaItemKey == -1)
		{
			SPDLOG_INFO(
				string() + "_mmsEngineDBFacade->saveSourceContentMetadata..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", encodingProfileKey: " + to_string(encodingProfileKey) + ", contentType: " + MMSEngineDBFacade::toString(contentType) +
				", ExternalReadOnlyStorage: " + to_string(localAssetIngestionEvent.getExternalReadOnlyStorage()) +
				", relativePathToBeUsed: " + relativePathToBeUsed + ", mediaSourceFileName: " + mediaSourceFileName +
				", mmsPartitionUsed: " + to_string(mmsPartitionUsed) + ", sizeInBytes: " + to_string(sizeInBytes)

				+ ", videoTracks.size: " + to_string(videoTracks.size()) + ", audioTracks.size: " + to_string(audioTracks.size())

				+ ", imageWidth: " + to_string(imageWidth) + ", imageHeight: " + to_string(imageHeight) + ", imageFormat: " + imageFormat +
				", imageQuality: " + to_string(imageQuality)
			);

			pair<int64_t, int64_t> mediaItemKeyAndPhysicalPathKey = _mmsEngineDBFacade->saveSourceContentMetadata(
				localAssetIngestionEvent.getWorkspace(), localAssetIngestionEvent.getIngestionJobKey(),
				localAssetIngestionEvent.getIngestionRowToBeUpdatedAsSuccess(), contentType, encodingProfileKey, parametersRoot,
				localAssetIngestionEvent.getExternalReadOnlyStorage(), relativePathToBeUsed, mediaSourceFileName, mmsPartitionUsed, sizeInBytes,

				// video-audio
				mediaInfoDetails, videoTracks, audioTracks,

				// image
				imageWidth, imageHeight, imageFormat, imageQuality
			);

			int64_t mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;

			SPDLOG_INFO(
				string() + "Added a new ingested content" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mediaItemKey: " + to_string(mediaItemKeyAndPhysicalPathKey.first) +
				", physicalPathKey: " + to_string(mediaItemKeyAndPhysicalPathKey.second)
			);
		}
		else
		{
			string externalDeliveryTechnology;
			string externalDeliveryURL;
			{
				string field = "externalDeliveryTechnology";
				externalDeliveryTechnology = JSONUtils::asString(parametersRoot, field, "");

				field = "externalDeliveryURL";
				externalDeliveryURL = JSONUtils::asString(parametersRoot, field, "");
			}

			int64_t physicalItemRetentionInMinutes = -1;
			{
				string field = "physicalItemRetention";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string retention = JSONUtils::asString(parametersRoot, field, "1d");
					physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
				}
			}

			int64_t sourceIngestionJobKey = -1;
			// in case of an encoding generated by the External Transcoder,
			// we have to insert into MMS_IngestionJobOutput
			// of the ingestion job
			{
				string field = "userData";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					json userDataRoot = parametersRoot[field];

					field = "mmsData";
					if (JSONUtils::isMetadataPresent(userDataRoot, field))
					{
						json mmsDataRoot = userDataRoot[field];

						if (JSONUtils::isMetadataPresent(mmsDataRoot, "externalTranscoder"))
						{
							field = "ingestionJobKey";
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot, field, -1);
						}
					}
				}
			}

			SPDLOG_INFO(
				string() + "_mmsEngineDBFacade->saveVariantContentMetadata.." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", sourceIngestionJobKey: " + to_string(sourceIngestionJobKey) + ", variantOfMediaItemKey: " + to_string(variantOfMediaItemKey) +
				", ExternalReadOnlyStorage: " + to_string(localAssetIngestionEvent.getExternalReadOnlyStorage()) +
				", externalDeliveryTechnology: " + externalDeliveryTechnology + ", externalDeliveryURL: " + externalDeliveryURL

				+ ", mediaSourceFileName: " + mediaSourceFileName + ", relativePathToBeUsed: " + relativePathToBeUsed + ", mmsPartitionUsed: " +
				to_string(mmsPartitionUsed) + ", sizeInBytes: " + to_string(sizeInBytes) + ", encodingProfileKey: " + to_string(encodingProfileKey) +
				", physicalItemRetentionInMinutes: " + to_string(physicalItemRetentionInMinutes)

				+ ", videoTracks.size: " + to_string(videoTracks.size()) + ", audioTracks.size: " + to_string(audioTracks.size())

				+ ", imageWidth: " + to_string(imageWidth) + ", imageHeight: " + to_string(imageHeight) + ", imageFormat: " + imageFormat +
				", imageQuality: " + to_string(imageQuality)
			);

			int64_t physicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
				localAssetIngestionEvent.getWorkspace()->_workspaceKey, localAssetIngestionEvent.getIngestionJobKey(), sourceIngestionJobKey,
				variantOfMediaItemKey, localAssetIngestionEvent.getExternalReadOnlyStorage(), externalDeliveryTechnology, externalDeliveryURL,

				mediaSourceFileName, relativePathToBeUsed, mmsPartitionUsed, sizeInBytes, encodingProfileKey, physicalItemRetentionInMinutes,

				// video-audio
				mediaInfoDetails, videoTracks, audioTracks,

				// image
				imageWidth, imageHeight, imageFormat, imageQuality
			);
			SPDLOG_INFO(
				string() + "Added a new variant content" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", variantOfMediaItemKey,: " + to_string(variantOfMediaItemKey) + ", physicalPathKey: " + to_string(physicalPathKey)
			);

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
			);
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
				"" // errorMessage
			);
		}
	}
	catch (DeadlockFound &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey failed" + ", _processorIdentifier: " +
			to_string(_processorIdentifier) + ", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what: " + e.what()
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw runtime_error(e.what());
	}
	catch (MediaItemKeyNotFound &e) // getMediaItemDetailsByIngestionJobKey failure
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey failed" + ", _processorIdentifier: " +
			to_string(_processorIdentifier) + ", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what: " + e.what()
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw runtime_error(e.what());
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->saveSourceContentMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what: " + e.what()
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->saveSourceContentMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
}

void MMSEngineProcessor::manageGroupOfTasks(int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot)
{
	try
	{
		vector<pair<int64_t, int64_t>> referencesOutput;

		Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
		// ReferencesOutput tag is always present:
		// 1. because it is already set by the Workflow (by the user)
		// 2. because it is automatically set by API_Ingestion.cpp using the
		// list of Tasks.
		//	This is when it was not found into the Workflow
		validator.fillReferencesOutput(workspace->_workspaceKey, parametersRoot, referencesOutput);

		int64_t liveRecordingIngestionJobKey = -1;
		for (pair<int64_t, int64_t> referenceOutput : referencesOutput)
		{
			/*
			 * 2020-06-08. I saw a scenario where:
			 *	1. MediaItems were coming from a LiveRecorder with high
			 *availability
			 *	2. a media item was present during
			 *validator.fillReferencesOutput
			 *	3. just before the calling of the below statement
			 *_mmsEngineDBFacade->addIngestionJobOutput it was removed (because
			 *it was not validated
			 *	4. _mmsEngineDBFacade->addIngestionJobOutput raised an exception
			 *		Cannot add or update a child row: a foreign key constraint
			 *fails
			 *		(`vedatest`.`MMS_IngestionJobOutput`, CONSTRAINT
			 *`MMS_IngestionJobOutput_FK` FOREIGN KEY (`physicalPathKey`)
			 *REFERENCES `MMS_PhysicalPath` (`physicalPathKey`) ON DELETE
			 *CASCADE) This scenario should never happen because the
			 *EncoderVideoAudioProxy::processLiveRecorder method wait that the
			 *high availability is completely managed.
			 *
			 * Anyway to be sure we will not interrupt our workflow, we will add
			 *a try catch
			 */
			try
			{
				SPDLOG_INFO(
					string() + "References.Output" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", mediaItemKey: " + to_string(referenceOutput.first) + ", physicalPathKey: " + to_string(referenceOutput.second)
				);

				_mmsEngineDBFacade->addIngestionJobOutput(
					ingestionJobKey, referenceOutput.first, referenceOutput.second, liveRecordingIngestionJobKey
				);
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "_mmsEngineDBFacade->addIngestionJobOutput failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(referenceOutput.first) +
					", physicalPathKey: " + to_string(referenceOutput.second) + ", e.what(): " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "_mmsEngineDBFacade->addIngestionJobOutput failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(referenceOutput.first) +
					", physicalPathKey: " + to_string(referenceOutput.second)
				);
			}
		}

		/*
		 * 2019-09-23: It is not clean now how to manage the status of the
		 *GroupOfTasks:
		 *	- depend on the status of his children (first level of Tasks of the
		 *GroupOfTasks) as calculated below (now commented)?
		 *	- depend on the ReferencesOutput?
		 *
		 *	Since this is not clean, I left it always Success
		 *
		 */
		/*
		// GroupOfTasks Ingestion Status is by default Failure;
		// It will be Success if at least just one Status of the children is
		Success MMSEngineDBFacade::IngestionStatus groupOfTasksIngestionStatus
			= MMSEngineDBFacade::IngestionStatus::End_IngestionFailure;
		{
			vector<pair<int64_t, MMSEngineDBFacade::IngestionStatus>>
		groupOfTasksChildrenStatus;

			_mmsEngineDBFacade->getGroupOfTasksChildrenStatus(ingestionJobKey,
		groupOfTasksChildrenStatus);

			for (pair<int64_t, MMSEngineDBFacade::IngestionStatus>
		groupOfTasksChildStatus: groupOfTasksChildrenStatus)
			{
				int64_t childIngestionJobKey = groupOfTasksChildStatus.first;
				MMSEngineDBFacade::IngestionStatus childStatus =
		groupOfTasksChildStatus.second;

				SPDLOG_INFO(string() + "manageGroupOfTasks, child status"
						+ ", group of tasks ingestionJobKey: " +
		to_string(ingestionJobKey)
						+ ", childIngestionJobKey: " +
		to_string(childIngestionJobKey)
						+ ", IngestionStatus: " +
		MMSEngineDBFacade::toString(childStatus)
				);

				if
		(!MMSEngineDBFacade::isIngestionStatusFinalState(childStatus))
				{
					SPDLOG_ERROR(string() + "manageGroupOfTasks, child
		status is not a final status. It should never happens because when this
		GroupOfTasks is executed, all the children should be finished"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " +
		MMSEngineDBFacade::toString(childStatus)
					);

					continue;
				}

				if (childStatus ==
		MMSEngineDBFacade::IngestionStatus::End_TaskSuccess)
				{
					groupOfTasksIngestionStatus =
		MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

					break;
				}
			}
		}
		*/
		MMSEngineDBFacade::IngestionStatus groupOfTasksIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

		string errorMessage = "";
		if (groupOfTasksIngestionStatus != MMSEngineDBFacade::IngestionStatus::End_TaskSuccess)
			errorMessage = "Failed because there is no one child with Status Success";

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", IngestionStatus: " + MMSEngineDBFacade::toString(groupOfTasksIngestionStatus) + ", errorMessage: " + errorMessage
		);
		_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, groupOfTasksIngestionStatus, errorMessage);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::removeContentThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "removeContentThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "removeContentThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be removed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				// check if there are ingestion dependencies on this media item
				{
					if (dependencyType == Validator::DependencyType::MediaItemKey)
					{
						bool warningIfMissing = false;
						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemKeyDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						MMSEngineDBFacade::ContentType localContentType;
						string localTitle;
						string localUserData;
						string localIngestionDate;
						int64_t ingestionJobKeyOfItemToBeRemoved;
						tie(localContentType, localTitle, localUserData, localIngestionDate, ignore, ingestionJobKeyOfItemToBeRemoved) =
							mediaItemKeyDetails;

						int ingestionDependenciesNumber = _mmsEngineDBFacade->getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
							ingestionJobKeyOfItemToBeRemoved,
							// 2022-12-18: importante essere sicuri
							true
						);
						if (ingestionDependenciesNumber > 0)
						{
							string errorMessage = string() +
												  "MediaItem cannot be removed because there are "
												  "still ingestion dependencies" +
												  ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) +
												  ", ingestionDependenciesNumber not finished: " + to_string(ingestionDependenciesNumber) +
												  ", ingestionJobKeyOfItemToBeRemoved: " + to_string(ingestionJobKeyOfItemToBeRemoved);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						int64_t localMediaItemKey;
						MMSEngineDBFacade::ContentType localContentType;
						string localTitle;
						string localUserData;
						string localIngestionDate;
						int64_t ingestionJobKeyOfItemToBeRemoved;
						tie(localMediaItemKey, localContentType, localTitle, localUserData, localIngestionDate, ingestionJobKeyOfItemToBeRemoved,
							ignore, ignore, ignore) = mediaItemDetails;

						int ingestionDependenciesNumber = _mmsEngineDBFacade->getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
							ingestionJobKeyOfItemToBeRemoved,
							// 2022-12-18: importante essere sicuri
							true
						);
						if (ingestionDependenciesNumber > 0)
						{
							string errorMessage = string() +
												  "MediaItem cannot be removed because there are "
												  "still ingestion dependencies" +
												  ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) +
												  ", ingestionDependenciesNumber not finished: " + to_string(ingestionDependenciesNumber) +
												  ", ingestionJobKeyOfItemToBeRemoved: " + to_string(ingestionJobKeyOfItemToBeRemoved);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					SPDLOG_INFO(
						string() + "removeMediaItem" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", mediaItemKey: " + to_string(key)
					);
					_mmsStorage->removeMediaItem(key);
				}
				else
				{
					SPDLOG_INFO(
						string() + "removePhysicalPath" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", physicalPathKey: " + to_string(key)
					);
					_mmsStorage->removePhysicalPath(key);
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "Remove Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "Remove Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::ftpDeliveryContentThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "ftpDeliveryContentThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be uploaded (FTP)" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string configurationLabel;
		{
			string field = "configurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			configurationLabel = JSONUtils::asString(parametersRoot, field, "");
		}

		string ftpServer;
		int ftpPort;
		string ftpUserName;
		string ftpPassword;
		string ftpRemoteDirectory;

		tuple<string, int, string, string, string> ftp = _mmsEngineDBFacade->getFTPByConfigurationLabel(workspace->_workspaceKey, configurationLabel);
		tie(ftpServer, ftpPort, ftpUserName, ftpPassword, ftpRemoteDirectory) = ftp;

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;
				string fileName;
				int64_t sizeInBytes;
				string deliveryFileName;
				int64_t mediaItemKey;
				int64_t physicalPathKey;
				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					mediaItemKey = key;

					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key, encodingProfileKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(physicalPathKey, mmsAssetPathName, ignore, ignore, fileName, sizeInBytes, deliveryFileName) =
						physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;
				}
				else
				{
					physicalPathKey = key;

					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
								_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
									workspace->_workspaceKey, physicalPathKey, warningIfMissing,
									// 2022-12-18: MIK potrebbe essere stato
									// appena aggiunto
									true
								);
						tie(mediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
					}

					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(mmsAssetPathName, ignore, ignore, fileName, sizeInBytes, deliveryFileName) =
						physicalPathFileNameSizeInBytesAndDeliveryFileName;
				}

				ftpUploadMediaSource(
					mmsAssetPathName, fileName, sizeInBytes, ingestionJobKey, workspace, mediaItemKey, physicalPathKey, ftpServer, ftpPort,
					ftpUserName, ftpPassword, ftpRemoteDirectory, deliveryFileName
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "FTP Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "FTP Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "ftpDeliveryContentTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "ftpDeliveryContentTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::localCopyContentThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "localCopyContentThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be copied" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string localPath;
		string localFileName;
		{
			string field = "LocalPath";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			localPath = JSONUtils::asString(parametersRoot, field, "");

			field = "LocalFileName";
			localFileName = JSONUtils::asString(parametersRoot, field, "");
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;
				string fileFormat;
				int64_t physicalPathKey;
				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key, encodingProfileKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) =
						physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;
				}
				else
				{
					physicalPathKey = key;

					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;
				}

				copyContent(ingestionJobKey, mmsAssetPathName, localPath, localFileName);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "local copy Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "local copy Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::extractTracksContentThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "extractTracksContentThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "extractTracksContentThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured media to be used to extract a track" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		vector<pair<string, int>> tracksToBeExtracted;
		string outputFileFormat;
		{
			{
				string field = "Tracks";
				json tracksToot = parametersRoot[field];
				if (tracksToot.size() == 0)
				{
					string errorMessage = string() + "No correct number of Tracks" + ", tracksToot.size: " + to_string(tracksToot.size());
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				for (int trackIndex = 0; trackIndex < tracksToot.size(); trackIndex++)
				{
					json trackRoot = tracksToot[trackIndex];

					field = "TrackType";
					if (!JSONUtils::isMetadataPresent(trackRoot, field))
					{
						string sTrackRoot = JSONUtils::toString(trackRoot);

						string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field + ", sTrackRoot: " + sTrackRoot;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					string trackType = JSONUtils::asString(trackRoot, field, "");

					int trackNumber = 0;
					field = "TrackNumber";
					trackNumber = JSONUtils::asInt(trackRoot, field, 0);

					tracksToBeExtracted.push_back(make_pair(trackType, trackNumber));
				}
			}

			string field = "outputFileFormat";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			outputFileFormat = JSONUtils::asString(parametersRoot, field, "");
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}

				{
					string localSourceFileName;
					string extractTrackMediaPathName;
					{
						localSourceFileName = to_string(ingestionJobKey) + "_" + to_string(key) + "_extractTrack" + "." + outputFileFormat;

						string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
						extractTrackMediaPathName = workspaceIngestionRepository + "/" + localSourceFileName;
					}

					FFMpeg ffmpeg(_configurationRoot, _logger);

					ffmpeg.extractTrackMediaToIngest(ingestionJobKey, mmsAssetPathName, tracksToBeExtracted, extractTrackMediaPathName);

					SPDLOG_INFO(
						string() + "extractTrackMediaToIngest done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", extractTrackMediaPathName: " + extractTrackMediaPathName
					);

					string title;
					int64_t imageOfVideoMediaItemKey = -1;
					int64_t cutOfVideoMediaItemKey = -1;
					int64_t cutOfAudioMediaItemKey = -1;
					double startTimeInSeconds = 0.0;
					double endTimeInSeconds = 0.0;
					string mediaMetaDataContent = generateMediaMetadataToIngest(
						ingestionJobKey, outputFileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey,
						startTimeInSeconds, endTimeInSeconds, parametersRoot
					);

					{
						shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent = make_shared<LocalAssetIngestionEvent>();
						/*
						shared_ptr<LocalAssetIngestionEvent>
						localAssetIngestionEvent =
						_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
								MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);
						*/

						localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

						localAssetIngestionEvent->setExternalReadOnlyStorage(false);
						localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
						localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
						localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
						localAssetIngestionEvent->setWorkspace(workspace);
						localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
						localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(
							/* it + 1 == dependencies.end() ? true : */
							false
						);

						// to manage a ffmpeg bug generating a corrupted/wrong
						// avgFrameRate, we will force the concat file to have
						// the same avgFrameRate of the source media Uncomment
						// next statements in case the problem is still present
						// event in case of the ExtractTracks task if
						// (forcedAvgFrameRate != "" && concatContentType ==
						// MMSEngineDBFacade::ContentType::Video)
						//    localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);

						localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

						handleLocalAssetIngestionEvent(processorsThreadsNumber, *localAssetIngestionEvent);
						/*
						shared_ptr<Event2>    event =
						dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
						_multiEventsSet->addEvent(event);

						SPDLOG_INFO(string() + "addEvent: EVENT_TYPE
						(INGESTASSETEVENT)"
							+ ", _processorIdentifier: " +
						to_string(_processorIdentifier)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", getEventKey().first: " +
						to_string(event->getEventKey().first)
							+ ", getEventKey().second: " +
						to_string(event->getEventKey().second));
						*/
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "extract track failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "extract track failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "Extracting tracks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			string() + "Extracting tracks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}

void MMSEngineProcessor::httpCallbackThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "httpCallbackThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be notified (HTTP Callback)" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		bool addMediaData;
		string httpProtocol;
		string httpHostName;
		string userName;
		string password;
		int httpPort;
		string httpURI;
		string httpURLParameters;
		bool formData;
		string httpMethod;
		long callbackTimeoutInSeconds;
		int maxRetries;
		string httpBody;
		json httpHeadersRoot = json::array();
		{
			string field = "addMediaData";
			addMediaData = JSONUtils::asBool(parametersRoot, field, true);

			field = "protocol";
			httpProtocol = JSONUtils::asString(parametersRoot, field, "http");

			field = "userName";
			userName = JSONUtils::asString(parametersRoot, field, "");

			field = "password";
			password = JSONUtils::asString(parametersRoot, field, "");

			field = "hostName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			httpHostName = JSONUtils::asString(parametersRoot, field, "");

			field = "port";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				if (httpProtocol == "http")
					httpPort = 80;
				else
					httpPort = 443;
			}
			else
				httpPort = JSONUtils::asInt(parametersRoot, field, 0);

			field = "timeout";
			callbackTimeoutInSeconds = JSONUtils::asInt(parametersRoot, field, 120);

			field = "uri";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			httpURI = JSONUtils::asString(parametersRoot, field, "");

			field = "parameters";
			httpURLParameters = JSONUtils::asString(parametersRoot, field, "");

			field = "formData";
			formData = JSONUtils::asBool(parametersRoot, field, false);

			field = "method";
			httpMethod = JSONUtils::asString(parametersRoot, field, "POST");

			field = "httpBody";
			httpBody = JSONUtils::asString(parametersRoot, field, "");

			field = "headers";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				// semicolon as separator
				stringstream ss(JSONUtils::asString(parametersRoot, field, ""));
				string token;
				char delim = ';';
				while (getline(ss, token, delim))
				{
					if (!token.empty())
						httpHeadersRoot.push_back(token);
				}
				// httpHeadersRoot = parametersRoot[field];
			}

			field = "maxRetries";
			maxRetries = JSONUtils::asInt(parametersRoot, field, 1);
		}

		if (addMediaData && (httpMethod == "POST" || httpMethod == "PUT"))
		{
			if (httpBody != "")
			{
				SPDLOG_INFO(
					string() + "POST/PUT with httpBody" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size())
				);

				int dependencyIndex = 0;
				for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
				{
					bool stopIfReferenceProcessingError = false;

					try
					{
						int64_t key;
						Validator::DependencyType dependencyType;

						tie(key, ignore, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

						int64_t physicalPathKey;
						int64_t mediaItemKey;

						if (dependencyType == Validator::DependencyType::MediaItemKey)
						{
							mediaItemKey = key;

							int64_t encodingProfileKey = -1;

							bool warningIfMissing = false;
							tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
								key, encodingProfileKey, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);
							tie(physicalPathKey, ignore, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
						}
						else
						{
							physicalPathKey = key;

							{
								bool warningIfMissing = false;
								tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
									mediaItemDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
										workspace->_workspaceKey, key, warningIfMissing,
										// 2022-12-18: MIK potrebbe
										// essere stato appena aggiunto
										true
									);

								tie(mediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemDetails;
							}
						}

						httpBody = regex_replace(httpBody, regex("\\$\\{mediaItemKey\\}"), to_string(mediaItemKey));
						httpBody = regex_replace(httpBody, regex("\\$\\{physicalPathKey\\}"), to_string(physicalPathKey));

						SPDLOG_INFO(
							string() + "userHttpCallback" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", httpProtocol: " + httpProtocol +
							", httpHostName: " + httpHostName + ", httpURI: " + httpURI + ", httpURLParameters: " + httpURLParameters +
							", formData: " + to_string(formData) + ", httpMethod: " + httpMethod + ", httpBody: " + httpBody
						);

						userHttpCallback(
							ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, httpMethod,
							callbackTimeoutInSeconds, httpHeadersRoot, httpBody, userName, password, maxRetries
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", dependencyIndex: " + to_string(dependencyIndex) +
											  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}
					catch (exception e)
					{
						string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
						+", dependencies.size(): " + to_string(dependencies.size());
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}

					dependencyIndex++;
				}
			}
			else
			{
				int dependencyIndex = 0;
				for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
				{
					bool stopIfReferenceProcessingError = false;

					try
					{
						int64_t key;
						MMSEngineDBFacade::ContentType referenceContentType;
						Validator::DependencyType dependencyType;

						tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

						json callbackMedatada;
						{
							callbackMedatada["workspaceKey"] = (int64_t)(workspace->_workspaceKey);

							MMSEngineDBFacade::ContentType contentType;
							int64_t physicalPathKey;
							int64_t mediaItemKey;

							if (dependencyType == Validator::DependencyType::MediaItemKey)
							{
								mediaItemKey = key;

								callbackMedatada["mediaItemKey"] = mediaItemKey;

								{
									bool warningIfMissing = false;
									tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
										contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
											workspace->_workspaceKey, mediaItemKey, warningIfMissing,
											// 2022-12-18: MIK potrebbe
											// essere stato appena
											// aggiunto
											true
										);

									string localTitle;
									string userData;
									tie(contentType, localTitle, userData, ignore, ignore, ignore) =
										contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;

									callbackMedatada["title"] = localTitle;

									if (userData == "")
										callbackMedatada["userData"] = nullptr;
									else
									{
										json userDataRoot = JSONUtils::toJson(userData);

										callbackMedatada["userData"] = userDataRoot;
									}
								}

								{
									int64_t encodingProfileKey = -1;
									bool warningIfMissing = false;
									tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails =
										_mmsStorage->getPhysicalPathDetails(
											key, encodingProfileKey, warningIfMissing,
											// 2022-12-18: MIK potrebbe
											// essere stato appena aggiunto
											true
										);

									string physicalPath;
									string fileName;
									int64_t sizeInBytes;
									string deliveryFileName;

									tie(physicalPathKey, physicalPath, ignore, ignore, fileName, ignore, ignore) = physicalPathDetails;

									callbackMedatada["physicalPathKey"] = physicalPathKey;
									callbackMedatada["fileName"] = fileName;
									// callbackMedatada["physicalPath"] =
									// physicalPath;
								}
							}
							else
							{
								physicalPathKey = key;

								callbackMedatada["physicalPathKey"] = physicalPathKey;

								{
									bool warningIfMissing = false;
									tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
										mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
											_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
												workspace->_workspaceKey, physicalPathKey, warningIfMissing,
												// 2022-12-18: MIK potrebbe
												// essere stato appena
												// aggiunto
												true
											);

									string localTitle;
									string userData;
									tie(mediaItemKey, contentType, localTitle, userData, ignore, ignore, ignore, ignore, ignore) =
										mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;

									callbackMedatada["mediaItemKey"] = mediaItemKey;
									callbackMedatada["title"] = localTitle;

									if (userData == "")
										callbackMedatada["userData"] = nullptr;
									else
									{
										json userDataRoot = JSONUtils::toJson(userData);

										callbackMedatada["userData"] = userDataRoot;
									}
								}

								{
									int64_t encodingProfileKey = -1;
									tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
										physicalPathKey,
										// 2022-12-18: MIK potrebbe
										// essere stato appena aggiunto
										true
									);

									string physicalPath;
									string fileName;
									int64_t sizeInBytes;
									string deliveryFileName;

									tie(physicalPath, ignore, ignore, fileName, ignore, ignore) = physicalPathDetails;

									callbackMedatada["fileName"] = fileName;
									// callbackMedatada["physicalPath"] =
									// physicalPath;
								}
							}

							if (contentType == MMSEngineDBFacade::ContentType::Video || contentType == MMSEngineDBFacade::ContentType::Audio)
							{
								try
								{
									int64_t durationInMilliSeconds = _mmsEngineDBFacade->getMediaDurationInMilliseconds(
										mediaItemKey, physicalPathKey,
										// 2022-12-18: MIK potrebbe
										// essere stato appena aggiunto
										true
									);

									float durationInSeconds = durationInMilliSeconds / 1000;

									callbackMedatada["durationInSeconds"] = durationInSeconds;
								}
								catch (runtime_error &e)
								{
									SPDLOG_ERROR(
										string() +
										"getMediaDurationInMilliseconds "
										"failed" +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
										", physicalPathKey: " + to_string(physicalPathKey) + ", exception: " + e.what()
									);
								}
								catch (exception &e)
								{
									SPDLOG_ERROR(
										string() +
										"getMediaDurationInMilliseconds "
										"failed" +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
										", physicalPathKey: " + to_string(physicalPathKey)
									);
								}
							}
						}

						string data = JSONUtils::toString(callbackMedatada);

						userHttpCallback(
							ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, httpMethod,
							callbackTimeoutInSeconds, httpHeadersRoot, data, userName, password, maxRetries
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", dependencyIndex: " + to_string(dependencyIndex) +
											  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}
					catch (exception e)
					{
						string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
						+", dependencies.size(): " + to_string(dependencies.size());
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}

					dependencyIndex++;
				}
			}
		}
		else
		{
			try
			{
				userHttpCallback(
					ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, httpMethod, callbackTimeoutInSeconds,
					httpHeadersRoot, httpBody, userName, password, maxRetries
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "httpCallbackTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "httpCallbackTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::postOnFacebookThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "postOnFacebookThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be posted on Facebook" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string facebookConfigurationLabel;
		string facebookNodeType;
		string facebookNodeId;
		{
			string field = "facebookConfigurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookNodeType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeType = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookNodeId";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeId = JSONUtils::asString(parametersRoot, field, "");
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;
				int64_t sizeInBytes;
				MMSEngineDBFacade::ContentType contentType;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathDetails;

					{
						bool warningIfMissing = false;
						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
							contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
					}
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathDetails;

					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
								_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
									workspace->_workspaceKey, key, warningIfMissing,
									// 2022-12-18: MIK potrebbe essere stato
									// appena aggiunto
									true
								);

						tie(ignore, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
					}
				}

				// check on thread availability was done at the beginning in
				// this method
				if (contentType == MMSEngineDBFacade::ContentType::Video)
				{
					postVideoOnFacebook(
						mmsAssetPathName, sizeInBytes, ingestionJobKey, workspace, facebookConfigurationLabel, facebookNodeType, facebookNodeId
					);
				}
				else // if (contentType == ContentType::Audio)
				{
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "post on facebook failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "post on facebook failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "postOnFacebookTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "postOnFacebookTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::postOnYouTubeThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "postOnYouTubeThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be posted on YouTube" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string youTubeConfigurationLabel;
		string youTubeTitle;
		string youTubeDescription;
		json youTubeTags = nullptr;
		int youTubeCategoryId = -1;
		string youTubePrivacyStatus;
		bool youTubeMadeForKids;
		{
			string field = "configurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			youTubeConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "title";
			youTubeTitle = JSONUtils::asString(parametersRoot, field, "");

			field = "description";
			youTubeDescription = JSONUtils::asString(parametersRoot, field, "");

			field = "tags";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				youTubeTags = parametersRoot[field];

			field = "categoryId";
			youTubeCategoryId = JSONUtils::asInt(parametersRoot, field, -1);

			field = "privacyStatus";
			youTubePrivacyStatus = JSONUtils::asString(parametersRoot, field, "private");

			field = "madeForKids";
			youTubeMadeForKids = JSONUtils::asBool(parametersRoot, field, false);
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;
				int64_t sizeInBytes;
				MMSEngineDBFacade::ContentType contentType;
				string title;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathDetails;

					{
						bool warningIfMissing = false;
						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
							contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
					}
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;

					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
								_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
									workspace->_workspaceKey, key, warningIfMissing,
									// 2022-12-18: MIK potrebbe essere stato
									// appena aggiunto
									true
								);

						tie(ignore, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
					}
				}

				if (youTubeTitle == "")
					youTubeTitle = title;

				postVideoOnYouTube(
					mmsAssetPathName, sizeInBytes, ingestionJobKey, workspace, youTubeConfigurationLabel, youTubeTitle, youTubeDescription,
					youTubeTags, youTubeCategoryId, youTubePrivacyStatus, youTubeMadeForKids
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "post on youtube failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "post on youtube failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::changeFileFormatThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)

{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "changeFileFormatThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "changeFileFormatThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured media to be used to changeFileFormat" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string outputFileFormat;
		{
			string field = "outputFileFormat";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			outputFileFormat = JSONUtils::asString(parametersRoot, field, "");
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				int64_t mediaItemKey;
				int64_t physicalPathKey;
				string mmsSourceAssetPathName;
				string relativePath;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					mediaItemKey = key;
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						mediaItemKey, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(physicalPathKey, mmsSourceAssetPathName, ignore, relativePath, ignore, ignore, ignore) = physicalPathDetails;
				}
				else
				{
					physicalPathKey = key;

					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						physicalPathKey,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(mmsSourceAssetPathName, ignore, relativePath, ignore, ignore, ignore) = physicalPathDetails;

					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, physicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato
							// appena aggiunto
							true
						);
					tie(mediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemDetails;
				}

				vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
				vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

				_mmsEngineDBFacade->getVideoDetails(
					mediaItemKey, physicalPathKey,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true, videoTracks, audioTracks
				);

				// add the new file as a new variant of the MIK
				{
					string changeFormatFileName = to_string(ingestionJobKey) + "_" + to_string(mediaItemKey) + "_changeFileFormat";
					if (outputFileFormat == "m3u8-tar.gz" || outputFileFormat == "m3u8-streaming")
						changeFormatFileName += ".m3u8";
					else
						changeFormatFileName += (string(".") + outputFileFormat);

					string stagingChangeFileFormatAssetPathName;
					{
						bool removeLinuxPathIfExist = true;
						bool neededForTranscoder = false;
						stagingChangeFileFormatAssetPathName = _mmsStorage->getStagingAssetPathName(
							neededForTranscoder, workspace->_directoryName, to_string(ingestionJobKey), "/", changeFormatFileName,
							-1, // _encodingItem->_mediaItemKey, not used
								// because encodedFileName is not ""
							-1, // _encodingItem->_physicalPathKey, not used
								// because encodedFileName is not ""
							removeLinuxPathIfExist
						);
					}

					try
					{
						SPDLOG_INFO(
							string() + "Calling ffmpeg.changeFileFormat" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName + ", changeFormatFileName: " + changeFormatFileName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName
						);

						FFMpeg ffmpeg(_configurationRoot, _logger);

						ffmpeg.changeFileFormat(
							ingestionJobKey, physicalPathKey, mmsSourceAssetPathName, videoTracks, audioTracks, stagingChangeFileFormatAssetPathName,
							outputFileFormat
						);

						SPDLOG_INFO(
							string() + "ffmpeg.changeFileFormat done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName + ", changeFormatFileName: " + changeFormatFileName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName
						);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "ffmpeg.changeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "ffmpeg.changeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						throw e;
					}

					tuple<int64_t, long, json> mediaInfoDetails;
					vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
					vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;

					int imageWidth = -1;
					int imageHeight = -1;
					string imageFormat;
					int imageQuality = -1;
					try
					{
						SPDLOG_INFO(
							string() + "Calling ffmpeg.getMediaInfo" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", _ingestionJobKey: " + to_string(ingestionJobKey) +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName
						);
						int timeoutInSeconds = 20;
						bool isMMSAssetPathName = true;
						FFMpeg ffmpeg(_configurationRoot, _logger);
						mediaInfoDetails = ffmpeg.getMediaInfo(
							ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, stagingChangeFileFormatAssetPathName, videoTracks, audioTracks
						);

						// tie(durationInMilliSeconds, bitRate,
						// 	videoCodecName, videoProfile, videoWidth,
						// videoHeight, videoAvgFrameRate, videoBitRate,
						// 	audioCodecName, audioSampleRate, audioChannels,
						// audioBitRate) = mediaInfo;
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "getMediaInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " +
							to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName +
							", _workspace->_directoryName: " + workspace->_directoryName + ", e.what(): " + e.what()
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _ingestionJobKey: " + to_string(ingestionJobKey) +
									", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName +
									", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "getMediaInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " +
							to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName +
							", workspace->_directoryName: " + workspace->_directoryName
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
									stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}

					string mmsChangeFileFormatAssetPathName;
					unsigned long mmsPartitionIndexUsed;
					try
					{
						bool deliveryRepositoriesToo = true;

						mmsChangeFileFormatAssetPathName = _mmsStorage->moveAssetInMMSRepository(
							ingestionJobKey, stagingChangeFileFormatAssetPathName, workspace->_directoryName, changeFormatFileName, relativePath,

							&mmsPartitionIndexUsed, // OUT
							// &sourceFileType,

							deliveryRepositoriesToo, workspace->_territories
						);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
									stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
									stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}

					// remove staging directory
					{
						string directoryPathName;
						try
						{
							size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
							if (endOfDirectoryIndex != string::npos)
							{
								directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

								SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
								fs::remove_all(directoryPathName);
							}
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
								stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
							);
						}
					}

					try
					{
						int64_t physicalItemRetentionInMinutes = -1;
						{
							string field = "physicalItemRetention";
							if (JSONUtils::isMetadataPresent(parametersRoot, field))
							{
								string retention = JSONUtils::asString(parametersRoot, field, "1d");
								physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
							}
						}

						unsigned long long mmsAssetSizeInBytes;
						{
							mmsAssetSizeInBytes = fs::file_size(mmsChangeFileFormatAssetPathName);
						}

						bool externalReadOnlyStorage = false;
						string externalDeliveryTechnology;
						string externalDeliveryURL;
						int64_t liveRecordingIngestionJobKey = -1;
						int64_t changeFormatPhysicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
							workspace->_workspaceKey, ingestionJobKey, liveRecordingIngestionJobKey, mediaItemKey, externalReadOnlyStorage,
							externalDeliveryTechnology, externalDeliveryURL, changeFormatFileName, relativePath, mmsPartitionIndexUsed,
							mmsAssetSizeInBytes,
							-1, // encodingProfileKey,
							physicalItemRetentionInMinutes,

							mediaInfoDetails, videoTracks, audioTracks,

							imageWidth, imageHeight, imageFormat, imageQuality
						);

						SPDLOG_INFO(
							string() + "Saved the Encoded content" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", _ingestionJobKey: " + to_string(ingestionJobKey) +
							", changeFormatPhysicalPathKey: " + to_string(changeFormatPhysicalPathKey)
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() +
							"_mmsEngineDBFacade->saveVariantContentMetadata "
							"failed" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " + to_string(ingestionJobKey)
						);

						if (fs::exists(mmsChangeFileFormatAssetPathName))
						{
							SPDLOG_INFO(
								string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
								to_string(ingestionJobKey) + ", mmsChangeFileFormatAssetPathName: " + mmsChangeFileFormatAssetPathName
							);

							fs::remove_all(mmsChangeFileFormatAssetPathName);
						}

						throw e;
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "change file format failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "change file format failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "ChangeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			string() + "ChangeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}

// this is to generate one Frame
void MMSEngineProcessor::generateAndIngestFrameThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
	MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "generateAndIngestFrameThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "generateAndIngestFrameThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string field;

		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No dependencies found" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
					"" // errorMessage
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
				);
			}

			// throw runtime_error(errorMessage);
			return;
		}

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
				{
					string errorMessage = string() + "ContentTpe is not a Video" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				int64_t sourceMediaItemKey;
				int64_t sourcePhysicalPathKey;
				string sourcePhysicalPath;
				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;
					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(sourcePhysicalPathKey, sourcePhysicalPath, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

					sourceMediaItemKey = key;
				}
				else
				{
					sourcePhysicalPathKey = key;

					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);
					tie(sourceMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;

					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						sourcePhysicalPathKey,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(sourcePhysicalPath, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}

				int periodInSeconds;
				double startTimeInSeconds;
				int maxFramesNumber;
				string videoFilter;
				bool mjpeg;
				int imageWidth;
				int imageHeight;
				int64_t durationInMilliSeconds;
				fillGenerateFramesParameters(
					workspace, ingestionJobKey, ingestionType, parametersRoot, sourceMediaItemKey, sourcePhysicalPathKey,

					periodInSeconds, startTimeInSeconds, maxFramesNumber, videoFilter, mjpeg, imageWidth, imageHeight, durationInMilliSeconds
				);

				string fileFormat = "jpg";
				string frameFileName = to_string(ingestionJobKey) + "." + fileFormat;
				string frameAssetPathName = workspaceIngestionRepository + "/" + frameFileName;

				pid_t childPid;
				FFMpeg ffmpeg(_configurationRoot, _logger);
				ffmpeg.generateFrameToIngest(
					ingestionJobKey, sourcePhysicalPath, durationInMilliSeconds, startTimeInSeconds, frameAssetPathName, imageWidth, imageHeight,
					&childPid
				);

				{
					SPDLOG_INFO(
						string() + "Generated Frame to ingest" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", frameAssetPathName: " + frameAssetPathName +
						", fileFormat: " + fileFormat
					);

					string title;
					int64_t imageOfVideoMediaItemKey = sourceMediaItemKey;
					int64_t cutOfVideoMediaItemKey = -1;
					int64_t cutOfAudioMediaItemKey = -1;
					double startTimeInSeconds = 0.0;
					double endTimeInSeconds = 0.0;
					string imageMetaDataContent = generateMediaMetadataToIngest(
						ingestionJobKey, fileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey,
						startTimeInSeconds, endTimeInSeconds, parametersRoot
					);

					try
					{
						shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent = make_shared<LocalAssetIngestionEvent>();

						localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

						localAssetIngestionEvent->setExternalReadOnlyStorage(false);
						localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
						localAssetIngestionEvent->setIngestionSourceFileName(frameFileName);
						// localAssetIngestionEvent->setMMSSourceFileName(mmsSourceFileName);
						localAssetIngestionEvent->setMMSSourceFileName(frameFileName);
						localAssetIngestionEvent->setWorkspace(workspace);
						localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
						localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

						localAssetIngestionEvent->setMetadataContent(imageMetaDataContent);

						handleLocalAssetIngestionEvent(processorsThreadsNumber, *localAssetIngestionEvent);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", exception: " + e.what()
						);

						{
							SPDLOG_INFO(
								string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", frameAssetPathName: " + frameAssetPathName
							);
							fs::remove_all(frameAssetPathName);
						}

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", exception: " + e.what()
						);

						{
							SPDLOG_INFO(
								string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", frameAssetPathName: " + frameAssetPathName
							);
							fs::remove_all(frameAssetPathName);
						}

						throw e;
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "generate and ingest frame failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "generate and ingest frame failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "generateAndIngestFrame failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "generateAndIngestFrame failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::manageFaceRecognitionMediaTask(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong medias number to be processed for Face Recognition" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		string faceRecognitionCascadeName;
		string faceRecognitionOutput;
		long initialFramesNumberToBeSkipped;
		bool oneFramePerSecond;
		{
			string field = "cascadeName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceRecognitionCascadeName = JSONUtils::asString(parametersRoot, field, "");

			field = "output";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceRecognitionOutput = JSONUtils::asString(parametersRoot, field, "");

			initialFramesNumberToBeSkipped = 0;
			oneFramePerSecond = true;
			if (faceRecognitionOutput == "FrameContainingFace")
			{
				field = "initialFramesNumberToBeSkipped";
				initialFramesNumberToBeSkipped = JSONUtils::asInt(parametersRoot, field, 0);

				field = "oneFramePerSecond";
				oneFramePerSecond = JSONUtils::asBool(parametersRoot, field, true);
			}
		}

		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			string mmsAssetPathName;
			MMSEngineDBFacade::ContentType contentType;
			string title;
			int64_t sourceMediaItemKey;
			int64_t sourcePhysicalPathKey;

			int64_t key;
			MMSEngineDBFacade::ContentType referenceContentType;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(sourcePhysicalPathKey, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) =
					physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourceMediaItemKey = key;

				{
					bool warningIfMissing = false;
					tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
						contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
							workspace->_workspaceKey, key, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

					tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
				}
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourcePhysicalPathKey = key;

				{
					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

					tie(sourceMediaItemKey, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
				}
			}

			_mmsEngineDBFacade->addEncoding_FaceRecognitionJob(
				workspace, ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey, mmsAssetPathName, faceRecognitionCascadeName,
				faceRecognitionOutput, encodingPriority, initialFramesNumberToBeSkipped, oneFramePerSecond
			);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageFaceIdentificationMediaTask(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong medias number to be processed for Face Identification" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		string faceIdentificationCascadeName;
		string deepLearnedModelTagsCommaSeparated;
		{
			string field = "cascadeName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceIdentificationCascadeName = JSONUtils::asString(parametersRoot, field, "");

			field = "deepLearnedModelTags";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			deepLearnedModelTagsCommaSeparated = JSONUtils::asString(parametersRoot, field, "");
		}

		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			string mmsAssetPathName;
			MMSEngineDBFacade::ContentType contentType;
			string title;

			int64_t key;
			MMSEngineDBFacade::ContentType referenceContentType;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(ignore, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) =
					physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;

				{
					bool warningIfMissing = false;
					tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
						contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
							workspace->_workspaceKey, key, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

					tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
				}
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;
				{
					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

					tie(ignore, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
				}
			}

			_mmsEngineDBFacade->addEncoding_FaceIdentificationJob(
				workspace, ingestionJobKey, mmsAssetPathName, faceIdentificationCascadeName, deepLearnedModelTagsCommaSeparated, encodingPriority
			);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceIdendificationMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceIdendificationMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageLiveRecorder(
	int64_t ingestionJobKey, string ingestionJobLabel, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace,
	json parametersRoot
)
{
	try
	{
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		else
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

		string configurationLabel;

		int64_t confKey = -1;
		string streamSourceType;
		string encodersPoolLabel;
		string pullUrl;
		string pushProtocol;
		int64_t pushEncoderKey = -1;
		string pushEncoderName;
		int pushServerPort = -1;
		string pushUri;
		int pushListenTimeout = -1;
		int captureVideoDeviceNumber = -1;
		string captureVideoInputFormat;
		int captureFrameRate = -1;
		int captureWidth = -1;
		int captureHeight = -1;
		int captureAudioDeviceNumber = -1;
		int captureChannelsNumber = -1;
		int64_t tvSourceTVConfKey = -1;

		int64_t recordingCode;

		string recordingPeriodStart;
		string recordingPeriodEnd;
		bool autoRenew;
		string outputFileFormat;

		bool liveRecorderVirtualVOD = false;
		string virtualVODHlsChannelConfigurationLabel;
		int liveRecorderVirtualVODMaxDurationInMinutes = 30;
		int64_t virtualVODEncodingProfileKey = -1;
		// int virtualVODSegmentDurationInSeconds = 10;

		bool monitorHLS = false;
		string monitorHlsChannelConfigurationLabel;
		// int monitorPlaylistEntriesNumber = 0;
		// int monitorSegmentDurationInSeconds = 0;
		int64_t monitorEncodingProfileKey = -1;

		json outputsRoot = nullptr;
		json framesToBeDetectedRoot = nullptr;
		{
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				configurationLabel = JSONUtils::asString(parametersRoot, field, "");

				{
					bool pushPublicEncoderName = false;
					bool warningIfMissing = false;
					tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
						channelConfDetails = _mmsEngineDBFacade->getStreamDetails(workspace->_workspaceKey, configurationLabel, warningIfMissing);
					tie(confKey, streamSourceType, encodersPoolLabel, pullUrl, pushProtocol, pushEncoderKey, pushPublicEncoderName, pushServerPort,
						pushUri, pushListenTimeout, captureVideoDeviceNumber, captureVideoInputFormat, captureFrameRate, captureWidth, captureHeight,
						captureAudioDeviceNumber, captureChannelsNumber, tvSourceTVConfKey) = channelConfDetails;

					if (pushEncoderKey >= 0)
					{
						auto [pushEncoderLabel, publicServerName, internalServerName] = _mmsEngineDBFacade->getEncoderDetails(pushEncoderKey);

						if (pushPublicEncoderName)
							pushEncoderName = publicServerName;
						else
							pushEncoderName = internalServerName;
					}
					// default is IP_PULL
					if (streamSourceType == "")
						streamSourceType = "IP_PULL";
				}
			}

			// EncodersPool override the one included in ChannelConf if present
			field = "encodersPool";
			encodersPoolLabel = JSONUtils::asString(parametersRoot, field, encodersPoolLabel);

			field = "schedule";
			json recordingPeriodRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");

			field = "end";
			if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");

			field = "autoRenew";
			autoRenew = JSONUtils::asBool(recordingPeriodRoot, field, false);

			field = "outputFileFormat";
			outputFileFormat = JSONUtils::asString(parametersRoot, field, "ts");

			field = "monitorHLS";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json monitorHLSRoot = parametersRoot[field];

				monitorHLS = true;

				field = "hlsChannelConfigurationLabel";
				monitorHlsChannelConfigurationLabel = JSONUtils::asString(monitorHLSRoot, field, "");

				// field = "playlistEntriesNumber";
				// monitorPlaylistEntriesNumber =
				// JSONUtils::asInt(monitorHLSRoot, field, 6);

				// field = "segmentDurationInSeconds";
				// monitorSegmentDurationInSeconds =
				// JSONUtils::asInt(monitorHLSRoot, field, 10);

				string keyField = "encodingProfileKey";
				string labelField = "encodingProfileLabel";
				string contentTypeField = "contentType";
				if (JSONUtils::isMetadataPresent(monitorHLSRoot, keyField))
					monitorEncodingProfileKey = JSONUtils::asInt64(monitorHLSRoot, keyField, 0);
				else if (JSONUtils::isMetadataPresent(monitorHLSRoot, labelField))
				{
					string encodingProfileLabel = JSONUtils::asString(monitorHLSRoot, labelField, "");

					MMSEngineDBFacade::ContentType contentType;
					if (JSONUtils::isMetadataPresent(monitorHLSRoot, contentTypeField))
					{
						contentType = MMSEngineDBFacade::toContentType(JSONUtils::asString(monitorHLSRoot, contentTypeField, ""));

						monitorEncodingProfileKey =
							_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, contentType, encodingProfileLabel);
					}
					else
					{
						bool contentTypeToBeUsed = false;
						monitorEncodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
							workspace->_workspaceKey, contentType, encodingProfileLabel, contentTypeToBeUsed
						);
					}
				}
			}
			else
			{
				monitorHLS = false;
			}

			field = "liveRecorderVirtualVOD";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json virtualVODRoot = parametersRoot[field];

				liveRecorderVirtualVOD = true;

				field = "hlsChannelConfigurationLabel";
				virtualVODHlsChannelConfigurationLabel = JSONUtils::asString(virtualVODRoot, field, "");

				field = "maxDuration";
				liveRecorderVirtualVODMaxDurationInMinutes = JSONUtils::asInt(virtualVODRoot, field, 30);

				// field = "segmentDurationInSeconds";
				// virtualVODSegmentDurationInSeconds =
				// JSONUtils::asInt(virtualVODRoot, field, 10);

				string keyField = "encodingProfileKey";
				string labelField = "encodingProfileLabel";
				string contentTypeField = "contentType";
				if (JSONUtils::isMetadataPresent(virtualVODRoot, keyField))
					virtualVODEncodingProfileKey = JSONUtils::asInt64(virtualVODRoot, keyField, 0);
				else if (JSONUtils::isMetadataPresent(virtualVODRoot, labelField))
				{
					string encodingProfileLabel = JSONUtils::asString(virtualVODRoot, labelField, "");

					MMSEngineDBFacade::ContentType contentType;
					if (JSONUtils::isMetadataPresent(virtualVODRoot, contentTypeField))
					{
						contentType = MMSEngineDBFacade::toContentType(JSONUtils::asString(virtualVODRoot, contentTypeField, ""));

						virtualVODEncodingProfileKey =
							_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, contentType, encodingProfileLabel);
					}
					else
					{
						bool contentTypeToBeUsed = false;
						virtualVODEncodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
							workspace->_workspaceKey, contentType, encodingProfileLabel, contentTypeToBeUsed
						);
					}
				}
			}
			else
			{
				liveRecorderVirtualVOD = false;
			}

			field = "recordingCode";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingCode = JSONUtils::asInt64(parametersRoot, field, 0);

			field = "outputs";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
			else if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs"))
				outputsRoot = parametersRoot["Outputs"];

			if (JSONUtils::isMetadataPresent(parametersRoot, "framesToBeDetected"))
			{
				framesToBeDetectedRoot = parametersRoot["framesToBeDetected"];

				for (int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size(); pictureIndex++)
				{
					json frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

					if (JSONUtils::isMetadataPresent(frameToBeDetectedRoot, "picturePhysicalPathKey"))
					{
						int64_t physicalPathKey = JSONUtils::asInt64(frameToBeDetectedRoot, "picturePhysicalPathKey", -1);
						string picturePathName;

						tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
							physicalPathKey,
							// 2022-12-18: MIK potrebbe essere stato
							// appena aggiunto
							true
						);
						tie(picturePathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

						bool videoFrameToBeCropped = JSONUtils::asBool(frameToBeDetectedRoot, "videoFrameToBeCropped", false);
						if (videoFrameToBeCropped)
						{
							int width;
							int height;

							tuple<int, int, string, int> imageDetails = _mmsEngineDBFacade->getImageDetails(
								-1, physicalPathKey,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);
							tie(width, height, ignore, ignore) = imageDetails;

							frameToBeDetectedRoot["width"] = width;
							frameToBeDetectedRoot["height"] = height;
						}

						frameToBeDetectedRoot["picturePathName"] = picturePathName;

						framesToBeDetectedRoot[pictureIndex] = frameToBeDetectedRoot;
					}
				}
			}
		}

		// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

		time_t utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);
		// SPDLOG_ERROR(string() + "ctime recordingPeriodStart: "
		//		+ ctime(utcRecordingPeriodStart));

		// next code is the same in the Validator class
		time_t utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

		string tvType;
		int64_t tvServiceId = -1;
		int64_t tvFrequency = -1;
		int64_t tvSymbolRate = -1;
		int64_t tvBandwidthInHz = -1;
		string tvModulation;
		int tvVideoPid = -1;
		int tvAudioItalianPid = -1;
		string liveURL;

		if (streamSourceType == "IP_PULL")
		{
			liveURL = pullUrl;

			// string youTubePrefix1("https://www.youtube.com/");
			// string youTubePrefix2("https://youtu.be/");
			// if ((liveURL.size() >= youTubePrefix1.size() && 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
			// 	(liveURL.size() >= youTubePrefix2.size() && 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2)))
			if (StringUtils::startWith(liveURL, "https://www.youtube.com/")
			|| StringUtils::startWith(liveURL, "https://youtu.be/")
				)
			{
				liveURL = _mmsEngineDBFacade->getStreamingYouTubeLiveURL(workspace, ingestionJobKey, confKey, liveURL);
			}
		}
		else if (streamSourceType == "IP_PUSH")
		{
			liveURL = pushProtocol + "://" + pushEncoderName + ":" + to_string(pushServerPort) + pushUri;
		}
		else if (streamSourceType == "TV")
		{
			bool warningIfMissing = false;
			tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int> tvChannelConfDetails =
				_mmsEngineDBFacade->getSourceTVStreamDetails(tvSourceTVConfKey, warningIfMissing);

			tie(tvType, tvServiceId, tvFrequency, tvSymbolRate, tvBandwidthInHz, tvModulation, tvVideoPid, tvAudioItalianPid) = tvChannelConfDetails;
		}

		{
			int encodersNumber = _mmsEngineDBFacade->getEncodersNumberByEncodersPool(workspace->_workspaceKey, encodersPoolLabel);
			if (encodersNumber == 0)
			{
				string errorMessage = string() + "No encoders available" + ", ingestionJobKey: " + to_string(ingestionJobKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// in case we have monitorHLS and/or liveRecorderVirtualVOD,
		// this will be "translated" in one entry added to the outputsRoot
		int monitorVirtualVODOutputRootIndex = -1;
		if (monitorHLS || liveRecorderVirtualVOD)
		{
			string monitorVirtualVODHlsChannelConfigurationLabel;
			{
				if (virtualVODHlsChannelConfigurationLabel != "")
					monitorVirtualVODHlsChannelConfigurationLabel = virtualVODHlsChannelConfigurationLabel;
				else
					monitorVirtualVODHlsChannelConfigurationLabel = monitorHlsChannelConfigurationLabel;
			}

			int64_t monitorVirtualVODEncodingProfileKey = -1;
			{
				if (monitorEncodingProfileKey != -1 && virtualVODEncodingProfileKey != -1)
					monitorVirtualVODEncodingProfileKey = virtualVODEncodingProfileKey;
				else if (monitorEncodingProfileKey != -1 && virtualVODEncodingProfileKey == -1)
					monitorVirtualVODEncodingProfileKey = monitorEncodingProfileKey;
				else if (monitorEncodingProfileKey == -1 && virtualVODEncodingProfileKey != -1)
					monitorVirtualVODEncodingProfileKey = virtualVODEncodingProfileKey;
			}

			json encodingProfileDetailsRoot = nullptr;
			MMSEngineDBFacade::ContentType encodingProfileContentType = MMSEngineDBFacade::ContentType::Video;
			if (monitorVirtualVODEncodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, monitorVirtualVODEncodingProfileKey);
				tie(ignore, encodingProfileContentType, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}

			/*
			int monitorVirtualVODSegmentDurationInSeconds;
			{
				if (liveRecorderVirtualVOD)
					monitorVirtualVODSegmentDurationInSeconds =
			virtualVODSegmentDurationInSeconds; else
					monitorVirtualVODSegmentDurationInSeconds =
			monitorSegmentDurationInSeconds;
			}

			int monitorVirtualVODPlaylistEntriesNumber;
			{
				if (liveRecorderVirtualVOD)
				{
					monitorVirtualVODPlaylistEntriesNumber =
			(liveRecorderVirtualVODMaxDurationInMinutes * 60) /
						monitorVirtualVODSegmentDurationInSeconds;
				}
				else
					monitorVirtualVODPlaylistEntriesNumber =
			monitorPlaylistEntriesNumber;
			}
			*/

			json localOutputRoot;

			field = "outputType";
			localOutputRoot[field] = string("HLS_Channel");

			field = "hlsChannelConfigurationLabel";
			localOutputRoot[field] = monitorVirtualVODHlsChannelConfigurationLabel;

			// next fields will be initialized in EncoderVideoAudioProxy.cpp
			// when we will know the HLS Channel Configuration Label
			/*
			field = "otherOutputOptions";
			localOutputRoot[field] = otherOutputOptions;

			field = "manifestDirectoryPath";
			localOutputRoot[field] = monitorManifestDirectoryPath;

			field = "manifestFileName";
			localOutputRoot[field] = monitorManifestFileName;
			*/

			field = "filters";
			localOutputRoot[field] = nullptr;

			{
				field = "encodingProfileKey";
				localOutputRoot[field] = monitorVirtualVODEncodingProfileKey;

				field = "encodingProfileDetails";
				localOutputRoot[field] = encodingProfileDetailsRoot;

				field = "encodingProfileContentType";
				localOutputRoot[field] = MMSEngineDBFacade::toString(encodingProfileContentType);
			}

			outputsRoot.push_back(localOutputRoot);
			monitorVirtualVODOutputRootIndex = outputsRoot.size() - 1;

			field = "outputs";
			parametersRoot[field] = outputsRoot;

			_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(parametersRoot));
		}

		json localOutputsRoot = getReviewedOutputsRoot(outputsRoot, workspace, ingestionJobKey, false);

		// the recorder generates the chunks in a local(transcoder) directory
		string chunksTranscoderStagingContentsPath;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;
			string stagingLiveRecordingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder, workspace->_directoryName,
				to_string(ingestionJobKey), // directoryNamePrefix,
				"/",						// _encodingItem->_relativePath,
				to_string(ingestionJobKey),
				-1, // _encodingItem->_mediaItemKey, not used because
					// encodedFileName is not ""
				-1, // _encodingItem->_physicalPathKey, not used because
					// encodedFileName is not ""
				removeLinuxPathIfExist
			);
			size_t directoryEndIndex = stagingLiveRecordingAssetPathName.find_last_of("/");
			if (directoryEndIndex == string::npos)
			{
				string errorMessage = string() + "No directory found in the staging asset path name" +
									  ", _ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", stagingLiveRecordingAssetPathName: " + stagingLiveRecordingAssetPathName;
				SPDLOG_ERROR(errorMessage);

				// throw runtime_error(errorMessage);
			}
			chunksTranscoderStagingContentsPath = stagingLiveRecordingAssetPathName.substr(0, directoryEndIndex + 1);
		}

		// in case of 'localTranscoder', the chunks are moved to a shared
		// directory,
		//		specified by 'stagingContentsPath', to be ingested using a
		// copy/move source URL
		// in case of an external encoder, 'stagingContentsPath' is not used and
		// the chunk
		//		is ingested using PUSH through the binary MMS URL (mms-binary)
		string chunksNFSStagingContentsPath;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = false;
			string stagingLiveRecordingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder, workspace->_directoryName,
				to_string(ingestionJobKey), // directoryNamePrefix,
				"/",						// _encodingItem->_relativePath,
				to_string(ingestionJobKey),
				-1, // _encodingItem->_mediaItemKey, not used because
					// encodedFileName is not ""
				-1, // _encodingItem->_physicalPathKey, not used because
					// encodedFileName is not ""
				removeLinuxPathIfExist
			);
			size_t directoryEndIndex = stagingLiveRecordingAssetPathName.find_last_of("/");
			if (directoryEndIndex == string::npos)
			{
				string errorMessage = string() + "No directory found in the staging asset path name" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", stagingLiveRecordingAssetPathName: " + stagingLiveRecordingAssetPathName;
				SPDLOG_ERROR(errorMessage);

				// throw runtime_error(errorMessage);
			}
			chunksNFSStagingContentsPath = stagingLiveRecordingAssetPathName.substr(0, directoryEndIndex + 1);
		}

		// playlist where the recorder writes the chunks generated
		string segmentListFileName = to_string(ingestionJobKey) + ".liveRecorder.list";

		string recordedFileNamePrefix = string("liveRecorder_") + to_string(ingestionJobKey);

		// The VirtualVOD is built based on the HLS generated
		// in the Live Directory (.../MMSLive/1/<deliverycode>/...)
		// In case of an external encoder, the monitor will not work because the
		// Live Directory is not the one shared with the MMS Platform, but the
		// generated HLS will be used to build the Virtual VOD In case of a
		// local encoder, the virtual VOD is generated into a shared directory
		//		(virtualVODStagingContentsPath) to be ingested using a copy/move
		// source URL
		// In case of an external encoder, the virtual VOD is generated in a
		// local directory
		//	(virtualVODTranscoderStagingContentsPath) to be ingested using PUSH
		//(mms-binary)
		string virtualVODStagingContentsPath;
		string virtualVODTranscoderStagingContentsPath;
		if (liveRecorderVirtualVOD)
		{
			{
				bool removeLinuxPathIfExist = false;
				bool neededForTranscoder = false;
				virtualVODStagingContentsPath = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder, workspace->_directoryName,
					to_string(ingestionJobKey) + "_virtualVOD", // directoryNamePrefix,
					"/",										// _encodingItem->_relativePath,
					to_string(ingestionJobKey) + "_liveRecorderVirtualVOD",
					-1, // _encodingItem->_mediaItemKey, not used because
						// encodedFileName is not ""
					-1, // _encodingItem->_physicalPathKey, not used because
						// encodedFileName is not ""
					removeLinuxPathIfExist
				);
			}
			{
				bool removeLinuxPathIfExist = false;
				bool neededForTranscoder = true;
				virtualVODTranscoderStagingContentsPath = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder, workspace->_directoryName,
					to_string(ingestionJobKey) + "_virtualVOD", // directoryNamePrefix,
					"/",										// _encodingItem->_relativePath,
					// next param. is initialized with "content" because:
					//	- this is the external encoder scenario
					//	- in this scenario, the m3u8 virtual VOD, will be
					// ingested 		using PUSH and the mms-binary url
					//	- In this case (PUSH of m3u8) there is the
					// convention that 		the directory name has to be
					// 'content' 		(see the
					// TASK_01_Add_Content_JSON_Format.txt documentation)
					//	- the last part of the
					// virtualVODTranscoderStagingContentsPath variable
					// is used to name the m3u8 virtual vod directory
					"content", // to_string(_encodingItem->_ingestionJobKey)
							   // + "_liveRecorderVirtualVOD",
					-1,		   // _encodingItem->_mediaItemKey, not used because
							   // encodedFileName is not ""
					-1,		   // _encodingItem->_physicalPathKey, not used because
							   // encodedFileName is not ""
					removeLinuxPathIfExist
				);
			}
		}

		int64_t liveRecorderVirtualVODImageMediaItemKey = -1;
		if (liveRecorderVirtualVOD && _liveRecorderVirtualVODImageLabel != "")
		{
			try
			{
				bool warningIfMissing = true;
				pair<int64_t, MMSEngineDBFacade::ContentType> mediaItemDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					workspace->_workspaceKey, _liveRecorderVirtualVODImageLabel, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
				tie(liveRecorderVirtualVODImageMediaItemKey, ignore) = mediaItemDetails;
			}
			catch (MediaItemKeyNotFound e)
			{
				_logger->warn(
					string() + "No associated VirtualVODImage to the Workspace" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _liveRecorderVirtualVODImageLabel: " + _liveRecorderVirtualVODImageLabel + ", exception: " + e.what()
				);

				liveRecorderVirtualVODImageMediaItemKey = -1;
			}
			catch (exception e)
			{
				SPDLOG_ERROR(
					string() +
					"_mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName "
					"failed" +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", _liveRecorderVirtualVODImageLabel: " + _liveRecorderVirtualVODImageLabel +
					", exception: " + e.what()
				);

				liveRecorderVirtualVODImageMediaItemKey = -1;
			}
		}

		json captureRoot;
		json tvRoot;
		if (streamSourceType == "CaptureLive")
		{
			captureRoot["videoDeviceNumber"] = captureVideoDeviceNumber;
			captureRoot["videoInputFormat"] = captureVideoInputFormat;
			captureRoot["frameRate"] = captureFrameRate;
			captureRoot["width"] = captureWidth;
			captureRoot["height"] = captureHeight;
			captureRoot["audioDeviceNumber"] = captureAudioDeviceNumber;
			captureRoot["channelsNumber"] = captureChannelsNumber;
		}
		else if (streamSourceType == "TV")
		{
			tvRoot["type"] = tvType;
			tvRoot["serviceId"] = tvServiceId;
			tvRoot["frequency"] = tvFrequency;
			tvRoot["symbolRate"] = tvSymbolRate;
			tvRoot["bandwidthInHz"] = tvBandwidthInHz;
			tvRoot["modulation"] = tvModulation;
			tvRoot["videoPid"] = tvVideoPid;
			tvRoot["audioItalianPid"] = tvAudioItalianPid;
		}

		_mmsEngineDBFacade->addEncoding_LiveRecorderJob(
			workspace, ingestionJobKey, ingestionJobLabel, streamSourceType, configurationLabel, confKey, liveURL, encodersPoolLabel,

			encodingPriority,

			pushListenTimeout, pushEncoderKey, pushEncoderName, captureRoot,

			tvRoot,

			monitorHLS, liveRecorderVirtualVOD, monitorVirtualVODOutputRootIndex,

			localOutputsRoot, framesToBeDetectedRoot,

			chunksTranscoderStagingContentsPath, chunksNFSStagingContentsPath, segmentListFileName, recordedFileNamePrefix,
			virtualVODStagingContentsPath, virtualVODTranscoderStagingContentsPath, liveRecorderVirtualVODImageMediaItemKey,

			_mmsWorkflowIngestionURL, _mmsBinaryIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveRecorder failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveRecorder failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageLiveProxy(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		/*
		 * commented because it will be High by default
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority =
				static_cast<MMSEngineDBFacade::EncodingPriority>(
				workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority =
				MMSEngineDBFacade::toEncodingPriority(
				parametersRoot.get(field, "").asString());
		}
		*/

		string configurationLabel;

		string streamSourceType;
		bool defaultBroadcast = false;
		int maxWidth = -1;
		string userAgent;
		string otherInputOptions;

		long waitingSecondsBetweenAttemptsInCaseOfErrors;
		json outputsRoot;
		bool timePeriod = false;
		int64_t utcProxyPeriodStart = -1;
		int64_t utcProxyPeriodEnd = -1;
		int64_t useVideoTrackFromMediaItemKey = -1;
		string taskEncodersPoolLabel;
		{
			{
				string field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				configurationLabel = JSONUtils::asString(parametersRoot, field, "");

				field = "useVideoTrackFromMediaItemKey";
				useVideoTrackFromMediaItemKey = JSONUtils::asInt64(parametersRoot, field, -1);

				{
					bool warningIfMissing = false;
					tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
						channelConfDetails = _mmsEngineDBFacade->getStreamDetails(workspace->_workspaceKey, configurationLabel, warningIfMissing);
					tie(ignore, streamSourceType, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
						ignore, ignore, ignore, ignore) = channelConfDetails;

					// default is IP_PULL
					if (streamSourceType == "")
						streamSourceType = "IP_PULL";
				}
			}

			// EncodersPool override the one included in ChannelConf if present
			string field = "encodersPool";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				taskEncodersPoolLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "defaultBroadcast";
			defaultBroadcast = JSONUtils::asBool(parametersRoot, field, false);

			field = "timePeriod";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				timePeriod = JSONUtils::asBool(parametersRoot, field, false);
				if (timePeriod)
				{
					field = "schedule";
					if (!JSONUtils::isMetadataPresent(parametersRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					// Validator validator(_logger, _mmsEngineDBFacade,
					// _configuration);

					json proxyPeriodRoot = parametersRoot[field];

					field = "start";
					if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
					utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);

					field = "end";
					if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
					utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
				}
			}

			field = "maxWidth";
			maxWidth = JSONUtils::asInt(parametersRoot, field, -1);

			field = "userAgent";
			userAgent = JSONUtils::asString(parametersRoot, field, "");

			field = "otherInputOptions";
			otherInputOptions = JSONUtils::asString(parametersRoot, field, "");

			// field = "maxAttemptsNumberInCaseOfErrors";
			// if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			// 	maxAttemptsNumberInCaseOfErrors = 3;
			// else
			// 	maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(
			// 		parametersRoot, field, 0);

			field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
			waitingSecondsBetweenAttemptsInCaseOfErrors = JSONUtils::asInt64(parametersRoot, field, 5);

			field = "outputs";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
			else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs",
				 // false))
				outputsRoot = parametersRoot["Outputs"];
		}

		string useVideoTrackFromPhysicalPathName;
		string useVideoTrackFromPhysicalDeliveryURL;
		if (useVideoTrackFromMediaItemKey != -1)
		{
			int64_t sourcePhysicalPathKey;

			SPDLOG_INFO(
				string() + "useVideoTrackFromMediaItemKey" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", useVideoTrackFromMediaItemKey: " + to_string(useVideoTrackFromMediaItemKey)
			);

			{
				int64_t encodingProfileKey = -1;
				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
					useVideoTrackFromMediaItemKey, encodingProfileKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
				tie(sourcePhysicalPathKey, useVideoTrackFromPhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
			}

			// calculate delivery URL in case of an external encoder
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

					sourcePhysicalPathKey,

					-1, // ingestionJobKey,	(in case of live)
					-1, // deliveryCode,

					365 * 24 * 60 * 60, // ttlInSeconds, 365 days!!!
					999999,				// maxRetries,
					false,				// save,
					"MMS_SignedToken",	// deliveryType,

					false, // warningIfMissingMediaItemKey,
					true,  // filteredByStatistic
					""	   // userId (it is not needed it filteredByStatistic is
					   // true
				);

				tie(useVideoTrackFromPhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
			}
		}

		json localOutputsRoot = getReviewedOutputsRoot(outputsRoot, workspace, ingestionJobKey, false);

		// 2021-12-22: in case of a Broadcaster, we may have a playlist
		// (inputsRoot) already ready
		json inputsRoot;
		if (JSONUtils::isMetadataPresent(parametersRoot, "internalMMS") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"], "broadcaster") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"]["broadcaster"], "broadcasterInputsRoot"))
		{
			inputsRoot = parametersRoot["internalMMS"]["broadcaster"]["broadcasterInputsRoot"];
		}
		else
		{
			/* 2023-01-01: normalmente, nel caso di un semplice Task di
			   LiveProxy, il drawTextDetails viene inserito all'interno
			   dell'OutputRoot, Nel caso pero' di un Live Channel, per capire il
			   campo broadcastDrawTextDetails, vedi il commento all'interno del
			   metodo java CatraMMSBroadcaster::buildLiveProxyJsonForBroadcast
			*/
			json drawTextDetailsRoot = nullptr;

			string field = "broadcastDrawTextDetails";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				drawTextDetailsRoot = parametersRoot[field];

			json streamInputRoot = _mmsEngineDBFacade->getStreamInputRoot(
				workspace, ingestionJobKey, configurationLabel, useVideoTrackFromPhysicalPathName, useVideoTrackFromPhysicalDeliveryURL, maxWidth,
				userAgent, otherInputOptions, taskEncodersPoolLabel, drawTextDetailsRoot
			);

			json inputRoot;
			{
				string field = "streamInput";
				inputRoot[field] = streamInputRoot;

				field = "timePeriod";
				inputRoot[field] = timePeriod;

				field = "utcScheduleStart";
				inputRoot[field] = utcProxyPeriodStart;

				field = "sUtcScheduleStart";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodStart);

				field = "utcScheduleEnd";
				inputRoot[field] = utcProxyPeriodEnd;

				field = "sUtcScheduleEnd";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodEnd);

				if (defaultBroadcast)
				{
					field = "defaultBroadcast";
					inputRoot[field] = defaultBroadcast;
				}
			}

			json localInputsRoot = json::array();
			localInputsRoot.push_back(inputRoot);

			inputsRoot = localInputsRoot;
		}

		_mmsEngineDBFacade->addEncoding_LiveProxyJob(
			workspace, ingestionJobKey,
			inputsRoot,			 // used by FFMPEGEncoder
			streamSourceType,	 // used by FFMPEGEncoder
			utcProxyPeriodStart, // used in MMSEngineDBFacade::getEncodingJobs
			// maxAttemptsNumberInCaseOfErrors,	// used in
			// EncoderVideoAudioProxy.cpp
			waitingSecondsBetweenAttemptsInCaseOfErrors, // used in
														 // EncoderVideoAudioProxy.cpp
			localOutputsRoot,							 // used by FFMPEGEncoder
			_mmsWorkflowIngestionURL
		);
	}
	catch (ConfKeyNotFound &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw runtime_error(e.what());
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageVODProxy(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		SPDLOG_INFO(
			string() + "manageVODProxy" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		if (dependencies.size() < 1)
		{
			string errorMessage = string() + "Wrong source number to be proxied" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		json outputsRoot;
		bool timePeriod = false;
		int64_t utcProxyPeriodStart = -1;
		int64_t utcProxyPeriodEnd = -1;
		bool defaultBroadcast = false;
		{
			string field = "timePeriod";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				timePeriod = JSONUtils::asBool(parametersRoot, field, false);
				if (timePeriod)
				{
					field = "schedule";
					if (!JSONUtils::isMetadataPresent(parametersRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					// Validator validator(_logger, _mmsEngineDBFacade,
					// _configuration);

					json proxyPeriodRoot = parametersRoot[field];

					field = "start";
					if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
					utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);

					field = "end";
					if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
					utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
				}
			}

			field = "defaultBroadcast";
			defaultBroadcast = JSONUtils::asBool(parametersRoot, field, false);

			field = "outputs";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
			else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs",
				 // false))
				outputsRoot = parametersRoot["Outputs"];
		}

		MMSEngineDBFacade::ContentType vodContentType;

		json inputsRoot = json::array();
		// 2021-12-22: in case of a Broadcaster, we may have a playlist
		// (inputsRoot)
		//		already ready
		if (JSONUtils::isMetadataPresent(parametersRoot, "internalMMS") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"], "broadcaster") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"]["broadcaster"], "broadcasterInputsRoot"))
		{
			inputsRoot = parametersRoot["internalMMS"]["broadcaster"]["broadcasterInputsRoot"];
		}
		else
		{
			vector<tuple<int64_t, string, string, string>> sources;

			for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
			{
				string sourcePhysicalPathName;
				int64_t sourcePhysicalPathKey;

				int64_t key;
				// MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

				tie(key, vodContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				SPDLOG_INFO(
					string() + "manageVODProxy" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", key: " + to_string(key)
				);

				int64_t sourceMediaItemKey;
				string mediaItemTitle;
				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;
					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(sourcePhysicalPathKey, sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

					sourceMediaItemKey = key;

					warningIfMissing = false;
					tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemKeyDetails =
						_mmsEngineDBFacade->getMediaItemKeyDetails(
							workspace->_workspaceKey, sourceMediaItemKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

					tie(ignore, mediaItemTitle, ignore, ignore, ignore, ignore) = mediaItemKeyDetails;
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

					sourcePhysicalPathKey = key;

					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemKeyDetails =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato
							// appena aggiunto
							true
						);

					tie(sourceMediaItemKey, ignore, mediaItemTitle, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemKeyDetails;
				}

				// int64_t durationInMilliSeconds =
				// 	_mmsEngineDBFacade->getMediaDurationInMilliseconds(
				// 		-1, sourcePhysicalPathKey);

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

						sourcePhysicalPathKey,

						-1, // ingestionJobKey,	(in case of live)
						-1, // deliveryCode,

						365 * 24 * 60 * 60, // ttlInSeconds, 365 days!!!
						999999,				// maxRetries,
						false,				// save,
						"MMS_SignedToken",	// deliveryType,

						false, // warningIfMissingMediaItemKey,
						true,  // filteredByStatistic
						""	   // userId (it is not needed it
							   // filteredByStatistic is true
					);

					tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
				}

				sources.push_back(make_tuple(sourcePhysicalPathKey, mediaItemTitle, sourcePhysicalPathName, sourcePhysicalDeliveryURL));
			}

			/* 2023-01-01: normalmente, nel caso di un semplica Task di
			   VODProxy, il drawTextDetails viene inserito all'interno
			   dell'OutputRoot, Nel caso pero' di un Live Channel, per capire il
			   campo broadcastDrawTextDetails, vedi il commento all'interno del
			   metodo java CatraMMSBroadcaster::buildVODProxyJsonForBroadcast
			*/
			json drawTextDetailsRoot = nullptr;

			string field = "broadcastDrawTextDetails";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				drawTextDetailsRoot = parametersRoot[field];

			// same json structure is used in
			// API_Ingestion::changeLiveProxyPlaylist
			json vodInputRoot = _mmsEngineDBFacade->getVodInputRoot(vodContentType, sources, drawTextDetailsRoot);

			json inputRoot;
			{
				string field = "vodInput";
				inputRoot[field] = vodInputRoot;

				field = "timePeriod";
				inputRoot[field] = timePeriod;

				field = "utcScheduleStart";
				inputRoot[field] = utcProxyPeriodStart;

				field = "sUtcScheduleStart";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodStart);

				field = "utcScheduleEnd";
				inputRoot[field] = utcProxyPeriodEnd;

				field = "sUtcScheduleEnd";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodEnd);

				if (defaultBroadcast)
				{
					field = "defaultBroadcast";
					inputRoot[field] = defaultBroadcast;
				}
			}

			inputsRoot.push_back(inputRoot);
		}

		json localOutputsRoot =
			getReviewedOutputsRoot(outputsRoot, workspace, ingestionJobKey, vodContentType == MMSEngineDBFacade::ContentType::Image ? true : false);

		// the only reason we may have a failure should be in case the vod is
		// missing/removed
		long waitingSecondsBetweenAttemptsInCaseOfErrors = 30;
		long maxAttemptsNumberInCaseOfErrors = 3;

		_mmsEngineDBFacade->addEncoding_VODProxyJob(
			workspace, ingestionJobKey, inputsRoot,

			utcProxyPeriodStart, localOutputsRoot, maxAttemptsNumberInCaseOfErrors, waitingSecondsBetweenAttemptsInCaseOfErrors,
			_mmsWorkflowIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageCountdown(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, string ingestionDate, shared_ptr<Workspace> workspace,
	json parametersRoot, vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong media number for Countdown" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool defaultBroadcast = false;
		bool timePeriod = true;
		int64_t utcProxyPeriodStart = -1;
		int64_t utcProxyPeriodEnd = -1;
		{
			string field = "defaultBroadcast";
			defaultBroadcast = JSONUtils::asBool(parametersRoot, field, false);

			field = "schedule";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

			json proxyPeriodRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);

			field = "end";
			if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
		}

		string mmsSourceVideoAssetPathName;
		string mmsSourceVideoAssetDeliveryURL;
		MMSEngineDBFacade::ContentType referenceContentType;
		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		{
			int64_t key;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];
			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPath = _mmsStorage->getPhysicalPathDetails(
					key, encodingProfileKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
				tie(sourcePhysicalPathKey, mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPath;

				sourceMediaItemKey = key;

				// sourcePhysicalPathKey = -1;
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPath = _mmsStorage->getPhysicalPathDetails(
					key,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
				tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPath;

				sourcePhysicalPathKey = key;

				bool warningIfMissing = false;
				tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

				MMSEngineDBFacade::ContentType localContentType;
				string localTitle;
				string userData;
				string ingestionDate;
				int64_t localIngestionJobKey;
				tie(sourceMediaItemKey, localContentType, localTitle, userData, ingestionDate, localIngestionJobKey, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}

			// calculate delivery URL in case of an external encoder
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

					sourcePhysicalPathKey,

					-1, // ingestionJobKey,	(in case of live)
					-1, // deliveryCode,

					abs(utcNow - utcProxyPeriodEnd), // ttlInSeconds,
					999999,							 // maxRetries,
					false,							 // save,
					"MMS_SignedToken",				 // deliveryType,

					false, // warningIfMissingMediaItemKey,
					true,  // filteredByStatistic
					""	   // userId (it is not needed it filteredByStatistic is
					   // true
				);

				tie(mmsSourceVideoAssetDeliveryURL, ignore) = deliveryAuthorizationDetails;
			}
		}

		int64_t videoDurationInMilliSeconds = _mmsEngineDBFacade->getMediaDurationInMilliseconds(
			sourceMediaItemKey, sourcePhysicalPathKey,
			// 2022-12-18: MIK potrebbe essere stato appena aggiunto
			true
		);

		json outputsRoot;
		{
			string field = "outputs";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
			else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs",
				 // false))
				outputsRoot = parametersRoot["Outputs"];
		}

		json localOutputsRoot = getReviewedOutputsRoot(
			outputsRoot, workspace, ingestionJobKey, referenceContentType == MMSEngineDBFacade::ContentType::Image ? true : false
		);

		// 2021-12-22: in case of a Broadcaster, we may have a playlist
		// (inputsRoot) already ready
		json inputsRoot;
		if (JSONUtils::isMetadataPresent(parametersRoot, "internalMMS") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"], "broadcaster") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"]["broadcaster"], "broadcasterInputsRoot"))
		{
			inputsRoot = parametersRoot["internalMMS"]["broadcaster"]["broadcasterInputsRoot"];
		}
		else
		{
			/* 2023-01-01: normalmente, nel caso di un semplica Task di
			   Countdown, il drawTextDetails viene inserito all'interno
			   dell'OutputRoot, Nel caso pero' di un Live Channel, per capire il
			   campo broadcastDrawTextDetails, vedi il commento all'interno del
			   metodo java CatraMMSBroadcaster::buildCountdownJsonForBroadcast
			*/
			json drawTextDetailsRoot = nullptr;

			string field = "broadcastDrawTextDetails";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				drawTextDetailsRoot = parametersRoot[field];

			// same json structure is used in
			// API_Ingestion::changeLiveProxyPlaylist
			json countdownInputRoot = _mmsEngineDBFacade->getCountdownInputRoot(
				mmsSourceVideoAssetPathName, mmsSourceVideoAssetDeliveryURL, sourcePhysicalPathKey, videoDurationInMilliSeconds, drawTextDetailsRoot
			);

			json inputRoot;
			{
				string field = "countdownInput";
				inputRoot[field] = countdownInputRoot;

				field = "timePeriod";
				inputRoot[field] = timePeriod;

				field = "utcScheduleStart";
				inputRoot[field] = utcProxyPeriodStart;

				field = "sUtcScheduleStart";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodStart);

				field = "utcScheduleEnd";
				inputRoot[field] = utcProxyPeriodEnd;

				field = "sUtcScheduleEnd";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodEnd);

				if (defaultBroadcast)
				{
					field = "defaultBroadcast";
					inputRoot[field] = defaultBroadcast;
				}
			}

			json localInputsRoot = json::array();
			localInputsRoot.push_back(inputRoot);

			inputsRoot = localInputsRoot;
		}

		// the only reason we may have a failure should be in case the image is
		// missing/removed
		long waitingSecondsBetweenAttemptsInCaseOfErrors = 30;
		long maxAttemptsNumberInCaseOfErrors = 3;

		_mmsEngineDBFacade->addEncoding_CountdownJob(
			workspace, ingestionJobKey, inputsRoot, utcProxyPeriodStart, localOutputsRoot, maxAttemptsNumberInCaseOfErrors,
			waitingSecondsBetweenAttemptsInCaseOfErrors, _mmsWorkflowIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
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
		bool awsSignedURL;
		int awsExpirationInMinutes;
		string cdn77ChannelConfigurationLabel;
		int cdn77ExpirationInMinutes;
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

void MMSEngineProcessor::manageLiveGrid(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		/*
		 * commented because it will be High by default
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority =
				static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority =
				MMSEngineDBFacade::toEncodingPriority(parametersRoot.get(field,
		"XXX").asString());
		}
		*/

		json inputChannelsRoot = json::array();
		json outputsRoot;
		{
			string field = "inputConfigurationLabels";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json inputChannelsRoot = parametersRoot[field];
			for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsRoot.size(); inputChannelIndex++)
			{
				string configurationLabel = JSONUtils::asString(inputChannelsRoot[inputChannelIndex]);

				bool warningIfMissing = false;
				tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
					confKeyAndChannelURL = _mmsEngineDBFacade->getStreamDetails(workspace->_workspaceKey, configurationLabel, warningIfMissing);

				int64_t confKey;
				string streamSourceType;
				string liveURL;
				tie(confKey, streamSourceType, ignore, liveURL, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore, ignore) = confKeyAndChannelURL;

				// bisognerebbe verificare streamSourceType

				// string youTubePrefix1("https://www.youtube.com/");
				// string youTubePrefix2("https://youtu.be/");
				// if ((liveURL.size() >= youTubePrefix1.size() && 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
				// 	(liveURL.size() >= youTubePrefix2.size() && 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2)))
			if (StringUtils::startWith(liveURL, "https://www.youtube.com/")
			|| StringUtils::startWith(liveURL, "https://youtu.be/")
				)
				{
					liveURL = _mmsEngineDBFacade->getStreamingYouTubeLiveURL(workspace, ingestionJobKey, confKey, liveURL);
				}

				json inputChannelRoot;

				inputChannelRoot["confKey"] = confKey;
				inputChannelRoot["configurationLabel"] = configurationLabel;
				inputChannelRoot["liveURL"] = liveURL;

				inputChannelsRoot.push_back(inputChannelRoot);
			}

			field = "outputs";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
		}

		json localOutputsRoot = getReviewedOutputsRoot(outputsRoot, workspace, ingestionJobKey, false);

		_mmsEngineDBFacade->addEncoding_LiveGridJob(
			workspace, ingestionJobKey, inputChannelsRoot,
			localOutputsRoot // used by FFMPEGEncoder
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveGrid failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveGrid failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageLiveCutThread_streamSegmenter(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json liveCutParametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			string() + "manageLiveCutThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		// string streamSourceType;
		// string ipConfigurationLabel;
		// string satConfigurationLabel;
		int64_t recordingCode;
		string cutPeriodStartTimeInMilliSeconds;
		string cutPeriodEndTimeInMilliSeconds;
		int maxWaitingForLastChunkInSeconds = 90;
		bool errorIfAChunkIsMissing = false;
		{
			/*
			string field = "streamSourceType";
			if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it
			is null"
					+ ", _processorIdentifier: " +
			to_string(_processorIdentifier)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamSourceType = liveCutParametersRoot.get(field, "").asString();

			if (streamSourceType == "IP_PULL")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or
			it is null"
						+ ", _processorIdentifier: " +
			to_string(_processorIdentifier)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				ipConfigurationLabel = liveCutParametersRoot.get(field,
			"").asString();
			}
			else if (streamSourceType == "Satellite")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or
			it is null"
						+ ", _processorIdentifier: " +
			to_string(_processorIdentifier)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				satConfigurationLabel = liveCutParametersRoot.get(field,
			"").asString();
			}
			*/

			// else if (streamSourceType == "IP_PUSH")
			string field = "recordingCode";
			if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingCode = JSONUtils::asInt64(liveCutParametersRoot, field, -1);

			field = "maxWaitingForLastChunkInSeconds";
			maxWaitingForLastChunkInSeconds = JSONUtils::asInt64(liveCutParametersRoot, field, 90);

			field = "errorIfAChunkIsMissing";
			errorIfAChunkIsMissing = JSONUtils::asBool(liveCutParametersRoot, field, false);

			field = "cutPeriod";
			json cutPeriodRoot = liveCutParametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodStartTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, field, "");

			field = "end";
			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodEndTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, field, "");
		}

		// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

		int64_t utcCutPeriodStartTimeInMilliSeconds = DateTime::sDateMilliSecondsToUtc(cutPeriodStartTimeInMilliSeconds);

		// next code is the same in the Validator class
		int64_t utcCutPeriodEndTimeInMilliSeconds = DateTime::sDateMilliSecondsToUtc(cutPeriodEndTimeInMilliSeconds);

		/*
		 * 2020-03-30: scenario: period end time is 300 seconds (5 minutes). In
		 * case the chunk is 1 minute, we will take 5 chunks. The result is that
		 * the Cut will fail because:
		 * - we need to cut to 300 seconds
		 * - the duration of the video is 298874 milliseconds
		 * For this reason, when we retrieve the chunks, we will use 'period end
		 * time' plus one second
		 */
		int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = utcCutPeriodEndTimeInMilliSeconds + 1000;

		/*
		int64_t confKey = -1;
		if (streamSourceType == "IP_PULL")
		{
			bool warningIfMissing = false;
			pair<int64_t, string> confKeyAndLiveURL =
		_mmsEngineDBFacade->getIPChannelConfDetails( workspace->_workspaceKey,
		ipConfigurationLabel, warningIfMissing); tie(confKey, ignore) =
		confKeyAndLiveURL;
		}
		else if (streamSourceType == "Satellite")
		{
			bool warningIfMissing = false;
			confKey = _mmsEngineDBFacade->getSATChannelConfDetails(
				workspace->_workspaceKey, satConfigurationLabel,
		warningIfMissing);
		}
		*/

		json mediaItemKeyReferencesRoot = json::array();
		int64_t utcFirstChunkStartTime;
		string firstChunkStartTime;
		int64_t utcLastChunkEndTime;
		string lastChunkEndTime;

		chrono::system_clock::time_point startLookingForChunks = chrono::system_clock::now();

		bool firstRequestedChunk = false;
		bool lastRequestedChunk = false;
		while (!lastRequestedChunk && (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startLookingForChunks).count() <
									   maxWaitingForLastChunkInSeconds))
		{
			int64_t mediaItemKey = -1;
			int64_t physicalPathKey = -1;
			string uniqueName;
			vector<int64_t> otherMediaItemsKey;
			int start = 0;
			int rows = 60 * 1; // assuming every MediaItem is one minute, let's take 1 hour
			bool contentTypePresent = true;
			MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::ContentType::Video;
			// bool startAndEndIngestionDatePresent = false;
			string startIngestionDate;
			string endIngestionDate;
			string title;
			int liveRecordingChunk = 1;
			vector<string> tagsIn;
			vector<string> tagsNotIn;
			string orderBy = "";
			bool admin = false;

			firstRequestedChunk = false;
			lastRequestedChunk = false;

			string jsonCondition;
			{
				// SC: Start Chunk
				// PS: Playout Start, PE: Playout End
				// --------------SC--------------SC--------------SC--------------SC
				//                       PS-------------------------------PE

				jsonCondition = "(";

				// first chunk of the cut
				jsonCondition +=
					("(JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') * "
					 "1000 <= " +
					 to_string(utcCutPeriodStartTimeInMilliSeconds) + " " + "and " + to_string(utcCutPeriodStartTimeInMilliSeconds) +
					 " < JSON_EXTRACT(userData, '$.mmsData.utcChunkEndTime') * "
					 "1000 ) ");

				jsonCondition += " or ";

				// internal chunk of the cut
				jsonCondition +=
					("( " + to_string(utcCutPeriodStartTimeInMilliSeconds) +
					 " <= JSON_EXTRACT(userData, "
					 "'$.mmsData.utcChunkStartTime') * 1000 " +
					 "and JSON_EXTRACT(userData, '$.mmsData.utcChunkEndTime') "
					 "* 1000 <= " +
					 to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond) + ") ");

				jsonCondition += " or ";

				// last chunk of the cut
				jsonCondition +=
					("( JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') "
					 "* 1000 < " +
					 to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond) + " " + "and " +
					 to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond) +
					 " <= JSON_EXTRACT(userData, '$.mmsData.utcChunkEndTime') "
					 "* 1000 ) ");

				jsonCondition += ")";
			}
			string jsonOrderBy = "JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') asc";

			long utcPreviousUtcChunkEndTime = -1;
			bool firstRetrievedChunk = true;

			// retrieve the reference of all the MediaItems to be concatenate
			mediaItemKeyReferencesRoot.clear();

			// json mediaItemsListRoot;
			json mediaItemsRoot;
			do
			{
				int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
				int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
				set<string> responseFields;
				json mediaItemsListRoot = _mmsEngineDBFacade->getMediaItemsList(
					workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey, start, rows, contentTypePresent,
					contentType,
					// startAndEndIngestionDatePresent,
					startIngestionDate, endIngestionDate, title, liveRecordingChunk, recordingCode, utcCutPeriodStartTimeInMilliSeconds,
					utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, tagsIn, tagsNotIn, orderBy, jsonOrderBy, responseFields, admin,
					// 2022-12-18: MIKs potrebbero essere stati appena
					// aggiunti
					true
				);

				string field = "response";
				json responseRoot = mediaItemsListRoot[field];

				field = "mediaItems";
				mediaItemsRoot = responseRoot[field];

				for (int mediaItemIndex = 0; mediaItemIndex < mediaItemsRoot.size(); mediaItemIndex++)
				{
					json mediaItemRoot = mediaItemsRoot[mediaItemIndex];

					field = "mediaItemKey";
					int64_t mediaItemKey = JSONUtils::asInt64(mediaItemRoot, field, 0);

					json userDataRoot;
					{
						field = "userData";
						string userData = JSONUtils::asString(mediaItemRoot, field, "");
						if (userData == "")
						{
							string errorMessage = string() + "recording media item without userData!!!" +
												  ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						userDataRoot = JSONUtils::toJson(userData);
					}

					field = "mmsData";
					json mmsDataRoot = userDataRoot[field];

					field = "utcChunkStartTime";
					int64_t currentUtcChunkStartTime = JSONUtils::asInt64(mmsDataRoot, field, 0);

					field = "utcChunkEndTime";
					int64_t currentUtcChunkEndTime = JSONUtils::asInt64(mmsDataRoot, field, 0);

					string currentChunkStartTime;
					string currentChunkEndTime;
					{
						char strDateTime[64];
						tm tmDateTime;

						localtime_r(&currentUtcChunkStartTime, &tmDateTime);
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
						);
						currentChunkStartTime = strDateTime;

						localtime_r(&currentUtcChunkEndTime, &tmDateTime);
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
						);
						currentChunkEndTime = strDateTime;
					}

					SPDLOG_INFO(
						string() + "Retrieved chunk" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaITemKey: " + to_string(mediaItemKey) +
						", currentUtcChunkStartTime: " + to_string(currentUtcChunkStartTime) + " (" + currentChunkStartTime + ")" +
						", currentUtcChunkEndTime: " + to_string(currentUtcChunkEndTime) + " (" + currentChunkEndTime + ")"
					);

					// check if it is the next chunk
					if (utcPreviousUtcChunkEndTime != -1 && utcPreviousUtcChunkEndTime != currentUtcChunkStartTime)
					{
						string previousUtcChunkEndTime;
						{
							char strDateTime[64];
							tm tmDateTime;

							localtime_r(&utcPreviousUtcChunkEndTime, &tmDateTime);

							sprintf(
								strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
								tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
							);
							previousUtcChunkEndTime = strDateTime;
						}

						// it is not the next chunk
						string errorMessage =
							string("#Chunks check. Next chunk was not found") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", utcPreviousUtcChunkEndTime: " + to_string(utcPreviousUtcChunkEndTime) + " (" + previousUtcChunkEndTime + ")" +
							", currentUtcChunkStartTime: " + to_string(currentUtcChunkStartTime) + " (" + currentChunkStartTime + ")" +
							", currentUtcChunkEndTime: " + to_string(currentUtcChunkEndTime) + " (" + currentChunkEndTime + ")" +
							", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
							cutPeriodStartTimeInMilliSeconds + ")" +
							", utcCutPeriodEndTimeInMilliSeconds: " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
							cutPeriodEndTimeInMilliSeconds + ")";
						if (errorIfAChunkIsMissing)
						{
							SPDLOG_ERROR(string() + errorMessage);

							throw runtime_error(errorMessage);
						}
						else
						{
							_logger->warn(string() + errorMessage);
						}
					}

					// check if it is the first chunk
					if (firstRetrievedChunk)
					{
						firstRetrievedChunk = false;

						// check that it is the first chunk

						if (!(currentUtcChunkStartTime * 1000 <= utcCutPeriodStartTimeInMilliSeconds &&
							  utcCutPeriodStartTimeInMilliSeconds < currentUtcChunkEndTime * 1000))
						{
							firstRequestedChunk = false;

							// it is not the first chunk
							string errorMessage =
								string("#Chunks check. First chunk was not found") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", first utcChunkStart: " + to_string(currentUtcChunkStartTime) +
								" (" + currentChunkStartTime + ")" + ", first currentUtcChunkEndTime: " + to_string(currentUtcChunkEndTime) + " (" +
								currentChunkEndTime + ")" +
								", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
								cutPeriodStartTimeInMilliSeconds + ")" +
								", utcCutPeriodEndTimeInMilliSeconds: " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
								cutPeriodEndTimeInMilliSeconds + ")";
							if (errorIfAChunkIsMissing)
							{
								SPDLOG_ERROR(string() + errorMessage);

								throw runtime_error(errorMessage);
							}
							else
							{
								_logger->warn(string() + errorMessage);
							}
						}
						else
						{
							firstRequestedChunk = true;
						}

						utcFirstChunkStartTime = currentUtcChunkStartTime;
						firstChunkStartTime = currentChunkStartTime;
					}

					{
						json mediaItemKeyReferenceRoot;

						field = "mediaItemKey";
						mediaItemKeyReferenceRoot[field] = mediaItemKey;

						mediaItemKeyReferencesRoot.push_back(mediaItemKeyReferenceRoot);
					}

					{
						// check if it is the last chunk

						if (!(currentUtcChunkStartTime * 1000 < utcCutPeriodEndTimeInMilliSecondsPlusOneSecond &&
							  utcCutPeriodEndTimeInMilliSecondsPlusOneSecond <= currentUtcChunkEndTime * 1000))
							lastRequestedChunk = false;
						else
						{
							lastRequestedChunk = true;
							utcLastChunkEndTime = currentUtcChunkEndTime;
							lastChunkEndTime = currentChunkEndTime;
						}
					}

					utcPreviousUtcChunkEndTime = currentUtcChunkEndTime;
				}

				start += rows;

				SPDLOG_INFO(
					string() + "Retrieving chunk" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", start: " + to_string(start) + ", rows: " + to_string(rows) +
					", mediaItemsRoot.size: " + to_string(mediaItemsRoot.size()) + ", lastRequestedChunk: " + to_string(lastRequestedChunk)
				);
			} while (mediaItemsRoot.size() == rows);

			// just waiting if the last chunk was not finished yet
			if (!lastRequestedChunk)
			{
				if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startLookingForChunks).count() <
					maxWaitingForLastChunkInSeconds)
				{
					int secondsToWaitLastChunk = 30;

					this_thread::sleep_for(chrono::seconds(secondsToWaitLastChunk));
				}
			}
		}

		if (!firstRequestedChunk || !lastRequestedChunk)
		{
			string errorMessage = string("#Chunks check. Chunks not available") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", firstRequestedChunk: " + to_string(firstRequestedChunk) +
								  ", lastRequestedChunk: " +
								  to_string(lastRequestedChunk)
								  // + ", streamSourceType: " + streamSourceType
								  // + ", ipConfigurationLabel: " + ipConfigurationLabel
								  // + ", satConfigurationLabel: " + satConfigurationLabel
								  + ", recordingCode: " + to_string(recordingCode) +
								  ", cutPeriodStartTimeInMilliSeconds: " + cutPeriodStartTimeInMilliSeconds +
								  ", cutPeriodEndTimeInMilliSeconds: " + cutPeriodEndTimeInMilliSeconds +
								  ", maxWaitingForLastChunkInSeconds: " + to_string(maxWaitingForLastChunkInSeconds);
			if (errorIfAChunkIsMissing)
			{
				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
			else
			{
				_logger->warn(string() + errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Preparing workflow to ingest..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		json liveCutOnSuccess = nullptr;
		json liveCutOnError = nullptr;
		json liveCutOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				json internalMMSRoot = liveCutParametersRoot[field];

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

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json eventsRoot = internalMMSRoot[field];

					field = "onSuccess";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnSuccess = eventsRoot[field];

					field = "onError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnError = eventsRoot[field];

					field = "onComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnComplete = eventsRoot[field];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		{
			json concatDemuxerRoot;
			json concatDemuxerParametersRoot;
			{
				string field = "label";
				concatDemuxerRoot[field] = "Concat from " + to_string(utcFirstChunkStartTime) + " (" + firstChunkStartTime + ") to " +
										   to_string(utcLastChunkEndTime) + " (" + lastChunkEndTime + ")";

				field = "type";
				concatDemuxerRoot[field] = "Concat-Demuxer";

				concatDemuxerParametersRoot = liveCutParametersRoot;
				{
					field = "recordingCode";
					concatDemuxerParametersRoot.erase(field);
				}

				{
					field = "cutPeriod";
					concatDemuxerParametersRoot.erase(field);
				}
				{
					field = "maxWaitingForLastChunkInSeconds";
					if (JSONUtils::isMetadataPresent(concatDemuxerParametersRoot, field))
					{
						concatDemuxerParametersRoot.erase(field);
					}
				}

				field = "retention";
				concatDemuxerParametersRoot[field] = "0";

				field = "references";
				concatDemuxerParametersRoot[field] = mediaItemKeyReferencesRoot;

				field = "parameters";
				concatDemuxerRoot[field] = concatDemuxerParametersRoot;
			}

			json cutRoot;
			{
				string field = "label";
				cutRoot[field] = string("Cut (Live) from ") + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
								 cutPeriodStartTimeInMilliSeconds + ") to " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
								 cutPeriodEndTimeInMilliSeconds + ")";

				field = "type";
				cutRoot[field] = "Cut";

				json cutParametersRoot = concatDemuxerParametersRoot;
				{
					field = "references";
					cutParametersRoot.erase(field);
				}

				field = "retention";
				cutParametersRoot[field] = JSONUtils::asString(liveCutParametersRoot, field, "");

				double startTimeInMilliSeconds = utcCutPeriodStartTimeInMilliSeconds - (utcFirstChunkStartTime * 1000);
				double startTimeInSeconds = startTimeInMilliSeconds / 1000;
				field = "startTime";
				cutParametersRoot[field] = startTimeInSeconds;

				double endTimeInMilliSeconds = utcCutPeriodEndTimeInMilliSeconds - (utcFirstChunkStartTime * 1000);
				double endTimeInSeconds = endTimeInMilliSeconds / 1000;
				field = "endTime";
				cutParametersRoot[field] = endTimeInSeconds;

				// 2020-07-19: keyFrameSeeking by default it is true.
				//	Result is that the cut is a bit over (in my test it was
				// about one second more). 	Using keyFrameSeeking false the Cut
				// is accurate.
				string cutType = "FrameAccurateWithoutEncoding";
				field = "cutType";
				cutParametersRoot[field] = cutType;

				bool fixEndTimeIfOvercomeDuration;
				if (!errorIfAChunkIsMissing)
					fixEndTimeIfOvercomeDuration = true;
				else
					fixEndTimeIfOvercomeDuration = false;
				field = "fixEndTimeIfOvercomeDuration";
				cutParametersRoot[field] = fixEndTimeIfOvercomeDuration;

				{
					json userDataRoot;

					field = "userData";
					if (JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
					{
						// to_string(static_cast<int>(liveCutParametersRoot[field].type()))
						// == 7 means objectValue
						//		(see Json::ValueType definition:
						// http://jsoncpp.sourceforge.net/value_8h_source.html)

						json::value_t valueType = liveCutParametersRoot[field].type();

						SPDLOG_INFO(
							string() + "Preparing workflow to ingest... (2)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", type: " + to_string(static_cast<int>(valueType))
						);

						if (valueType == json::value_t::string)
						{
							string sUserData = JSONUtils::asString(liveCutParametersRoot, field, "");

							if (sUserData != "")
								userDataRoot = JSONUtils::toJson(sUserData);
						}
						else // if (valueType == Json::ValueType::objectValue)
						{
							userDataRoot = liveCutParametersRoot[field];
						}
					}

					json mmsDataRoot;

					/*
					 * liveCutUtcStartTimeInMilliSecs and
					liveCutUtcEndTimeInMilliSecs was
					 * commented because:
					 *	1. they do not have the right name
					 *	2. LiveCut generates a workflow of
					 *		- concat of chunks --> cut of the concat
					 *		The Concat media will have the TimeCode because they
					are *		automatically generated by the task (see the
					concat method *		in this class) *		The Cut media
					will have the TimeCode because they are * automatically
					generated by the task (see the cut method *		in this
					class)

					field = "liveCutUtcStartTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodStartTimeInMilliSeconds;

					field = "liveCutUtcEndTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodEndTimeInMilliSeconds;
					*/

					/*
					field = "streamSourceType";
					mmsDataRoot[field] = streamSourceType;

					if (streamSourceType == "IP_PULL")
					{
						field = "configurationLabel";
						mmsDataRoot[field] = ipConfigurationLabel;
					}
					else if (streamSourceType == "Satellite")
					{
						field = "configurationLabel";
						mmsDataRoot[field] = satConfigurationLabel;
					}
					else // if (streamSourceType == "IP_PUSH")
					{
						field = "actAsServerChannelCode";
						mmsDataRoot[field] = actAsServerChannelCode;
					}
					*/
					field = "recordingCode";
					mmsDataRoot[field] = recordingCode;

					field = "mmsData";
					userDataRoot["mmsData"] = mmsDataRoot;

					field = "userData";
					cutParametersRoot[field] = userDataRoot;
				}

				field = "parameters";
				cutRoot[field] = cutParametersRoot;

				if (liveCutOnSuccess != nullptr)
				{
					field = "onSuccess";
					cutRoot[field] = liveCutOnSuccess;
				}
				if (liveCutOnError != nullptr)
				{
					field = "onError";
					cutRoot[field] = liveCutOnError;
				}
				if (liveCutOnComplete != nullptr)
				{
					field = "onComplete";
					cutRoot[field] = liveCutOnComplete;
				}
			}

			json concatOnSuccessRoot;
			{
				json cutTaskRoot;
				string field = "task";
				cutTaskRoot[field] = cutRoot;

				field = "onSuccess";
				concatDemuxerRoot[field] = cutTaskRoot;
			}

			json workflowRoot;
			{
				string field = "label";
				workflowRoot[field] = string("Cut from ") + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" + cutPeriodStartTimeInMilliSeconds +
									  ") to " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" + cutPeriodEndTimeInMilliSeconds + ")";

				field = "type";
				workflowRoot[field] = "Workflow";

				field = "task";
				workflowRoot[field] = concatDemuxerRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		string sResponse =
			MMSCURL::httpPostString(
				_logger, ingestionJobKey, _mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
				"application/json", // contentType
				otherHeaders
			)
				.second;

		// mancherebbe la parte aggiunta a LiveCut hls segmenter

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
}

void MMSEngineProcessor::manageLiveCutThread_hlsSegmenter(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace,
	json liveCutParametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			string() + "manageLiveCutThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		int64_t recordingCode;
		int64_t chunkEncodingProfileKey = -1;
		string chunkEncodingProfileLabel;
		string cutPeriodStartTimeInMilliSeconds;
		string cutPeriodEndTimeInMilliSeconds;
		int maxWaitingForLastChunkInSeconds = 90;
		bool errorIfAChunkIsMissing = false;
		{
			string field = "recordingCode";
			if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingCode = JSONUtils::asInt64(liveCutParametersRoot, field, -1);

			field = "chunkEncodingProfileKey";
			chunkEncodingProfileKey = JSONUtils::asInt64(liveCutParametersRoot, field, -1);

			field = "chunkEncodingProfileLabel";
			chunkEncodingProfileLabel = JSONUtils::asString(liveCutParametersRoot, field, "");

			field = "maxWaitingForLastChunkInSeconds";
			maxWaitingForLastChunkInSeconds = JSONUtils::asInt64(liveCutParametersRoot, field, 90);

			field = "errorIfAChunkIsMissing";
			errorIfAChunkIsMissing = JSONUtils::asBool(liveCutParametersRoot, field, false);

			field = "cutPeriod";
			json cutPeriodRoot = liveCutParametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodStartTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, field, "");

			field = "end";
			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodEndTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, field, "");
		}

		// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

		int64_t utcCutPeriodStartTimeInMilliSeconds = DateTime::sDateMilliSecondsToUtc(cutPeriodStartTimeInMilliSeconds);

		// next code is the same in the Validator class
		int64_t utcCutPeriodEndTimeInMilliSeconds = DateTime::sDateMilliSecondsToUtc(cutPeriodEndTimeInMilliSeconds);

		/*
		 * 2020-03-30: scenario: period end time is 300 seconds (5 minutes). In
		 * case the chunk is 1 minute, we will take 5 chunks. The result is that
		 * the Cut will fail because:
		 * - we need to cut to 300 seconds
		 * - the duration of the video is 298874 milliseconds
		 * For this reason, when we retrieve the chunks, we will use 'period end
		 * time' plus one second
		 */
		int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = utcCutPeriodEndTimeInMilliSeconds + 1000;

		json mediaItemKeyReferencesRoot = json::array();
		int64_t utcFirstChunkStartTimeInMilliSecs;
		string firstChunkStartTime;
		int64_t utcLastChunkEndTimeInMilliSecs;
		string lastChunkEndTime;

		chrono::system_clock::time_point startLookingForChunks = chrono::system_clock::now();

		bool firstRequestedChunk = false;
		bool lastRequestedChunk = false;
		while (!lastRequestedChunk && (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startLookingForChunks).count() <
									   maxWaitingForLastChunkInSeconds))
		{
			int64_t mediaItemKey = -1;
			int64_t physicalPathKey = -1;
			string uniqueName;
			vector<int64_t> otherMediaItemsKey;
			int start = 0;
			int rows = 60 * 1; // assuming every MediaItem is one minute, let's take 1 hour
			bool contentTypePresent = true;
			MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::ContentType::Video;
			// bool startAndEndIngestionDatePresent = false;
			string startIngestionDate;
			string endIngestionDate;
			string title;
			int liveRecordingChunk = 1;
			vector<string> tagsIn;
			vector<string> tagsNotIn;
			string orderBy;
			bool admin = false;

			firstRequestedChunk = false;
			lastRequestedChunk = false;

			string jsonCondition;
			/*
			{
				// SC: Start Chunk
				// PS: Playout Start, PE: Playout End
				//
			--------------SC--------------SC--------------SC--------------SC
				//                       PS-------------------------------PE
			}
			*/
			string jsonOrderBy;
			orderBy = "utcStartTimeInMilliSecs_virtual asc";

			long utcPreviousUtcChunkEndTimeInMilliSecs = -1;
			bool firstRetrievedChunk = true;

			// retrieve the reference of all the MediaItems to be concatenate
			mediaItemKeyReferencesRoot.clear();

			// json mediaItemsListRoot;
			json mediaItemsRoot;
			do
			{
				set<string> responseFields;
				json mediaItemsListRoot = _mmsEngineDBFacade->getMediaItemsList(
					workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey, start, rows, contentTypePresent,
					contentType,
					// startAndEndIngestionDatePresent,
					startIngestionDate, endIngestionDate, title, liveRecordingChunk, recordingCode, utcCutPeriodStartTimeInMilliSeconds,
					utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, tagsIn, tagsNotIn, orderBy, jsonOrderBy, responseFields, admin,
					// 2022-12-18: MIKs potrebbero essere stati appena
					// aggiunti
					true
				);

				string field = "response";
				json responseRoot = mediaItemsListRoot[field];

				field = "mediaItems";
				mediaItemsRoot = responseRoot[field];

				for (int mediaItemIndex = 0; mediaItemIndex < mediaItemsRoot.size(); mediaItemIndex++)
				{
					json mediaItemRoot = mediaItemsRoot[mediaItemIndex];

					field = "mediaItemKey";
					int64_t mediaItemKey = JSONUtils::asInt64(mediaItemRoot, field, 0);

					json userDataRoot;
					{
						field = "userData";
						string userData = JSONUtils::asString(mediaItemRoot, field, "");
						if (userData == "")
						{
							string errorMessage = string() + "recording media item without userData!!!" +
												  ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						userDataRoot = JSONUtils::toJson(userData);
					}

					field = "mmsData";
					json mmsDataRoot = userDataRoot[field];

					field = "utcStartTimeInMilliSecs";
					int64_t currentUtcChunkStartTimeInMilliSecs = JSONUtils::asInt64(mmsDataRoot, field, 0);

					field = "utcEndTimeInMilliSecs";
					int64_t currentUtcChunkEndTimeInMilliSecs = JSONUtils::asInt64(mmsDataRoot, field, 0);

					string currentChunkStartTime;
					string currentChunkEndTime;
					{
						char strDateTime[64];
						tm tmDateTime;

						int64_t currentUtcChunkStartTime = currentUtcChunkStartTimeInMilliSecs / 1000;
						localtime_r(&currentUtcChunkStartTime, &tmDateTime);
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, (int)(currentUtcChunkStartTimeInMilliSecs % 1000)
						);
						currentChunkStartTime = strDateTime;

						int64_t currentUtcChunkEndTime = currentUtcChunkEndTimeInMilliSecs / 1000;
						localtime_r(&currentUtcChunkEndTime, &tmDateTime);
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, (int)(currentUtcChunkEndTimeInMilliSecs % 1000)
						);
						currentChunkEndTime = strDateTime;
					}

					SPDLOG_INFO(
						string() + "Retrieved chunk" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaITemKey: " + to_string(mediaItemKey) +
						", currentUtcChunkStartTimeInMilliSecs: " + to_string(currentUtcChunkStartTimeInMilliSecs) + " (" + currentChunkStartTime +
						")" + ", currentUtcChunkEndTimeInMilliSecs: " + to_string(currentUtcChunkEndTimeInMilliSecs) + " (" + currentChunkEndTime +
						")"
					);

					// check if it is the next chunk
					if (utcPreviousUtcChunkEndTimeInMilliSecs != -1 && utcPreviousUtcChunkEndTimeInMilliSecs != currentUtcChunkStartTimeInMilliSecs)
					{
						string previousUtcChunkEndTime;
						{
							char strDateTime[64];
							tm tmDateTime;

							int64_t utcPreviousUtcChunkEndTime = utcPreviousUtcChunkEndTimeInMilliSecs / 1000;
							localtime_r(&utcPreviousUtcChunkEndTime, &tmDateTime);

							sprintf(
								strDateTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1,
								tmDateTime.tm_mday, tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec,
								(int)(utcPreviousUtcChunkEndTimeInMilliSecs % 1000)
							);
							previousUtcChunkEndTime = strDateTime;
						}

						// it is not the next chunk
						string errorMessage = string("Next chunk was not found") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", utcPreviousUtcChunkEndTimeInMilliSecs: " + to_string(utcPreviousUtcChunkEndTimeInMilliSecs) + " (" +
											  previousUtcChunkEndTime + ")" +
											  ", currentUtcChunkStartTimeInMilliSecs: " + to_string(currentUtcChunkStartTimeInMilliSecs) + " (" +
											  currentChunkStartTime + ")" +
											  ", currentUtcChunkEndTimeInMilliSecs: " + to_string(currentUtcChunkEndTimeInMilliSecs) + " (" +
											  currentChunkEndTime + ")" +
											  ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
											  cutPeriodStartTimeInMilliSeconds + ")" +
											  ", utcCutPeriodEndTimeInMilliSeconds: " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
											  cutPeriodEndTimeInMilliSeconds + ")";
						if (errorIfAChunkIsMissing)
						{
							SPDLOG_ERROR(string() + errorMessage);

							throw runtime_error(errorMessage);
						}
						else
						{
							_logger->warn(string() + errorMessage);
						}
					}

					// check if it is the first chunk
					if (firstRetrievedChunk)
					{
						firstRetrievedChunk = false;

						// check that it is the first chunk

						if (!(currentUtcChunkStartTimeInMilliSecs <= utcCutPeriodStartTimeInMilliSeconds &&
							  utcCutPeriodStartTimeInMilliSeconds < currentUtcChunkEndTimeInMilliSecs))
						{
							firstRequestedChunk = false;

							// it is not the first chunk
							string errorMessage = string("First chunk was not found") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) +
												  ", first utcChunkStartInMilliSecs: " + to_string(currentUtcChunkStartTimeInMilliSecs) + " (" +
												  currentChunkStartTime + ")" +
												  ", first currentUtcChunkEndTimeInMilliSecs: " + to_string(currentUtcChunkEndTimeInMilliSecs) +
												  " (" + currentChunkEndTime + ")" +
												  ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
												  cutPeriodStartTimeInMilliSeconds + ")" +
												  ", utcCutPeriodEndTimeInMilliSeconds: " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
												  cutPeriodEndTimeInMilliSeconds + ")";
							if (errorIfAChunkIsMissing)
							{
								SPDLOG_ERROR(string() + errorMessage);

								throw runtime_error(errorMessage);
							}
							else
							{
								_logger->warn(string() + errorMessage);
							}
						}
						else
						{
							firstRequestedChunk = true;
						}

						utcFirstChunkStartTimeInMilliSecs = currentUtcChunkStartTimeInMilliSecs;
						firstChunkStartTime = currentChunkStartTime;
					}

					{
						json mediaItemKeyReferenceRoot;

						field = "mediaItemKey";
						mediaItemKeyReferenceRoot[field] = mediaItemKey;

						if (chunkEncodingProfileKey != -1)
						{
							field = "encodingProfileKey";
							mediaItemKeyReferenceRoot[field] = chunkEncodingProfileKey;
						}
						else if (chunkEncodingProfileLabel != "")
						{
							field = "encodingProfileLabel";
							mediaItemKeyReferenceRoot[field] = chunkEncodingProfileLabel;
						}

						mediaItemKeyReferencesRoot.push_back(mediaItemKeyReferenceRoot);
					}

					{
						// check if it is the last chunk

						if (!(currentUtcChunkStartTimeInMilliSecs < utcCutPeriodEndTimeInMilliSecondsPlusOneSecond &&
							  utcCutPeriodEndTimeInMilliSecondsPlusOneSecond <= currentUtcChunkEndTimeInMilliSecs))
							lastRequestedChunk = false;
						else
						{
							lastRequestedChunk = true;
							utcLastChunkEndTimeInMilliSecs = currentUtcChunkEndTimeInMilliSecs;
							lastChunkEndTime = currentChunkEndTime;
						}
					}

					utcPreviousUtcChunkEndTimeInMilliSecs = currentUtcChunkEndTimeInMilliSecs;
				}

				start += rows;

				SPDLOG_INFO(
					string() + "Retrieving chunk" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", start: " + to_string(start) + ", rows: " + to_string(rows) +
					", mediaItemsRoot.size: " + to_string(mediaItemsRoot.size()) + ", lastRequestedChunk: " + to_string(lastRequestedChunk)
				);
			} while (mediaItemsRoot.size() == rows);

			// just waiting if the last chunk was not finished yet
			if (!lastRequestedChunk)
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (chrono::duration_cast<chrono::seconds>(now - startLookingForChunks).count() < maxWaitingForLastChunkInSeconds)
				{
					int secondsToWaitLastChunk = 15;

					SPDLOG_INFO(
						string() + "Sleeping to wait the last chunk..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) +
						", maxWaitingForLastChunkInSeconds: " + to_string(maxWaitingForLastChunkInSeconds) +
						", seconds passed: " + to_string(chrono::duration_cast<chrono::seconds>(now - startLookingForChunks).count()) +
						", secondsToWait before next check: " + to_string(secondsToWaitLastChunk)
					);

					this_thread::sleep_for(chrono::seconds(secondsToWaitLastChunk));
				}
			}
		}

		if (!firstRequestedChunk || !lastRequestedChunk)
		{
			string errorMessage = string("Chunks not available") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", firstRequestedChunk: " + to_string(firstRequestedChunk) +
								  ", lastRequestedChunk: " + to_string(lastRequestedChunk) + ", recordingCode: " + to_string(recordingCode) +
								  ", cutPeriodStartTimeInMilliSeconds: " + cutPeriodStartTimeInMilliSeconds +
								  ", cutPeriodEndTimeInMilliSeconds: " + cutPeriodEndTimeInMilliSeconds +
								  ", maxWaitingForLastChunkInSeconds: " + to_string(maxWaitingForLastChunkInSeconds);
			if (errorIfAChunkIsMissing)
			{
				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
			else
			{
				_logger->warn(string() + errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Preparing workflow to ingest..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		json liveCutOnSuccess = nullptr;
		json liveCutOnError = nullptr;
		json liveCutOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				json internalMMSRoot = liveCutParametersRoot[field];

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

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json eventsRoot = internalMMSRoot[field];

					field = "onSuccess";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnSuccess = eventsRoot[field];

					field = "onError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnError = eventsRoot[field];

					field = "onComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnComplete = eventsRoot[field];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		string cutLabel;
		{
			json concatDemuxerRoot;
			json concatDemuxerParametersRoot;
			{
				string field = "label";
				concatDemuxerRoot[field] = "Concat from " + to_string(utcFirstChunkStartTimeInMilliSecs) + " (" + firstChunkStartTime + ") to " +
										   to_string(utcLastChunkEndTimeInMilliSecs) + " (" + lastChunkEndTime + ")";

				field = "type";
				concatDemuxerRoot[field] = "Concat-Demuxer";

				concatDemuxerParametersRoot = liveCutParametersRoot;
				{
					field = "recordingCode";
					concatDemuxerParametersRoot.erase(field);
				}

				{
					field = "cutPeriod";
					concatDemuxerParametersRoot.erase(field);
				}
				{
					field = "maxWaitingForLastChunkInSeconds";
					if (JSONUtils::isMetadataPresent(concatDemuxerParametersRoot, field))
					{
						concatDemuxerParametersRoot.erase(field);
					}
				}

				field = "retention";
				concatDemuxerParametersRoot[field] = "0";

				field = "references";
				concatDemuxerParametersRoot[field] = mediaItemKeyReferencesRoot;

				field = "parameters";
				concatDemuxerRoot[field] = concatDemuxerParametersRoot;
			}

			json cutRoot;
			{
				string field = "label";
				cutLabel = string("Cut (Live) from ") + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" + cutPeriodStartTimeInMilliSeconds +
						   ") to " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" + cutPeriodEndTimeInMilliSeconds + ")";
				cutRoot[field] = cutLabel;

				field = "type";
				cutRoot[field] = "Cut";

				json cutParametersRoot = concatDemuxerParametersRoot;
				{
					field = "references";
					cutParametersRoot.erase(field);
				}

				field = "retention";
				cutParametersRoot[field] = JSONUtils::asString(liveCutParametersRoot, field, "");

				double startTimeInMilliSeconds = utcCutPeriodStartTimeInMilliSeconds - utcFirstChunkStartTimeInMilliSecs;
				double startTimeInSeconds = startTimeInMilliSeconds / 1000;
				if (startTimeInSeconds < 0)
					startTimeInSeconds = 0.0;
				field = "startTime";
				cutParametersRoot[field] = startTimeInSeconds;

				double endTimeInMilliSeconds = utcCutPeriodEndTimeInMilliSeconds - utcFirstChunkStartTimeInMilliSecs;
				double endTimeInSeconds = endTimeInMilliSeconds / 1000;
				field = "endTime";
				cutParametersRoot[field] = endTimeInSeconds;

				// 2020-07-19: keyFrameSeeking by default it is true.
				//	Result is that the cut is a bit over (in my test it was
				// about one second more). 	Using keyFrameSeeking false the Cut
				// is accurate because it could be a bframe too.
				string cutType = "FrameAccurateWithoutEncoding";
				field = "cutType";
				cutParametersRoot[field] = cutType;

				bool fixEndTimeIfOvercomeDuration;
				if (!errorIfAChunkIsMissing)
					fixEndTimeIfOvercomeDuration = true;
				else
					fixEndTimeIfOvercomeDuration = false;
				field = "fixEndTimeIfOvercomeDuration";
				cutParametersRoot[field] = fixEndTimeIfOvercomeDuration;

				{
					json userDataRoot;

					field = "userData";
					if (JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
					{
						// to_string(static_cast<int>(liveCutParametersRoot[field].type()))
						// == 7 means objectValue
						//		(see Json::ValueType definition:
						// http://jsoncpp.sourceforge.net/value_8h_source.html)

						json::value_t valueType = liveCutParametersRoot[field].type();

						SPDLOG_INFO(
							string() + "Preparing workflow to ingest... (2)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", type: " + to_string(static_cast<int>(valueType))
						);

						if (valueType == json::value_t::string)
						{
							string sUserData = JSONUtils::asString(liveCutParametersRoot, field, "");

							if (sUserData != "")
							{
								userDataRoot = JSONUtils::toJson(sUserData);
							}
						}
						else // if (valueType == Json::ValueType::objectValue)
						{
							userDataRoot = liveCutParametersRoot[field];
						}
					}

					json mmsDataRoot;

					/*
					 * liveCutUtcStartTimeInMilliSecs and
					liveCutUtcEndTimeInMilliSecs was
					 * commented because:
					 *	1. they do not have the right name
					 *	2. LiveCut generates a workflow of
					 *		- concat of chunks --> cut of the concat
					 *		The Concat media will have the TimeCode because they
					are *		automatically generated by the task (see the
					concat method *		in this class) *		The Cut media
					will have the TimeCode because they are * automatically
					generated by the task (see the cut method *		in this
					class)

					field = "liveCutUtcStartTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodStartTimeInMilliSeconds;

					field = "liveCutUtcEndTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodEndTimeInMilliSeconds;
					*/

					// field = "recordingCode";
					// mmsDataRoot[field] = recordingCode;

					// Per capire il motivo dell'aggiunta dei due campi liveCut
					// e ingestionJobKey, leggi il commento sotto (2023-08-10)
					// in particolare la parte "Per risolvere il problema nr. 2"
					json liveCutRoot;
					liveCutRoot["recordingCode"] = recordingCode;
					liveCutRoot["ingestionJobKey"] = (int64_t)(ingestionJobKey);
					mmsDataRoot["liveCut"] = liveCutRoot;

					field = "mmsData";
					userDataRoot["mmsData"] = mmsDataRoot;

					field = "userData";
					cutParametersRoot[field] = userDataRoot;
				}

				field = "parameters";
				cutRoot[field] = cutParametersRoot;

				if (liveCutOnSuccess != nullptr)
				{
					field = "onSuccess";
					cutRoot[field] = liveCutOnSuccess;
				}
				if (liveCutOnError != nullptr)
				{
					field = "onError";
					cutRoot[field] = liveCutOnError;
				}
				if (liveCutOnComplete != nullptr)
				{
					field = "onComplete";
					cutRoot[field] = liveCutOnComplete;
				}
			}

			json concatOnSuccessRoot;
			{
				json cutTaskRoot;
				string field = "task";
				cutTaskRoot[field] = cutRoot;

				field = "onSuccess";
				concatDemuxerRoot[field] = cutTaskRoot;
			}

			json workflowRoot;
			{
				string field = "label";
				workflowRoot[field] = ingestionJobLabel + ". Cut from " + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
									  cutPeriodStartTimeInMilliSeconds + ") to " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
									  cutPeriodEndTimeInMilliSeconds + ")";

				field = "type";
				workflowRoot[field] = "Workflow";

				field = "task";
				workflowRoot[field] = concatDemuxerRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		json workflowResponseRoot = MMSCURL::httpPostStringAndGetJson(
			_logger, ingestionJobKey, _mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
			"application/json", // contentType
			otherHeaders
		);

		/*
			2023-08-10
			Scenario: abbiamo il seguente workflow:
					GroupOfTask (ingestionJobKey: 5624319) composto da due
LiveCut (ingestionJobKey: 5624317 e 5624318) Concat dipende dal GroupOfTask per
concatenare i due file ottenuti dai due LiveCut L'engine esegue i due LiveCut
che creano ognuno un Workflow (Concat e poi Cut, vedi codice c++ sopra) Il
GroupOfTask, e quindi il Concat, finiti i due LiveCut, vengono eseguiti ma non
ricevono i due file perchè il LiveCut non ottiene il file ma crea un Workflow
per ottenere il file. La tabella MMS_IngestionJobDependency contiene le seguenti
due righe: mysql> select * from MMS_IngestionJobDependency where ingestionJobKey
= 5624319;
+---------------------------+-----------------+-----------------+-------------------------+-------------+---------------------------+
| ingestionJobDependencyKey | ingestionJobKey | dependOnSuccess |
dependOnIngestionJobKey | orderNumber | referenceOutputDependency |
+---------------------------+-----------------+-----------------+-------------------------+-------------+---------------------------+
|                   7315565 |         5624319 |               1 | 5624317 | 0 |
0 | |                   7315566 |         5624319 |               1 | 5624318 |
1 |                         0 |
+---------------------------+-----------------+-----------------+-------------------------+-------------+---------------------------+
2 rows in set (0.00 sec)

			In questo scenario abbiamo 2 problemi:
				1. Il GroupOfTask aspetta i due LiveCut mentre dovrebbe
aspettare i due Cut che generano i files e che si trovano all'interno dei due
workflow generati dai LiveCut
				2. GroupOfTask, al suo interno ha come referenceOutput gli
ingestionJobKey dei due LiveCut. Poichè i LiveCut non generano files (perchè i
files vengono generati dai due Cut), GroupOfTask non riceverà alcun file di
output

			Per risolvere il problema nr. 1:
			Per risolvere questo problema, prima che il LiveCut venga marcato
come End_TaskSuccess, bisogna aggiornare la tabella sopra per cambiare le
dipendenze, in sostanza tutti gli ingestionJobKey che dipendevano dal LiveCut,
dovranno dipendere dall'ingestionJobKey del Cut che è il Task che genera il file
all'interno del workflow creato dal LiveCut. Per questo motivo, dalla risposta
dell'ingestion del workflow, ci andiamo a recuperare l'ingestionJobKey del Cut
ed eseguiamo una update della tabella MMS_IngestionJobDependency

			Per risolvere il problema nr. 2:
			Come già accade in altri casi (LiveRecorder con i chunks) indichiamo
al Task Cut, di aggiungere il suo output anche come output del livecut. Questo
accade anche quando viene ingestato un Chunk che appare anche come output del
task Live-Recorder.
		*/
		{
			int64_t cutIngestionJobKey = -1;
			{
				if (!JSONUtils::isMetadataPresent(workflowResponseRoot, "tasks"))
				{
					string errorMessage = string("LiveCut workflow ingestion: wrong response, "
												 "tasks not found") +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", workflowResponseRoot: " + JSONUtils::toString(workflowResponseRoot);
					SPDLOG_ERROR(string() + errorMessage);

					throw runtime_error(errorMessage);
				}
				json tasksRoot = workflowResponseRoot["tasks"];
				for (int taskIndex = 0; taskIndex < tasksRoot.size(); taskIndex++)
				{
					json taskRoot = tasksRoot[taskIndex];
					string taskIngestionJobLabel = JSONUtils::asString(taskRoot, "label", "");
					if (taskIngestionJobLabel == cutLabel)
					{
						cutIngestionJobKey = JSONUtils::asInt64(taskRoot, "ingestionJobKey", -1);

						break;
					}
				}
				if (cutIngestionJobKey == -1)
				{
					string errorMessage = string("LiveCut workflow ingestion: wrong response, "
												 "cutLabel not found") +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", cutLabel: " + cutLabel +
										  ", workflowResponseRoot: " + JSONUtils::toString(workflowResponseRoot);
					SPDLOG_ERROR(string() + errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			SPDLOG_INFO(
				string() + "changeIngestionJobDependency" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", cutIngestionJobKey: " + to_string(cutIngestionJobKey)
			);
			_mmsEngineDBFacade->changeIngestionJobDependency(ingestionJobKey, cutIngestionJobKey);
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
}

void MMSEngineProcessor::youTubeLiveBroadcastThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			string() + "youTubeLiveBroadcastThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string youTubeConfigurationLabel;
		string youTubeLiveBroadcastTitle;
		string youTubeLiveBroadcastDescription;
		string youTubeLiveBroadcastPrivacyStatus;
		bool youTubeLiveBroadcastMadeForKids;
		string youTubeLiveBroadcastLatencyPreference;

		json scheduleRoot;
		string scheduleStartTimeInSeconds;
		string scheduleEndTimeInSeconds;
		string sourceType;
		// streamConfigurationLabel or referencesRoot has to be present
		string streamConfigurationLabel;
		json referencesRoot;
		{
			string field = "YouTubeConfigurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			youTubeConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "title";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			youTubeLiveBroadcastTitle = JSONUtils::asString(parametersRoot, field, "");

			field = "MadeForKids";
			youTubeLiveBroadcastMadeForKids = JSONUtils::asBool(parametersRoot, "MadeForKids", true);

			field = "Description";
			youTubeLiveBroadcastDescription = JSONUtils::asString(parametersRoot, field, "");

			field = "PrivacyStatus";
			youTubeLiveBroadcastPrivacyStatus = JSONUtils::asString(parametersRoot, field, "unlisted");

			field = "LatencyPreference";
			youTubeLiveBroadcastLatencyPreference = JSONUtils::asString(parametersRoot, field, "normal");

			field = "youTubeSchedule";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(scheduleRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleStartTimeInSeconds = JSONUtils::asString(scheduleRoot, field, "");

			field = "end";
			if (!JSONUtils::isMetadataPresent(scheduleRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleEndTimeInSeconds = JSONUtils::asString(scheduleRoot, field, "");

			field = "SourceType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceType = JSONUtils::asString(parametersRoot, field, "");

			if (sourceType == "Live")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				streamConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");
			}
			else // if (sourceType == "MediaItem")
			{
				field = "references";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				referencesRoot = parametersRoot[field];
			}
		}

		// 1. get refresh_token from the configuration
		// 2. call google API
		// 3. the response will have the access token to be used
		string youTubeAccessToken = getYouTubeAccessTokenByConfigurationLabel(ingestionJobKey, workspace, youTubeConfigurationLabel);

		string youTubeURL;
		string sResponse;
		string broadcastId;

		// first call, create the Live Broadcast
		try
		{
			/*
			* curl -v --request POST \
				'https://youtube.googleapis.com/youtube/v3/liveBroadcasts?part=snippet%2CcontentDetails%2Cstatus'
			\
				--header 'Authorization: Bearer
			ya29.a0ARrdaM9t2WqGKTgB9rZtoZU4oUCnW96Pe8qmgdk6ryYxEEe21T9WXWr8Eai1HX3AzG9zdOAEzRm8T6MhBmuQEDj4C5iDmfhRVjmUakhCKbZ7mWmqLOP9M6t5gha1QsH5ocNKqAZkhbCnWK0euQxGoK79MBjA'
			\
				--header 'Accept: application/json' \
				--header 'Content-Type: application/json' \
				--data '{"snippet":{"title":"Test
			CiborTV","scheduledStartTime":"2021-11-19T20:00:00.000Z","scheduledEndTime":"2021-11-19T22:00:00.000Z"},"contentDetails":{"enableClosedCaptions":true,"enableContentEncryption":true,"enableDvr":true,"enableEmbed":true,"recordFromStart":true,"startWithSlate":true},"status":{"privacyStatus":"unlisted"}}'
			\
				--compressed
			*/
			/*
				2023-02-23: L'account deve essere abilitato a fare live
			   streaming vedi: https://www.youtube.com/watch?v=wnjzdRpOIUA
			*/
			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPILiveBroadcastURI;

			string body;
			{
				json bodyRoot;

				{
					json snippetRoot;

					string field = "title";
					snippetRoot[field] = youTubeLiveBroadcastTitle;

					if (youTubeLiveBroadcastDescription != "")
					{
						field = "description";
						snippetRoot[field] = youTubeLiveBroadcastDescription;
					}

					if (streamConfigurationLabel != "")
					{
						field = "channelId";
						snippetRoot[field] = streamConfigurationLabel;
					}

					// scheduledStartTime
					{
						int64_t utcScheduleStartTimeInSeconds = DateTime::sDateSecondsToUtc(scheduleStartTimeInSeconds);

						// format: YYYY-MM-DDTHH:MI:SS.000Z
						string scheduleStartTimeInMilliSeconds = scheduleStartTimeInSeconds;
						scheduleStartTimeInMilliSeconds.insert(scheduleStartTimeInSeconds.length() - 1, ".000", 4);

						field = "scheduledStartTime";
						snippetRoot[field] = scheduleStartTimeInMilliSeconds;
					}

					// scheduledEndTime
					{
						int64_t utcScheduleEndTimeInSeconds = DateTime::sDateSecondsToUtc(scheduleEndTimeInSeconds);

						// format: YYYY-MM-DDTHH:MI:SS.000Z
						string scheduleEndTimeInMilliSeconds = scheduleEndTimeInSeconds;
						scheduleEndTimeInMilliSeconds.insert(scheduleEndTimeInSeconds.length() - 1, ".000", 4);

						field = "scheduledEndTime";
						snippetRoot[field] = scheduleEndTimeInMilliSeconds;
					}

					field = "snippet";
					bodyRoot[field] = snippetRoot;
				}

				{
					json contentDetailsRoot;

					bool enableContentEncryption = true;
					string field = "enableContentEncryption";
					contentDetailsRoot[field] = enableContentEncryption;

					bool enableDvr = true;
					field = "enableDvr";
					contentDetailsRoot[field] = enableDvr;

					bool enableEmbed = true;
					field = "enableEmbed";
					contentDetailsRoot[field] = enableEmbed;

					bool recordFromStart = true;
					field = "recordFromStart";
					contentDetailsRoot[field] = recordFromStart;

					bool startWithSlate = true;
					field = "startWithSlate";
					contentDetailsRoot[field] = startWithSlate;

					bool enableAutoStart = true;
					field = "enableAutoStart";
					contentDetailsRoot[field] = enableAutoStart;

					bool enableAutoStop = true;
					field = "enableAutoStop";
					contentDetailsRoot[field] = enableAutoStop;

					field = "latencyPreference";
					contentDetailsRoot[field] = youTubeLiveBroadcastLatencyPreference;

					field = "contentDetails";
					bodyRoot[field] = contentDetailsRoot;
				}

				{
					json statusRoot;

					string field = "privacyStatus";
					statusRoot[field] = youTubeLiveBroadcastPrivacyStatus;

					field = "selfDeclaredMadeForKids";
					statusRoot[field] = youTubeLiveBroadcastMadeForKids;

					field = "status";
					bodyRoot[field] = statusRoot;
				}

				body = JSONUtils::toString(bodyRoot);
			}

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Content-Length: " + to_string(body.length());
				headerList.push_back(header);

				header = "Accept: application/json";
				headerList.push_back(header);
			}

			json responseRoot = MMSCURL::httpPostStringAndGetJson(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"application/json", // contentType
				headerList
			);

			/* sResponse:
			HTTP/2 200
			content-type: application/json; charset=UTF-8
			vary: Origin
			vary: X-Origin
			vary: Referer
			content-encoding: gzip
			date: Sat, 20 Nov 2021 11:19:49 GMT
			server: scaffolding on HTTPServer2
			cache-control: private
			content-length: 858
			x-xss-protection: 0
			x-frame-options: SAMEORIGIN
			x-content-type-options: nosniff
			alt-svc: h3=":443"; ma=2592000,h3-29=":443";
			ma=2592000,h3-Q050=":443"; ma=2592000,h3-Q046=":443";
			ma=2592000,h3-Q043=":443"; ma=2592000,quic=":443"; ma=2592000;
			v="46,43"

			{
				"kind": "youtube#liveBroadcast",
				"etag": "AwdnruQBkHYB37w_2Rp6zWvtVbw",
				"id": "xdAak4DmKPI",
				"snippet": {
					"publishedAt": "2021-11-19T14:12:14Z",
					"channelId": "UC2WYB3NxVDD0mf-jML8qGAA",
					"title": "Test broadcast 2",
					"description": "",
					"thumbnails": {
						"default": {
							"url":
			"https://i.ytimg.com/vi/xdAak4DmKPI/default_live.jpg", "width": 120,
							"height": 90
						},
						"medium": {
							"url":
			"https://i.ytimg.com/vi/xdAak4DmKPI/mqdefault_live.jpg", "width":
			320, "height": 180
						},
						"high": {
							"url":
			"https://i.ytimg.com/vi/xdAak4DmKPI/hqdefault_live.jpg", "width":
			480, "height": 360
						}
					},
					"scheduledStartTime": "2021-11-19T16:00:00Z",
					"scheduledEndTime": "2021-11-19T17:00:00Z",
					"isDefaultBroadcast": false,
					"liveChatId":
			"KicKGFVDMldZQjNOeFZERDBtZi1qTUw4cUdBQRILeGRBYWs0RG1LUEk"
				},
				"status": {
					"lifeCycleStatus": "created",
					"privacyStatus": "unlisted",
					"recordingStatus": "notRecording",
					"madeForKids": false,
					"selfDeclaredMadeForKids": false
				},
				"contentDetails": {
					"monitorStream": {
					"enableMonitorStream": true,
					"broadcastStreamDelayMs": 0,
					"embedHtml": "\u003ciframe width=\"425\" height=\"344\"
			src=\"https://www.youtube.com/embed/xdAak4DmKPI?autoplay=1&livemonitor=1\"
			frameborder=\"0\" allow=\"accelerometer; autoplay; clipboard-write;
			encrypted-media; gyroscope; picture-in-picture\"
			allowfullscreen\u003e\u003c/iframe\u003e"
					},
					"enableEmbed": true,
					"enableDvr": true,
					"enableContentEncryption": true,
					"startWithSlate": true,
					"recordFromStart": true,
					"enableClosedCaptions": true,
					"closedCaptionsType": "closedCaptionsHttpPost",
					"enableLowLatency": false,
					"latencyPreference": "normal",
					"projection": "rectangular",
					"enableAutoStart": false,
					"enableAutoStop": false
				}
			}
			*/

			string field = "id";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			broadcastId = JSONUtils::asString(responseRoot, field, "");

			sResponse = "";
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("YouTube live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse +
								  ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("YouTube live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		string streamId;
		string rtmpURL;

		// second call, create the Live Stream
		try
		{
			/*
			* curl -v --request POST \
				'https://youtube.googleapis.com/youtube/v3/liveStreams?part=snippet%2Ccdn%2CcontentDetails%2Cstatus'
			\
				--header 'Authorization: Bearer
			ya29.a0ARrdaM9t2WqGKTgB9rZtoZU4oUCnW96Pe8qmgdk6ryYxEEe21T9WXWr8Eai1HX3AzG9zdOAEzRm8T6MhBmuQEDj4C5iDmfhRVjmUakhCKbZ7mWmqLOP9M6t5gha1QsH5ocNKqAZkhbCnWK0euQxGoK79MBjA'
			\
				--header 'Accept: application/json' \
				--header 'Content-Type: application/json' \
				--data '{"snippet":{"title":"my new video stream
			name","description":"A description of your video stream. This field
			is
			optional."},"cdn":{"frameRate":"60fps","ingestionType":"rtmp","resolution":"1080p"},"contentDetails":{"isReusable":true}}'
			\
				--compressed
			*/
			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPILiveStreamURI;

			string body;
			{
				json bodyRoot;

				{
					json snippetRoot;

					string field = "title";
					snippetRoot[field] = youTubeLiveBroadcastTitle;

					if (youTubeLiveBroadcastDescription != "")
					{
						field = "description";
						snippetRoot[field] = youTubeLiveBroadcastDescription;
					}

					if (streamConfigurationLabel != "")
					{
						field = "channelId";
						snippetRoot[field] = streamConfigurationLabel;
					}

					field = "snippet";
					bodyRoot[field] = snippetRoot;
				}

				{
					json cdnRoot;

					string field = "frameRate";
					cdnRoot[field] = "variable";

					field = "ingestionType";
					cdnRoot[field] = "rtmp";

					field = "resolution";
					cdnRoot[field] = "variable";

					field = "cdn";
					bodyRoot[field] = cdnRoot;
				}
				{
					json contentDetailsRoot;

					bool isReusable = true;
					string field = "isReusable";
					contentDetailsRoot[field] = isReusable;

					field = "contentDetails";
					bodyRoot[field] = contentDetailsRoot;
				}

				body = JSONUtils::toString(bodyRoot);
			}

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Content-Length: " + to_string(body.length());
				headerList.push_back(header);

				header = "Accept: application/json";
				headerList.push_back(header);
			}

			json responseRoot = MMSCURL::httpPostStringAndGetJson(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"application/json", // contentType
				headerList
			);

			/* sResponse:
			HTTP/2 200
			content-type: application/json; charset=UTF-8
			vary: Origin
			vary: X-Origin
			vary: Referer
			content-encoding: gzip
			date: Sat, 20 Nov 2021 11:19:49 GMT
			server: scaffolding on HTTPServer2
			cache-control: private
			content-length: 858
			x-xss-protection: 0
			x-frame-options: SAMEORIGIN
			x-content-type-options: nosniff
			alt-svc: h3=":443"; ma=2592000,h3-29=":443";
			ma=2592000,h3-Q050=":443"; ma=2592000,h3-Q046=":443";
			ma=2592000,h3-Q043=":443"; ma=2592000,quic=":443"; ma=2592000;
			v="46,43"

			{
				"kind": "youtube#liveStream",
				"etag": "MYZZfdTjQds1ghPCh_jyIjtsT9c",
				"id": "2WYB3NxVDD0mf-jML8qGAA1637335849431228",
				"snippet": {
					"publishedAt": "2021-11-19T15:30:49Z",
					"channelId": "UC2WYB3NxVDD0mf-jML8qGAA",
					"title": "my new video stream name",
					"description": "A description of your video stream. This
			field is optional.", "isDefaultStream": false
				},
				"cdn": {
					"ingestionType": "rtmp",
					"ingestionInfo": {
						"streamName": "py80-04jp-6jq3-eq29-407j",
						"ingestionAddress": "rtmp://a.rtmp.youtube.com/live2",
						"backupIngestionAddress":
			"rtmp://b.rtmp.youtube.com/live2?backup=1", "rtmpsIngestionAddress":
			"rtmps://a.rtmps.youtube.com/live2", "rtmpsBackupIngestionAddress":
			"rtmps://b.rtmps.youtube.com/live2?backup=1"
					},
					"resolution": "1080p",
					"frameRate": "60fps"
				},
				"status": {
					"streamStatus": "ready",
					"healthStatus": {
						"status": "noData"
					}
				},
				"contentDetails": {
					"closedCaptionsIngestionUrl":
			"http://upload.youtube.com/closedcaption?cid=py80-04jp-6jq3-eq29-407j",
					"isReusable": true
				}
			}
			*/

			string field = "id";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamId = JSONUtils::asString(responseRoot, field, "");

			field = "cdn";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json cdnRoot = responseRoot[field];

			field = "ingestionInfo";
			if (!JSONUtils::isMetadataPresent(cdnRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json ingestionInfoRoot = cdnRoot[field];

			field = "streamName";
			if (!JSONUtils::isMetadataPresent(ingestionInfoRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string streamName = JSONUtils::asString(ingestionInfoRoot, field, "");

			field = "ingestionAddress";
			if (!JSONUtils::isMetadataPresent(ingestionInfoRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string ingestionAddress = JSONUtils::asString(ingestionInfoRoot, field, "");

			rtmpURL = ingestionAddress + "/" + streamName;

			sResponse = "";
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse +
								  ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		// third call, bind live broadcast - live stream
		try
		{
			/*
			* curl -v --request POST \
				'https://youtube.googleapis.com/youtube/v3/liveBroadcasts/bind?id=xdAak4DmKPI&part=snippet&streamId=2WYB3NxVDD0mf-jML8qGAA1637335849431228'
			\
				--header 'Authorization: Bearer
			ya29.a0ARrdaM9t2WqGKTgB9rZtoZU4oUCnW96Pe8qmgdk6ryYxEEe21T9WXWr8Eai1HX3AzG9zdOAEzRm8T6MhBmuQEDj4C5iDmfhRVjmUakhCKbZ7mWmqLOP9M6t5gha1QsH5ocNKqAZkhbCnWK0euQxGoK79MBjA'
			\
				--header 'Accept: application/json' \
				--compressed
			*/
			string youTubeDataAPILiveBroadcastBindURI = regex_replace(_youTubeDataAPILiveBroadcastBindURI, regex("__BROADCASTID__"), broadcastId);
			youTubeDataAPILiveBroadcastBindURI = regex_replace(youTubeDataAPILiveBroadcastBindURI, regex("__STREAMID__"), streamId);

			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + youTubeDataAPILiveBroadcastBindURI;

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Accept: application/json";
				headerList.push_back(header);
			}

			string body;

			json responseRoot = MMSCURL::httpPostStringAndGetJson(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"", // contentType
				headerList
			);

			/* sResponse:
			HTTP/2 200 ^M
			content-type: application/json; charset=UTF-8^M
			vary: X-Origin^M
			vary: Referer^M
			vary: Origin,Accept-Encoding^M
			date: Wed, 24 Nov 2021 22:35:48 GMT^M
			server: scaffolding on HTTPServer2^M
			cache-control: private^M
			x-xss-protection: 0^M
			x-frame-options: SAMEORIGIN^M
			x-content-type-options: nosniff^M
			accept-ranges: none^M
			alt-svc: h3=":443"; ma=2592000,h3-29=":443";
			ma=2592000,h3-Q050=":443"; ma=2592000,h3-Q046=":443";
			ma=2592000,h3-Q043=":443"; ma=2592000,quic=":443"; ma=2592000;
			v="46,43"^M ^M
			{
			"kind": "youtube#liveBroadcast",
			"etag": "1NM7pffpR8009CHTdckGzn0rN-o",
			"id": "tP_L5RKFrQM",
			"snippet": {
				"publishedAt": "2021-11-24T22:35:46Z",
				"channelId": "UC2WYB3NxVDD0mf-jML8qGAA",
				"title": "test",
				"description": "",
				"thumbnails": {
				"default": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/default_live.jpg", "width": 120,
					"height": 90
				},
				"medium": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/mqdefault_live.jpg", "width":
			320, "height": 180
				},
				"high": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/hqdefault_live.jpg", "width":
			480, "height": 360
				},
				"standard": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/sddefault_live.jpg", "width":
			640, "height": 480
				}
				},
				"scheduledStartTime": "2021-11-24T22:25:00Z",
				"scheduledEndTime": "2021-11-24T22:50:00Z",
				"isDefaultBroadcast": false,
				"liveChatId":
			"KicKGFVDMldZQjNOeFZERDBtZi1qTUw4cUdBQRILdFBfTDVSS0ZyUU0"
			}
			}
			*/
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse +
								  ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			string() + "Preparing workflow to ingest..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		json youTubeLiveBroadcastOnSuccess = nullptr;
		json youTubeLiveBroadcastOnError = nullptr;
		json youTubeLiveBroadcastOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json internalMMSRoot = parametersRoot[field];

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

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json eventsRoot = internalMMSRoot[field];

					field = "onSuccess";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						youTubeLiveBroadcastOnSuccess = eventsRoot[field];

					field = "onError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						youTubeLiveBroadcastOnError = eventsRoot[field];

					field = "onComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						youTubeLiveBroadcastOnComplete = eventsRoot[field];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		{
			string proxyLabel;
			json proxyRoot;

			if (sourceType == "Live")
			{
				json liveProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "Proxy " + streamConfigurationLabel + " to YouTube (" + youTubeConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "Live-Proxy";

					liveProxyParametersRoot = parametersRoot;
					{
						field = "YouTubeConfigurationLabel";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "Description";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "SourceType";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}
					{
						field = "youTubeSchedule";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}

					{
						bool timePeriod = true;
						field = "timePeriod";
						liveProxyParametersRoot[field] = timePeriod;

						field = "schedule";
						liveProxyParametersRoot[field] = scheduleRoot;
					}

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// youTubeConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, youTubeConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (ConfKeyNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = youTubeConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					liveProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = liveProxyParametersRoot;
			}
			else // if (sourceType == "MediaItem")
			{
				json vodProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "VOD-Proxy MediaItem to YouTube (" + youTubeConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "VOD-Proxy";

					vodProxyParametersRoot = parametersRoot;
					{
						field = "YouTubeConfigurationLabel";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "Description";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "SourceType";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}
					{
						field = "youTubeSchedule";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}

					{
						bool timePeriod = true;
						field = "timePeriod";
						vodProxyParametersRoot[field] = timePeriod;

						field = "schedule";
						vodProxyParametersRoot[field] = scheduleRoot;
					}

					field = "references";
					vodProxyParametersRoot[field] = referencesRoot;

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// youTubeConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, youTubeConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (ConfKeyNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = youTubeConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					vodProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = vodProxyParametersRoot;
			}

			json workflowRoot;
			{
				string field = "label";
				workflowRoot[field] = ingestionJobLabel + ". " + proxyLabel;

				field = "type";
				workflowRoot[field] = "Workflow";

				field = "task";
				workflowRoot[field] = proxyRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		MMSCURL::httpPostString(
			_logger, ingestionJobKey, _mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
			"application/json", // contentType
			otherHeaders
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
}

void MMSEngineProcessor::facebookLiveBroadcastThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			string() + "facebookLiveBroadcastThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string facebookConfigurationLabel;
		string facebookNodeType;
		string facebookNodeId;
		string title;
		string description;
		string facebookLiveType;

		json scheduleRoot;
		int64_t utcScheduleStartTimeInSeconds;
		string sourceType;
		// configurationLabel or referencesRoot has to be present
		string configurationLabel;
		json referencesRoot;
		{
			string field = "facebookNodeType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeType = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookNodeId";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeId = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookLiveType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookLiveType = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookConfigurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "title";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			title = JSONUtils::asString(parametersRoot, field, "");

			field = "description";
			description = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookSchedule";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(scheduleRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string scheduleStartTimeInSeconds = JSONUtils::asString(scheduleRoot, field, "");
			utcScheduleStartTimeInSeconds = DateTime::sDateSecondsToUtc(scheduleStartTimeInSeconds);

			field = "sourceType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceType = JSONUtils::asString(parametersRoot, field, "");

			if (sourceType == "Live")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				configurationLabel = JSONUtils::asString(parametersRoot, field, "");
			}
			else // if (sourceType == "MediaItem")
			{
				field = "references";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				referencesRoot = parametersRoot[field];
			}
		}

		string facebookToken;
		if (facebookNodeType == "Page")
			facebookToken = getFacebookPageToken(ingestionJobKey, workspace, facebookConfigurationLabel, facebookNodeId);
		else // if (facebookNodeType == "User")
			facebookToken = _mmsEngineDBFacade->getFacebookUserAccessTokenByConfigurationLabel(workspace->_workspaceKey, facebookConfigurationLabel);
		/* 2023-01-08: capire se bisogna recuperare un altro tipo di token
		else if (facebookDestination == "Event")
		{
		}
		else // if (facebookDestination == "Group")
		{
		}
		*/

		string facebookURL;
		json responseRoot;
		string rtmpURL;
		try
		{
			/*
				curl -i -X POST \
					"https://graph.facebook.com/{page-id}/live_videos
					?status=LIVE_NOW
					&title=Today%27s%20Page%20Live%20Video
					&description=This%20is%20the%20live%20video%20for%20the%20Page%20for%20today
					&access_token=EAAC..."

				curl -i -X POST \
					"https://graph.facebook.com/{page-id}/live_videos
					?status=SCHEDULED_UNPUBLISHED
					&planned_start_time=1541539800
					&access_token={access-token}"
			*/
			facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIHostName + ":" + to_string(_facebookGraphAPIPort) + "/" +
						  _facebookGraphAPIVersion + regex_replace(_facebookGraphAPILiveVideosURI, regex("__NODEID__"), facebookNodeId) +
						  "?title=" + curlpp::escape(title) + (description != "" ? ("&description=" + curlpp::escape(description)) : "") +
						  "&access_token=" + curlpp::escape(facebookToken);
			if (facebookLiveType == "LiveNow")
			{
				facebookURL += "&status=LIVE_NOW";
			}
			else
			{
				facebookURL += (string("&status=SCHEDULED_UNPUBLISHED") + "&planned_start_time=" + to_string(utcScheduleStartTimeInSeconds));
			}

			SPDLOG_INFO(string() + "create a Live Video object" + ", facebookURL: " + facebookURL);

			vector<string> otherHeaders;
			json responseRoot =
				MMSCURL::httpPostStringAndGetJson(_logger, ingestionJobKey, facebookURL, _mmsAPITimeoutInSeconds, "", "", "", "", otherHeaders);

			/*
				{
				"id": "1953020644813108",
				"stream_url": "rtmp://rtmp-api.facebook...",
				"secure_stream_url":"rtmps://rtmp-api.facebook..."
				}
			*/

			string field = "secure_stream_url";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field + ", response: " + JSONUtils::toString(responseRoot);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			rtmpURL = JSONUtils::asString(responseRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("Facebook live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", facebookURL: " + facebookURL +
								  ", response: " + JSONUtils::toString(responseRoot) + ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("Facebook live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", facebookURL: " + facebookURL +
								  ", response: " + JSONUtils::toString(responseRoot);
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			string() + "Preparing workflow to ingest..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", rtmpURL: " + rtmpURL
		);

		json facebookLiveBroadcastOnSuccess = nullptr;
		json facebookLiveBroadcastOnError = nullptr;
		json facebookLiveBroadcastOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json internalMMSRoot = parametersRoot[field];

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

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json eventsRoot = internalMMSRoot[field];

					field = "onSuccess";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						facebookLiveBroadcastOnSuccess = eventsRoot[field];

					field = "onError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						facebookLiveBroadcastOnError = eventsRoot[field];

					field = "onComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						facebookLiveBroadcastOnComplete = eventsRoot[field];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		{
			string proxyLabel;
			json proxyRoot;

			if (sourceType == "Live")
			{
				json liveProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "Proxy " + configurationLabel + " to Facebook (" + facebookConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "Live-Proxy";

					liveProxyParametersRoot = parametersRoot;
					{
						field = "facebookConfigurationLabel";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "description";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "sourceType";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}
					{
						field = "facebookSchedule";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}

					{
						bool timePeriod = true;
						field = "timePeriod";
						liveProxyParametersRoot[field] = timePeriod;

						if (facebookLiveType == "LiveNow")
						{
							string sNow;
							{
								tm tmDateTime;
								char strDateTime[64];

								chrono::system_clock::time_point now = chrono::system_clock::now();
								time_t utcNow = chrono::system_clock::to_time_t(now);

								gmtime_r(&utcNow, &tmDateTime);
								sprintf(
									strDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1,
									tmDateTime.tm_mday, tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
								);
								sNow = strDateTime;
							}

							field = "start";
							scheduleRoot[field] = sNow;
						}

						field = "schedule";
						liveProxyParametersRoot[field] = scheduleRoot;
					}

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// facebookConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, facebookConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (ConfKeyNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = facebookConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					liveProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = liveProxyParametersRoot;
			}
			else // if (sourceType == "MediaItem")
			{
				json vodProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "Proxy MediaItem to Facebook (" + facebookConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "VOD-Proxy";

					vodProxyParametersRoot = parametersRoot;
					{
						field = "facebookConfigurationLabel";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "description";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "sourceType";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}
					{
						field = "facebookSchedule";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}

					field = "references";
					vodProxyParametersRoot[field] = referencesRoot;

					{
						bool timePeriod = true;
						field = "timePeriod";
						vodProxyParametersRoot[field] = timePeriod;

						if (facebookLiveType == "LiveNow")
						{
							field = "start";
							scheduleRoot[field] = utcScheduleStartTimeInSeconds;
						}

						field = "schedule";
						vodProxyParametersRoot[field] = scheduleRoot;
					}

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// facebookConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, facebookConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (ConfKeyNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = facebookConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					vodProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = vodProxyParametersRoot;
			}

			json workflowRoot;
			{
				string field = "label";
				workflowRoot[field] = ingestionJobLabel + ". " + proxyLabel;

				field = "type";
				workflowRoot[field] = "Workflow";

				field = "task";
				workflowRoot[field] = proxyRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		MMSCURL::httpPostString(
			_logger, ingestionJobKey, _mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
			"application/json", // contentType
			otherHeaders
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
}

void MMSEngineProcessor::copyContent(int64_t ingestionJobKey, string mmsAssetPathName, string localPath, string localFileName)
{

	try
	{
		SPDLOG_INFO(
			string() + "copyContent" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		string localPathName = localPath;
		if (localFileName != "")
		{
			string cleanedFileName;
			{
				cleanedFileName.resize(localFileName.size());
				transform(
					localFileName.begin(), localFileName.end(), cleanedFileName.begin(),
					[](unsigned char c)
					{
						if (c == '/')
							return (int)' ';
						else
							return (int)c;
					}
				);

				string fileFormat;
				{
					size_t extensionIndex = mmsAssetPathName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = string() +
											  "No fileFormat (extension of the file) found in "
											  "mmsAssetPathName" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
				}

				string suffix = "." + fileFormat;
				//if (cleanedFileName.size() >= suffix.size() &&
				//	0 == cleanedFileName.compare(cleanedFileName.size() - suffix.size(), suffix.size(), suffix))
				if (StringUtils::endWith(cleanedFileName, suffix))
					;
				else
					cleanedFileName += suffix;

				string prefix = "MMS ";
				cleanedFileName = prefix + cleanedFileName;
			}

			if (localPathName.back() != '/')
				localPathName += "/";
			localPathName += cleanedFileName;
		}

		SPDLOG_INFO(
			string() + "Coping" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", mmsAssetPathName: " + mmsAssetPathName + ", localPath: " + localPath + ", localFileName: " + localFileName +
			", localPathName: " + localPathName
		);

		fs::copy(mmsAssetPathName, localPathName, fs::copy_options::recursive);
	}
	catch (runtime_error &e)
	{
		string errorMessage = string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName +
							  ", localPath: " + localPath + ", localFileName: " + localFileName + ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName +
							  ", localPath: " + localPath + ", localFileName: " + localFileName + ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

void MMSEngineProcessor::handleMultiLocalAssetIngestionEventThread(
	shared_ptr<long> processorsThreadsNumber, MultiLocalAssetIngestionEvent multiLocalAssetIngestionEvent
)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleMultiLocalAssetIngestionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		multiLocalAssetIngestionEvent.getIngestionJobKey()
	);

	string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(multiLocalAssetIngestionEvent.getWorkspace());
	vector<string> generatedFramesFileNames;

	try
	{
		SPDLOG_INFO(
			string() + "handleMultiLocalAssetIngestionEventThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
			", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		// get files from file system
		{
			string generatedFrames_BaseFileName = to_string(multiLocalAssetIngestionEvent.getIngestionJobKey());

			for (fs::directory_entry const &entry : fs::directory_iterator(workspaceIngestionRepository))
			{
				try
				{
					if (!entry.is_regular_file())
						continue;

					if (entry.path().filename().string().size() >= generatedFrames_BaseFileName.size() &&
						0 == entry.path().filename().string().compare(0, generatedFrames_BaseFileName.size(), generatedFrames_BaseFileName))
						generatedFramesFileNames.push_back(entry.path().filename().string());
				}
				catch (runtime_error &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
				catch (exception &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
			}
		}

		// we have one ingestion job row and one or more generated frames to be
		// ingested One MIK in case of a .mjpeg One or more MIKs in case of .jpg
		// We want to update the ingestion row just once at the end,
		// in case of success or when an error happens.
		// To do this we will add a field in the localAssetIngestionEvent
		// structure (ingestionRowToBeUpdatedAsSuccess) and we will set it to
		// false except for the last frame where we will set to true In case of
		// error, handleLocalAssetIngestionEvent will update ingestion row and
		// we will not call anymore handleLocalAssetIngestionEvent for the next
		// frames When I say 'update the ingestion row', it's not just the
		// update but it is also manageIngestionJobStatusUpdate
		bool generatedFrameIngestionFailed = false;

		for (vector<string>::iterator it = generatedFramesFileNames.begin(); it != generatedFramesFileNames.end(); ++it)
		{
			string generatedFrameFileName = *it;

			if (generatedFrameIngestionFailed)
			{
				string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + generatedFrameFileName;

				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
					", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
				);
				fs::remove_all(workspaceIngestionBinaryPathName);
			}
			else
			{
				SPDLOG_INFO(
					string() + "Generated Frame to ingest" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
					to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", generatedFrameFileName: " + generatedFrameFileName
					// + ", textToBeReplaced: " + textToBeReplaced
					// + ", textToReplace: " + textToReplace
				);

				string fileFormat;
				size_t extensionIndex = generatedFrameFileName.find_last_of(".");
				if (extensionIndex == string::npos)
				{
					string errorMessage = string() +
										  "No fileFormat (extension of the file) found in "
										  "generatedFileName" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
										  ", generatedFrameFileName: " + generatedFrameFileName;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				fileFormat = generatedFrameFileName.substr(extensionIndex + 1);

				//            if (mmsSourceFileName.find(textToBeReplaced) !=
				//            string::npos)
				//                mmsSourceFileName.replace(mmsSourceFileName.find(textToBeReplaced),
				//                textToBeReplaced.length(), textToReplace);

				SPDLOG_INFO(
					string() + "Generated Frame to ingest" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
					", new generatedFrameFileName: " + generatedFrameFileName + ", fileFormat: " + fileFormat
				);

				string title;
				{
					string field = "title";
					if (JSONUtils::isMetadataPresent(multiLocalAssetIngestionEvent.getParametersRoot(), field))
						title = JSONUtils::asString(multiLocalAssetIngestionEvent.getParametersRoot(), field, "");
					title += (" (" + to_string(it - generatedFramesFileNames.begin() + 1) + " / " + to_string(generatedFramesFileNames.size()) + ")");
				}
				int64_t imageOfVideoMediaItemKey = -1;
				int64_t cutOfVideoMediaItemKey = -1;
				int64_t cutOfAudioMediaItemKey = -1;
				double startTimeInSeconds = 0.0;
				double endTimeInSeconds = 0.0;
				string imageMetaDataContent = generateMediaMetadataToIngest(
					multiLocalAssetIngestionEvent.getIngestionJobKey(),
					// mjpeg,
					fileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey, startTimeInSeconds, endTimeInSeconds,
					multiLocalAssetIngestionEvent.getParametersRoot()
				);

				{
					// shared_ptr<LocalAssetIngestionEvent>
					// localAssetIngestionEvent =
					// _multiEventsSet->getEventsFactory()
					//        ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);
					shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent = make_shared<LocalAssetIngestionEvent>();

					localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
					localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
					localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

					localAssetIngestionEvent->setExternalReadOnlyStorage(false);
					localAssetIngestionEvent->setIngestionJobKey(multiLocalAssetIngestionEvent.getIngestionJobKey());
					localAssetIngestionEvent->setIngestionSourceFileName(generatedFrameFileName);
					localAssetIngestionEvent->setMMSSourceFileName(generatedFrameFileName);
					localAssetIngestionEvent->setWorkspace(multiLocalAssetIngestionEvent.getWorkspace());
					localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
					localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(it + 1 == generatedFramesFileNames.end() ? true : false);

					localAssetIngestionEvent->setMetadataContent(imageMetaDataContent);

					try
					{
						handleLocalAssetIngestionEvent(processorsThreadsNumber, *localAssetIngestionEvent);
					}
					catch (runtime_error &e)
					{
						generatedFrameIngestionFailed = true;

						SPDLOG_ERROR(
							string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", exception: " + e.what()
						);
					}
					catch (exception &e)
					{
						generatedFrameIngestionFailed = true;

						SPDLOG_ERROR(
							string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", exception: " + e.what()
						);
					}

					//                    shared_ptr<Event2>    event =
					//                    dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
					//                    _multiEventsSet->addEvent(event);
					//
					//                    SPDLOG_INFO(string() + "addEvent:
					//                    EVENT_TYPE (INGESTASSETEVENT)"
					//                        + ", _processorIdentifier: " +
					//                        to_string(_processorIdentifier)
					//                        + ", ingestionJobKey: " +
					//                        to_string(ingestionJobKey)
					//                        + ", getEventKey().first: " +
					//                        to_string(event->getEventKey().first)
					//                        + ", getEventKey().second: " +
					//                        to_string(event->getEventKey().second));
				}
			}
		}

		/*
		if (generatedFrameIngestionFailed)
		{
			SPDLOG_INFO(string() + "updater->updateEncodingJob
		PunctualError"
				+ ", _encodingItem->_encodingJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
				+ ", _encodingItem->_ingestionJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
			);

			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			// PunctualError is used because, in case it always happens, the
		encoding will never reach a final state int encodingFailureNumber =
		updater->updateEncodingJob (
					multiLocalAssetIngestionEvent->getEncodingJobKey(),
					MMSEngineDBFacade::EncodingError::PunctualError,    //
		ErrorBeforeEncoding, mediaItemKey, encodedPhysicalPathKey,
					multiLocalAssetIngestionEvent->getIngestionJobKey());

			SPDLOG_INFO(string() + "updater->updateEncodingJob
		PunctualError"
				+ ", _encodingItem->_encodingJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
				+ ", _encodingItem->_ingestionJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
				+ ", encodingFailureNumber: " + to_string(encodingFailureNumber)
			);
		}
		else
		{
			SPDLOG_INFO(string() + "updater->updateEncodingJob NoError"
				+ ", _encodingItem->_encodingJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
				+ ", _encodingItem->_ingestionJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
			);

			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			updater->updateEncodingJob (
				multiLocalAssetIngestionEvent->getEncodingJobKey(),
				MMSEngineDBFacade::EncodingError::NoError,
				mediaItemKey, encodedPhysicalPathKey,
				multiLocalAssetIngestionEvent->getIngestionJobKey());
		}
		*/
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "handleMultiLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
			", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				multiLocalAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		for (vector<string>::iterator it = generatedFramesFileNames.begin(); it != generatedFramesFileNames.end(); ++it)
		{
			string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + *it;

			SPDLOG_INFO(
				string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
				", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
			);
			fs::remove_all(workspaceIngestionBinaryPathName);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "handleMultiLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey())
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
			", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				multiLocalAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		for (vector<string>::iterator it = generatedFramesFileNames.begin(); it != generatedFramesFileNames.end(); ++it)
		{
			string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + *it;

			SPDLOG_INFO(
				string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
				", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
			);
			fs::remove_all(workspaceIngestionBinaryPathName);
		}

		throw e;
	}
}

void MMSEngineProcessor::manageGenerateFramesTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType referenceContentType;
		string sourceAssetPathName;
		string sourceRelativePath;
		string sourceFileName;
		string sourceFileExtension;
		int64_t sourceDurationInMilliSecs;
		string sourcePhysicalDeliveryURL;
		string sourceTranscoderStagingAssetPathName;
		bool stopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
			sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
			stopIfReferenceProcessingError) = dependencyInfo;

		if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
		{
			string errorMessage = string() + "ContentTpe is not a Video" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int periodInSeconds;
		double startTimeInSeconds;
		int maxFramesNumber;
		string videoFilter;
		bool mjpeg;
		int imageWidth;
		int imageHeight;
		int64_t durationInMilliSeconds;
		fillGenerateFramesParameters(
			workspace, ingestionJobKey, ingestionType, parametersRoot, sourceMediaItemKey, sourcePhysicalPathKey,

			periodInSeconds, startTimeInSeconds, maxFramesNumber, videoFilter, mjpeg, imageWidth, imageHeight, durationInMilliSeconds
		);

		string transcoderStagingImagesDirectory; // used in case of external
												 // encoder
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			string directoryNameForFrames = to_string(ingestionJobKey)
				// + "_"
				// + to_string(encodingProfileKey)
				;
			transcoderStagingImagesDirectory = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				directoryNameForFrames,
				-1, // _encodingItem->_mediaItemKey, not used because
					// encodedFileName is not ""
				-1, // _encodingItem->_physicalPathKey, not used because
					// encodedFileName is not ""
				removeLinuxPathIfExist
			);
		}

		string nfsImagesDirectory = _mmsStorage->getWorkspaceIngestionRepository(workspace);

		_mmsEngineDBFacade->addEncoding_GenerateFramesJob(
			workspace, ingestionJobKey, encodingPriority, nfsImagesDirectory,
			transcoderStagingImagesDirectory,	  // used in case of external
												  // encoder
			sourcePhysicalDeliveryURL,			  // used in case of external encoder
			sourceTranscoderStagingAssetPathName, // used in case of external
												  // encoder
			sourceAssetPathName, sourcePhysicalPathKey, sourceFileExtension, sourceFileName, durationInMilliSeconds, startTimeInSeconds,
			maxFramesNumber, videoFilter, periodInSeconds, mjpeg, imageWidth, imageHeight, _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL,
			_mmsIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageGenerateFramesTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageGenerateFramesTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::fillGenerateFramesParameters(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot,
	int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,

	int &periodInSeconds, double &startTimeInSeconds, int &maxFramesNumber, string &videoFilter, bool &mjpeg, int &imageWidth, int &imageHeight,
	int64_t &durationInMilliSeconds
)
{
	try
	{
		string field;

		periodInSeconds = -1;
		{
			if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
			{
			}
			else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames)
			{
				field = "PeriodInSeconds";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				periodInSeconds = JSONUtils::asInt(parametersRoot, field, 0);
			}
			else // if (ingestionType ==
				 // MMSEngineDBFacade::IngestionType::IFrames || ingestionType
				 // == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
			{
			}
		}

		startTimeInSeconds = 0;
		{
			if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
			{
				field = "instantInSeconds";
				startTimeInSeconds = JSONUtils::asDouble(parametersRoot, field, 0);
			}
			else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::IFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
			{
				field = "startTimeInSeconds";
				startTimeInSeconds = JSONUtils::asDouble(parametersRoot, field, 0);
			}
		}

		maxFramesNumber = -1;
		{
			if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
			{
				maxFramesNumber = 1;
			}
			else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::IFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
			{
				field = "MaxFramesNumber";
				maxFramesNumber = JSONUtils::asInt(parametersRoot, field, -1);
			}
		}

		// 2021-09-14: default is set to true because often we have the error
		//	endTimeInSeconds is bigger of few milliseconds of the duration of
		// the media 	For this reason this field is set to true by default
		bool fixStartTimeIfOvercomeDuration = true;
		if (JSONUtils::isMetadataPresent(parametersRoot, "fixInstantInSecondsIfOvercomeDuration"))
			fixStartTimeIfOvercomeDuration = JSONUtils::asBool(parametersRoot, "fixInstantInSecondsIfOvercomeDuration", true);
		else if (JSONUtils::isMetadataPresent(parametersRoot, "FixStartTimeIfOvercomeDuration"))
			fixStartTimeIfOvercomeDuration = JSONUtils::asBool(parametersRoot, "FixStartTimeIfOvercomeDuration", true);

		{
			if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
			{
			}
			else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames)
			{
				videoFilter = "PeriodicFrame";
			}
			else if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames)
			{
				videoFilter = "All-I-Frames";
			}
		}

		{
			if (ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
				ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
			{
				mjpeg = true;
			}
			else
			{
				mjpeg = false;
			}
		}

		int width = -1;
		{
			field = "width";
			width = JSONUtils::asInt64(parametersRoot, field, -1);
		}

		int height = -1;
		{
			field = "height";
			height = JSONUtils::asInt(parametersRoot, field, -1);
		}

		int videoWidth;
		int videoHeight;
		try
		{
			vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
			vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

			_mmsEngineDBFacade->getVideoDetails(
				sourceMediaItemKey, sourcePhysicalPathKey,
				// 2022-12-18: MIK potrebbe essere stato appena aggiunto
				true, videoTracks, audioTracks
			);
			if (videoTracks.size() == 0)
			{
				string errorMessage = string() + "No video track are present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey);

				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack = videoTracks[0];

			tie(ignore, ignore, durationInMilliSeconds, videoWidth, videoHeight, ignore, ignore, ignore, ignore) = videoTrack;

			if (durationInMilliSeconds <= 0)
			{
				durationInMilliSeconds = _mmsEngineDBFacade->getMediaDurationInMilliseconds(
					sourceMediaItemKey, sourcePhysicalPathKey,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = string() + "_mmsEngineDBFacade->getVideoDetails failed" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", e.what(): " + e.what();

			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string() + "_mmsEngineDBFacade->getVideoDetails failed" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", e.what(): " + e.what();

			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		imageWidth = width == -1 ? videoWidth : width;
		imageHeight = height == -1 ? videoHeight : height;

		if (durationInMilliSeconds < startTimeInSeconds * 1000)
		{
			if (fixStartTimeIfOvercomeDuration)
			{
				double previousStartTimeInSeconds = startTimeInSeconds;
				startTimeInSeconds = durationInMilliSeconds / 1000;

				SPDLOG_INFO(
					string() + "startTimeInSeconds was changed to durationInMilliSeconds" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", fixStartTimeIfOvercomeDuration: " + to_string(fixStartTimeIfOvercomeDuration) + ", previousStartTimeInSeconds: " +
					to_string(previousStartTimeInSeconds) + ", new startTimeInSeconds: " + to_string(startTimeInSeconds)
				);
			}
			else
			{
				string errorMessage =
					string() +
					"Frame was not generated because instantInSeconds is "
					"bigger than the video duration" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) + ", startTimeInSeconds: " + to_string(startTimeInSeconds) +
					", durationInMilliSeconds: " + to_string(durationInMilliSeconds);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "fillGenerateFramesParameters failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "fillGenerateFramesParameters failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		throw e;
	}
}

void MMSEngineProcessor::manageSlideShowTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No images found" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot;
		{
			// This task shall contain EncodingProfileKey or
			// EncodingProfileLabel. We cannot have EncodingProfilesSetKey
			// because we replaced it with a GroupOfTasks
			//  having just EncodingProfileKey

			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				encodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
					workspace->_workspaceKey, MMSEngineDBFacade::ContentType::Video, encodingProfileLabel
				);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		json imagesRoot = json::array();
		json audiosRoot = json::array();
		float shortestAudioDurationInSeconds = -1.0;

		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			MMSEngineDBFacade::ContentType referenceContentType;
			int64_t sourceMediaItemKey;
			int64_t sourcePhysicalPathKey;
			string sourceAssetPathName;
			string sourceRelativePath;
			string sourceFileName;
			string sourceFileExtension;
			int64_t sourceDurationInMilliSecs;
			string sourcePhysicalDeliveryURL;
			string sourceTranscoderStagingAssetPathName;
			bool stopIfReferenceProcessingError;
			tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
				processDependencyInfo(workspace, ingestionJobKey, keyAndDependencyType);
			tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
				sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
				stopIfReferenceProcessingError) = dependencyInfo;

			if (referenceContentType == MMSEngineDBFacade::ContentType::Image)
			{
				json imageRoot;

				imageRoot["sourceAssetPathName"] = sourceAssetPathName;
				imageRoot["sourceFileExtension"] = sourceFileExtension;
				imageRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
				imageRoot["sourceTranscoderStagingAssetPathName"] = sourceTranscoderStagingAssetPathName;

				imagesRoot.push_back(imageRoot);
			}
			else if (referenceContentType == MMSEngineDBFacade::ContentType::Audio)
			{
				json audioRoot;

				audioRoot["sourceAssetPathName"] = sourceAssetPathName;
				audioRoot["sourceFileExtension"] = sourceFileExtension;
				audioRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
				audioRoot["sourceTranscoderStagingAssetPathName"] = sourceTranscoderStagingAssetPathName;

				audiosRoot.push_back(audioRoot);

				if (shortestAudioDurationInSeconds == -1.0 || shortestAudioDurationInSeconds > sourceDurationInMilliSecs / 1000)
					shortestAudioDurationInSeconds = sourceDurationInMilliSecs / 1000;
			}
			else
			{
				string errorMessage = string() +
									  "It is not possible to build a slideshow with a media that "
									  "is not an Image-Audio" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da .mp4
		string targetFileFormat = ".mp4";
		string encodedFileName = to_string(ingestionJobKey) + "_slideShow" + targetFileFormat;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
		}

		_mmsEngineDBFacade->addEncoding_SlideShowJob(
			workspace, ingestionJobKey, encodingProfileKey, encodingProfileDetailsRoot, targetFileFormat, imagesRoot, audiosRoot,
			shortestAudioDurationInSeconds, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL,
			_mmsBinaryIngestionURL, _mmsIngestionURL, encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageConcatThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "manageConcatThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "manageConcatThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() < 1)
		{
			string errorMessage = string() + "No enough media to be concatenated" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::ContentType concatContentType;
		bool concatContentTypeInitialized = false;
		vector<string> sourcePhysicalPaths;
		string forcedAvgFrameRate;

		// In case the first and the last chunk will have TimeCode, we will
		// report them in the new content
		int64_t utcStartTimeInMilliSecs = -1;
		int64_t utcEndTimeInMilliSecs = -1;
		bool firstMedia = true;
		string lastUserData;

		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			int64_t key;
			MMSEngineDBFacade::ContentType referenceContentType;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			SPDLOG_INFO(
				string() + "manageConcatThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", key: " + to_string(key)
			);

			int64_t sourceMediaItemKey;
			int64_t sourcePhysicalPathKey;
			string sourcePhysicalPath;
			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;
				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(sourcePhysicalPathKey, sourcePhysicalPath, ignore, ignore, ignore, ignore, ignore) =
					physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourceMediaItemKey = key;
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(sourcePhysicalPath, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourcePhysicalPathKey = key;

				bool warningIfMissing = false;
				tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

				tie(sourceMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}

			sourcePhysicalPaths.push_back(sourcePhysicalPath);

			bool warningIfMissing = false;
			tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemKeyDetails =
				_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
					workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);

			MMSEngineDBFacade::ContentType contentType;
			{
				int64_t localMediaItemKey;
				string localTitle;
				string ingestionDate;
				int64_t localIngestionJobKey;
				tie(localMediaItemKey, contentType, localTitle, lastUserData, ingestionDate, localIngestionJobKey, ignore, ignore, ignore) =
					mediaItemKeyDetails;
			}

			if (firstMedia)
			{
				firstMedia = false;

				if (lastUserData != "")
				{
					// try to retrieve time codes
					json sourceUserDataRoot = JSONUtils::toJson(lastUserData);

					string field = "mmsData";
					if (JSONUtils::isMetadataPresent(sourceUserDataRoot, field))
					{
						json sourceMmsDataRoot = sourceUserDataRoot[field];

						string utcStartTimeInMilliSecsField = "utcStartTimeInMilliSecs";
						// string utcChunkStartTimeField = "utcChunkStartTime";
						if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcStartTimeInMilliSecsField))
							utcStartTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcStartTimeInMilliSecsField, 0);
						/*
						else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
						utcChunkStartTimeField))
						{
							utcStartTimeInMilliSecs =
						JSONUtils::asInt64(sourceMmsDataRoot,
						utcChunkStartTimeField, 0); utcStartTimeInMilliSecs *=
						1000;
						}
						*/
					}
				}
			}

			if (!concatContentTypeInitialized)
			{
				concatContentType = contentType;
				if (concatContentType != MMSEngineDBFacade::ContentType::Video && concatContentType != MMSEngineDBFacade::ContentType::Audio)
				{
					string errorMessage = string() +
										  "It is not possible to concatenate a media that is not "
										  "video or audio" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", concatContentType: " + MMSEngineDBFacade::toString(concatContentType);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				if (concatContentType != contentType)
				{
					string errorMessage =
						string() + "Not all the References have the same ContentType" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", contentType: " + MMSEngineDBFacade::toString(contentType) +
						", concatContentType: " + MMSEngineDBFacade::toString(concatContentType);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			// to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate,
			// we will force the concat file to have the same avgFrameRate of
			// the source media
			if (concatContentType == MMSEngineDBFacade::ContentType::Video && forcedAvgFrameRate == "")
			{
				/*
				tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
				videoDetails =
				_mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey,
				sourcePhysicalPathKey);

				tie(ignore, ignore, ignore,
					ignore, ignore, ignore, forcedAvgFrameRate,
					ignore, ignore, ignore, ignore, ignore)
					= videoDetails;
				*/
				vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
				vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

				_mmsEngineDBFacade->getVideoDetails(
					sourceMediaItemKey, sourcePhysicalPathKey,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true, videoTracks, audioTracks
				);
				if (videoTracks.size() == 0)
				{
					string errorMessage = string() + "No video track are present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey);

					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack = videoTracks[0];

				tie(ignore, ignore, ignore, ignore, ignore, forcedAvgFrameRate, ignore, ignore, ignore) = videoTrack;
			}
		}

		SPDLOG_INFO(
			string() + "manageConcatThread, retrying time code" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", utcStartTimeInMilliSecs: " + to_string(utcStartTimeInMilliSecs) +
			", lastUserData: " + lastUserData
		);

		if (utcStartTimeInMilliSecs != -1)
		{
			if (lastUserData != "")
			{
				// try to retrieve time codes
				json sourceUserDataRoot = JSONUtils::toJson(lastUserData);

				string field = "mmsData";
				if (JSONUtils::isMetadataPresent(sourceUserDataRoot, field))
				{
					json sourceMmsDataRoot = sourceUserDataRoot[field];

					string utcEndTimeInMilliSecsField = "utcEndTimeInMilliSecs";
					// string utcChunkEndTimeField = "utcChunkEndTime";
					if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcEndTimeInMilliSecsField))
						utcEndTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcEndTimeInMilliSecsField, 0);
					/*
					else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
					utcChunkEndTimeField))
					{
						utcEndTimeInMilliSecs =
					JSONUtils::asInt64(sourceMmsDataRoot, utcChunkEndTimeField,
					0); utcEndTimeInMilliSecs *= 1000;
					}
					*/
				}
			}

			// utcStartTimeInMilliSecs and utcEndTimeInMilliSecs will be set in
			// parametersRoot
			if (utcStartTimeInMilliSecs != -1 && utcEndTimeInMilliSecs != -1)
			{
				json destUserDataRoot;

				/*
				{
					string json = JSONUtils::toString(parametersRoot);

					SPDLOG_INFO(string() + "manageConcatThread"
						+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", parametersRoot: " + json
					);
				}
				*/

				string field = "userData";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
					destUserDataRoot = parametersRoot[field];

				json destMmsDataRoot;

				field = "mmsData";
				if (JSONUtils::isMetadataPresent(destUserDataRoot, field))
					destMmsDataRoot = destUserDataRoot[field];

				field = "utcStartTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = utcStartTimeInMilliSecs;

				field = "utcEndTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = utcEndTimeInMilliSecs;

				// next statements will provoke an std::exception in case
				// parametersRoot -> UserData is a string (i.e.: "userData" :
				// "{\"matchId\": 363615, \"groupName\": \"CI\",
				//		\"homeTeamName\": \"Pescara Calcio\", \"awayTeamName\":
				//\"Olbia Calcio 1905\",
				//		\"start\": 1629398700000 }")
				//	and NOT a json

				field = "mmsData";
				destUserDataRoot[field] = destMmsDataRoot;

				field = "userData";
				parametersRoot[field] = destUserDataRoot;
			}
		}

		// this is a concat, so destination file name shall have the same
		// extension as the source file name
		string fileFormat;
		size_t extensionIndex = sourcePhysicalPaths.front().find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = string() +
								  "No fileFormat (extension of the file) found in "
								  "sourcePhysicalPath" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourcePhysicalPaths.front(): " + sourcePhysicalPaths.front();
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		fileFormat = sourcePhysicalPaths.front().substr(extensionIndex + 1);

		string localSourceFileName = to_string(ingestionJobKey) + "_concat" + "." + fileFormat // + "_source"
			;

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
		string concatenatedMediaPathName = workspaceIngestionRepository + "/" + localSourceFileName;

		if (sourcePhysicalPaths.size() == 1)
		{
			string sourcePhysicalPath = sourcePhysicalPaths.at(0);
			SPDLOG_INFO(
				string() + "Coping" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourcePhysicalPath: " + sourcePhysicalPath +
				", concatenatedMediaPathName: " + concatenatedMediaPathName
			);

			fs::copy(sourcePhysicalPath, concatenatedMediaPathName, fs::copy_options::recursive);
		}
		else
		{
			FFMpeg ffmpeg(_configurationRoot, _logger);
			ffmpeg.concat(
				ingestionJobKey, concatContentType == MMSEngineDBFacade::ContentType::Video ? true : false, sourcePhysicalPaths,
				concatenatedMediaPathName
			);
		}

		SPDLOG_INFO(
			string() + "generateConcatMediaToIngest done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName
		);

		double maxDurationInSeconds = 0.0;
		double extraSecondsToCutWhenMaxDurationIsReached = 0.0;
		string field = "MaxDurationInSeconds";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			maxDurationInSeconds = JSONUtils::asDouble(parametersRoot, field, 0.0);

			field = "ExtraSecondsToCutWhenMaxDurationIsReached";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				extraSecondsToCutWhenMaxDurationIsReached = JSONUtils::asDouble(parametersRoot, field, 0.0);

				if (extraSecondsToCutWhenMaxDurationIsReached >= abs(maxDurationInSeconds))
					extraSecondsToCutWhenMaxDurationIsReached = 0.0;
			}
		}
		SPDLOG_INFO(
			string() + "duration check" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", _ingestionJobKey: " + to_string(ingestionJobKey) + ", maxDurationInSeconds: " + to_string(maxDurationInSeconds) +
			", extraSecondsToCutWhenMaxDurationIsReached: " + to_string(extraSecondsToCutWhenMaxDurationIsReached)
		);
		if (maxDurationInSeconds != 0.0)
		{
			tuple<int64_t, long, json> mediaInfoDetails;
			vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
			vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
			int64_t durationInMilliSeconds;

			SPDLOG_INFO(
				string() + "Calling ffmpeg.getMediaInfo" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", _ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName
			);
			int timeoutInSeconds = 20;
			bool isMMSAssetPathName = true;
			FFMpeg ffmpeg(_configurationRoot, _logger);
			mediaInfoDetails =
				ffmpeg.getMediaInfo(ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, concatenatedMediaPathName, videoTracks, audioTracks);

			// tie(durationInMilliSeconds, ignore,
			//	ignore, ignore, ignore, ignore, ignore, ignore,
			//	ignore, ignore, ignore, ignore) = mediaInfo;
			tie(durationInMilliSeconds, ignore, ignore) = mediaInfoDetails;

			SPDLOG_INFO(
				string() + "duration check" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", _ingestionJobKey: " + to_string(ingestionJobKey) + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds) +
				", maxDurationInSeconds: " + to_string(maxDurationInSeconds) +
				", extraSecondsToCutWhenMaxDurationIsReached: " + to_string(extraSecondsToCutWhenMaxDurationIsReached)
			);
			if (durationInMilliSeconds > abs(maxDurationInSeconds) * 1000)
			{
				string localCutSourceFileName = to_string(ingestionJobKey) + "_concat_cut" + "." + fileFormat // + "_source"
					;

				string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
				string cutMediaPathName = workspaceIngestionRepository + "/" + localCutSourceFileName;

				string cutType = "KeyFrameSeeking";
				double startTimeInSeconds;
				double endTimeInSeconds;
				if (maxDurationInSeconds < 0.0)
				{
					startTimeInSeconds = ((durationInMilliSeconds / 1000) - (abs(maxDurationInSeconds) - extraSecondsToCutWhenMaxDurationIsReached));
					endTimeInSeconds = durationInMilliSeconds / 1000;
				}
				else
				{
					startTimeInSeconds = 0.0;
					endTimeInSeconds = maxDurationInSeconds - extraSecondsToCutWhenMaxDurationIsReached;
				}
				int framesNumber = -1;

				SPDLOG_INFO(
					string() + "Calling ffmpeg.cut" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", _ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName +
					", cutType: " + cutType + ", startTimeInSeconds: " + to_string(startTimeInSeconds) +
					", endTimeInSeconds: " + to_string(endTimeInSeconds) + ", framesNumber: " + to_string(framesNumber)
				);

				ffmpeg.cutWithoutEncoding(
					ingestionJobKey, concatenatedMediaPathName, concatContentType == MMSEngineDBFacade::ContentType::Video ? true : false, cutType,
					"", // startKeyFramesSeekingInterval,
					"", // endKeyFramesSeekingInterval,
					to_string(startTimeInSeconds), to_string(endTimeInSeconds), framesNumber, cutMediaPathName
				);

				SPDLOG_INFO(
					string() + "cut done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", cutMediaPathName: " + cutMediaPathName
				);

				localSourceFileName = localCutSourceFileName;

				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName
				);

				fs::remove_all(concatenatedMediaPathName);
			}
		}

		{
			string title;
			int64_t imageOfVideoMediaItemKey = -1;
			int64_t cutOfVideoMediaItemKey = -1;
			int64_t cutOfAudioMediaItemKey = -1;
			double startTimeInSeconds = 0.0;
			double endTimeInSeconds = 0.0;
			string mediaMetaDataContent = generateMediaMetadataToIngest(
				ingestionJobKey,
				// concatContentType == MMSEngineDBFacade::ContentType::Video ?
				// true : false,
				fileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey, startTimeInSeconds, endTimeInSeconds,
				parametersRoot
			);

			{
				shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
					_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
					);

				localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

				localAssetIngestionEvent->setExternalReadOnlyStorage(false);
				localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
				localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setWorkspace(workspace);
				localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
				localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

				// to manage a ffmpeg bug generating a corrupted/wrong
				// avgFrameRate, we will force the concat file to have the same
				// avgFrameRate of the source media
				if (forcedAvgFrameRate != "" && concatContentType == MMSEngineDBFacade::ContentType::Video)
					localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);

				localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

				shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
				_multiEventsSet->addEvent(event);

				SPDLOG_INFO(
					string() + "addEvent: EVENT_TYPE (INGESTASSETEVENT)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
					", getEventKey().second: " + to_string(event->getEventKey().second)
				);
			}
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::manageCutMediaThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "manageCutMediaThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "manageCutMediaThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong number of media to be cut" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType referenceContentType;
		string sourceAssetPathName;
		string sourceRelativePath;
		string sourceFileName;
		string sourceFileExtension;
		int64_t sourceDurationInMilliSecs;
		string sourcePhysicalDeliveryURL;
		string sourceTranscoderStagingAssetPathName;
		bool stopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
			sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
			stopIfReferenceProcessingError) = dependencyInfo;

		string userData;
		{
			bool warningIfMissing = false;

			tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemDetails =
				_mmsEngineDBFacade->getMediaItemKeyDetails(
					workspace->_workspaceKey, sourceMediaItemKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);

			tie(ignore, ignore, userData, ignore, ignore, ignore) = mediaItemDetails;
		}

		if (referenceContentType != MMSEngineDBFacade::ContentType::Video && referenceContentType != MMSEngineDBFacade::ContentType::Audio)
		{
			string errorMessage = string() + "It is not possible to cut a media that is not video or audio" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", contentType: " + MMSEngineDBFacade::toString(referenceContentType);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// abbiamo bisogno del source frame rate in un paio di casi sotto
		string forcedAvgFrameRate;
		int framesPerSecond = -1;
		{
			try
			{
				if (referenceContentType == MMSEngineDBFacade::ContentType::Video)
				{
					vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
					vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

					_mmsEngineDBFacade->getVideoDetails(
						sourceMediaItemKey, sourcePhysicalPathKey,
						// 2022-12-18: MIK potrebbe essere stato appena aggiunto
						true, videoTracks, audioTracks
					);
					if (videoTracks.size() == 0)
					{
						string errorMessage = string() + "No video track are present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey);

						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack = videoTracks[0];

					tie(ignore, ignore, ignore, ignore, ignore, forcedAvgFrameRate, ignore, ignore, ignore) = videoTrack;

					if (forcedAvgFrameRate != "")
					{
						// es: 25/1
						size_t index = forcedAvgFrameRate.find("/");
						if (index == string::npos)
							framesPerSecond = stoi(forcedAvgFrameRate);
						else
						{
							int frames = stoi(forcedAvgFrameRate.substr(0, index));
							int seconds = stoi(forcedAvgFrameRate.substr(index + 1));
							if (seconds != 0) // I saw: 0/0
								framesPerSecond = frames / seconds;
							SPDLOG_INFO(
								"forcedAvgFrameRate: {}"
								"frames: {}"
								"seconds: {}"
								"framesPerSecond: {}",
								forcedAvgFrameRate, frames, seconds, framesPerSecond
							);
						}
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "_mmsEngineDBFacade->getVideoDetails failed" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			catch (exception &e)
			{
				string errorMessage = string() + "_mmsEngineDBFacade->getVideoDetails failed" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "manageCutMediaThread frame rate" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", forcedAvgFrameRate" + forcedAvgFrameRate
		);

		// check start time / end time
		int framesNumber = -1;
		string startTime;
		string endTime = "0.0";
		{
			string field = "startTime";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			startTime = JSONUtils::asString(parametersRoot, field, "");

			field = "endTime";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				if (referenceContentType == MMSEngineDBFacade::ContentType::Audio)
				{
					// endTime in case of Audio is mandatory
					// because we cannot use the other option (FramesNumber)

					string errorMessage = string() +
										  "Field is not present or it is null, endTimeInSeconds "
										  "in case of Audio is mandatory because we cannot use "
										  "the other option (FramesNumber)" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			endTime = JSONUtils::asString(parametersRoot, field, "");

			if (referenceContentType == MMSEngineDBFacade::ContentType::Video)
			{
				field = "framesNumber";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
					framesNumber = JSONUtils::asInt(parametersRoot, field, 0);
			}

			// 2021-02-05: default is set to true because often we have the
			// error
			//	endTimeInSeconds is bigger of few milliseconds of the duration
			// of the media 	For this reason this field is set to true by
			// default
			bool fixEndTimeIfOvercomeDuration = true;
			field = "FixEndTimeIfOvercomeDuration";
			fixEndTimeIfOvercomeDuration = JSONUtils::asBool(parametersRoot, field, true);

			// startTime/endTime potrebbero avere anche il formato HH:MM:SS:FF.
			// Questo formato è stato utile per il cut di file mxf ma non è
			// supportato da ffmpeg (il formato supportati da ffmpeg sono quelli
			// gestiti da FFMpeg::timeToSeconds) Per cui qui riconduciamo il
			// formato HH:MM:SS:FF a quello gestito da ffmpeg HH:MM:SS.<decimi
			// di secondo>.
			{
				if (count_if(startTime.begin(), startTime.end(), [](char c) { return c == ':'; }) == 3)
				{
					int framesIndex = startTime.find_last_of(":");
					double frames = stoi(startTime.substr(framesIndex + 1));

					// se ad esempio sono 4 frames su 25 frames al secondo
					//	la parte decimale del secondo richiesta dal formato
					// ffmpeg sarà 16, 	cioè: (4/25)*100

					int decimals = (frames / ((double)framesPerSecond)) * 100;
					string newStartTime = startTime.substr(0, framesIndex) + "." + to_string(decimals);
					SPDLOG_INFO(
						"conversion from HH:MM:SS:FF to ffmeg format"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", startTime: {}"
						", newStartTime: {}",
						_processorIdentifier, ingestionJobKey, startTime, newStartTime
					);
					startTime = newStartTime;
				}
				if (count_if(endTime.begin(), endTime.end(), [](char c) { return c == ':'; }) == 3)
				{
					int framesIndex = endTime.find_last_of(":");
					double frames = stoi(endTime.substr(framesIndex + 1));

					// se ad esempio sono 4 frames su 25 frames al secondo
					//	la parte decimale del secondo richiesta dal formato
					// ffmpeg sarà 16, 	cioè: (4/25)*100

					int decimals = (frames / ((double)framesPerSecond)) * 100;
					string newEndTime = endTime.substr(0, framesIndex) + "." + to_string(decimals);
					SPDLOG_INFO(
						"conversion from HH:MM:SS:FF to ffmeg format"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", endTime: {}"
						", newEndTime: {}",
						_processorIdentifier, ingestionJobKey, endTime, newEndTime
					);
					endTime = newEndTime;
				}
			}
			double startTimeInSeconds = FFMpeg::timeToSeconds(ingestionJobKey, startTime).first;
			double endTimeInSeconds = FFMpeg::timeToSeconds(ingestionJobKey, endTime).first;

			field = "timesRelativeToMetaDataField";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string timesRelativeToMetaDataField = JSONUtils::asString(parametersRoot, field, "");

				string metaData;
				{
					bool warningIfMissing = false;

					metaData = _mmsEngineDBFacade->getPhysicalPathDetails(
						sourcePhysicalPathKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena aggiunto
						true
					);
				}

				if (metaData == "")
				{
					string errorMessage = fmt::format(
						"timesRelativeToMetaDataField cannot be applied "
						"because source media does not have metaData"
						", ingestionJobKey: {}"
						", sourcePhysicalPathKey: {}",
						ingestionJobKey, sourcePhysicalPathKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				json metaDataRoot = JSONUtils::toJson(metaData);

				string timeCode = JSONUtils::asString(metaDataRoot, timesRelativeToMetaDataField, "");
				if (timeCode == "")
				{
					string errorMessage = fmt::format(
						"timesRelativeToMetaDataField cannot be applied "
						"because source media has metaData but does not have "
						"the timecode"
						", ingestionJobKey: {}"
						", sourcePhysicalPathKey: {}"
						", metaData: {}"
						", timesRelativeToMetaDataField: {}",
						ingestionJobKey, sourcePhysicalPathKey, metaData, timesRelativeToMetaDataField
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				if (count_if(timeCode.begin(), timeCode.end(), [](char c) { return c == ':'; }) == 3)
				{
					int framesIndex = timeCode.find_last_of(":");
					double frames = stoi(timeCode.substr(framesIndex + 1));

					// se ad esempio sono 4 frames su 25 frames al secondo
					//	la parte decimale del secondo richiesta dal formato
					// ffmpeg sarà 16, 	cioè: (4/25)*100

					int decimals = (frames / ((double)framesPerSecond)) * 100;
					string newTimeCode = timeCode.substr(0, framesIndex) + "." + to_string(decimals);
					SPDLOG_INFO(
						"conversion from HH:MM:SS:FF to ffmeg format"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", timeCode: {}"
						", newTimeCode: {}",
						_processorIdentifier, ingestionJobKey, timeCode, newTimeCode
					);
					timeCode = newTimeCode;
				}

				long startTimeInCentsOfSeconds = FFMpeg::timeToSeconds(ingestionJobKey, startTime).second;
				long endTimeInCentsOfSeconds = FFMpeg::timeToSeconds(ingestionJobKey, endTime).second;
				long relativeTimeInCentsOfSeconds = FFMpeg::timeToSeconds(ingestionJobKey, timeCode).second;

				long newStartTimeInCentsOfSeconds = startTimeInCentsOfSeconds - relativeTimeInCentsOfSeconds;
				string newStartTime = FFMpeg::centsOfSecondsToTime(ingestionJobKey, newStartTimeInCentsOfSeconds);

				long newEndTimeInCentsOfSeconds = endTimeInCentsOfSeconds - relativeTimeInCentsOfSeconds;
				string newEndTime = FFMpeg::centsOfSecondsToTime(ingestionJobKey, newEndTimeInCentsOfSeconds);

				SPDLOG_INFO(
					"correction because of timesRelativeToMetaDataField"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", timeCode: {}"
					", relativeTimeInCentsOfSeconds: {}"
					", startTimeInCentsOfSeconds: {}"
					", startTime: {}"
					", newStartTime: {}"
					", endTimeInCentsOfSeconds: {}"
					", endTime: {}"
					", newEndTime: {}",
					_processorIdentifier, ingestionJobKey, timeCode, relativeTimeInCentsOfSeconds, startTimeInCentsOfSeconds, startTime, newStartTime,
					endTimeInCentsOfSeconds, endTime, newEndTime
				);

				startTimeInSeconds = newStartTimeInCentsOfSeconds / 100;
				startTime = newStartTime;

				endTimeInSeconds = newEndTimeInCentsOfSeconds / 100;
				endTime = newEndTime;
			}

			if (framesNumber == -1)
			{
				// il prossimo controllo possiamo farlo solo nel caso la stringa
				// non sia nel formato HH:MM:SS
				if (endTimeInSeconds < 0)
				{
					// if negative, it has to be subtract by the
					// durationInMilliSeconds
					double newEndTimeInSeconds = (sourceDurationInMilliSecs - (endTimeInSeconds * -1000)) / 1000;

					endTime = to_string(newEndTimeInSeconds);

					SPDLOG_ERROR(
						string() + "endTime was changed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
						", startTime: " + startTime + ", endTime: " + endTime + ", newEndTimeInSeconds: " + to_string(newEndTimeInSeconds) +
						", sourceDurationInMilliSecs: " + to_string(sourceDurationInMilliSecs)
					);
				}
			}

			if (startTimeInSeconds < 0.0)
			{
				startTime = "0.0";

				SPDLOG_INFO(
					string() + "startTime was changed to 0.0 because it is negative" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", fixEndTimeIfOvercomeDuration: " +
					to_string(fixEndTimeIfOvercomeDuration) + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
					", previousStartTimeInSeconds: " + to_string(startTimeInSeconds) + ", new startTime: " + startTime +
					", sourceDurationInMilliSecs (input media): " + to_string(sourceDurationInMilliSecs)
				);

				startTimeInSeconds = 0.0;
			}

			if (startTimeInSeconds > endTimeInSeconds)
			{
				string errorMessage =
					string() +
					"Cut was not done because startTimeInSeconds is bigger "
					"than endTimeInSeconds" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) + ", startTimeInSeconds: " + to_string(startTimeInSeconds) +
					", endTimeInSeconds: " + to_string(endTimeInSeconds) + ", sourceDurationInMilliSecs: " + to_string(sourceDurationInMilliSecs);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (framesNumber == -1)
			{
				if (sourceDurationInMilliSecs < endTimeInSeconds * 1000)
				{
					if (fixEndTimeIfOvercomeDuration)
					{
						endTime = to_string(sourceDurationInMilliSecs / 1000);

						SPDLOG_INFO(
							string() +
							"endTimeInSeconds was changed to "
							"durationInMilliSeconds" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", fixEndTimeIfOvercomeDuration: " + to_string(fixEndTimeIfOvercomeDuration) +
							", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
							", previousEndTimeInSeconds: " + to_string(endTimeInSeconds) + ", new endTimeInSeconds: " + endTime +
							", sourceDurationInMilliSecs (input media): " + to_string(sourceDurationInMilliSecs)
						);
					}
					else
					{
						string errorMessage =
							string() +
							"Cut was not done because endTimeInSeconds is "
							"bigger than durationInMilliSeconds (input media)" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
							", startTimeInSeconds: " + to_string(startTimeInSeconds) + ", endTimeInSeconds: " + to_string(endTimeInSeconds) +
							", sourceDurationInMilliSecs (input media): " + to_string(sourceDurationInMilliSecs);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}

		int64_t newUtcStartTimeInMilliSecs = -1;
		int64_t newUtcEndTimeInMilliSecs = -1;
		{
			// In case the media has TimeCode, we will report them in the new
			// content
			if (framesNumber == -1 && userData != "")
			{
				// try to retrieve time codes
				json sourceUserDataRoot = JSONUtils::toJson(userData);

				int64_t utcStartTimeInMilliSecs = -1;
				int64_t utcEndTimeInMilliSecs = -1;

				string field = "mmsData";
				if (JSONUtils::isMetadataPresent(sourceUserDataRoot, field))
				{
					json sourceMmsDataRoot = sourceUserDataRoot[field];

					string utcStartTimeInMilliSecsField = "utcStartTimeInMilliSecs";
					// string utcChunkStartTimeField = "utcChunkStartTime";
					if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcStartTimeInMilliSecsField))
						utcStartTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcStartTimeInMilliSecsField, 0);
					/*
					else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
					utcChunkStartTimeField))
					{
						utcStartTimeInMilliSecs =
					JSONUtils::asInt64(sourceMmsDataRoot,
					utcChunkStartTimeField, 0); utcStartTimeInMilliSecs *= 1000;
					}
					*/

					if (utcStartTimeInMilliSecs != -1)
					{
						string utcEndTimeInMilliSecsField = "utcEndTimeInMilliSecs";
						// string utcChunkEndTimeField = "utcChunkEndTime";
						if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcEndTimeInMilliSecsField))
							utcEndTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcEndTimeInMilliSecsField, 0);
						/*
						else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
						utcChunkEndTimeField))
						{
							utcEndTimeInMilliSecs =
						JSONUtils::asInt64(sourceMmsDataRoot,
						utcChunkEndTimeField, 0); utcEndTimeInMilliSecs *= 1000;
						}
						*/

						// utcStartTimeInMilliSecs and utcEndTimeInMilliSecs
						// will be set in parametersRoot
						if (utcStartTimeInMilliSecs != -1 && utcEndTimeInMilliSecs != -1)
						{
							newUtcStartTimeInMilliSecs = utcStartTimeInMilliSecs;
							newUtcStartTimeInMilliSecs += ((int64_t)(FFMpeg::timeToSeconds(ingestionJobKey, startTime).second * 10));
							newUtcEndTimeInMilliSecs =
								utcStartTimeInMilliSecs + ((int64_t)(FFMpeg::timeToSeconds(ingestionJobKey, endTime).second * 10));
						}
					}
				}
			}
		}

		string field = "cutType";
		string cutType = JSONUtils::asString(parametersRoot, field, "KeyFrameSeeking");

		SPDLOG_INFO(
			string() + "manageCutMediaThread new start/end" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", cutType: " + cutType + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
			", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey) + ", sourceAssetPathName: " + sourceAssetPathName +
			", sourceDurationInMilliSecs: " + to_string(sourceDurationInMilliSecs) + ", framesNumber: " + to_string(framesNumber) +
			", startTime: " + startTime + ", endTime: " + endTime + ", newUtcStartTimeInMilliSecs: " + to_string(newUtcStartTimeInMilliSecs) +
			", newUtcEndTimeInMilliSecs: " + to_string(newUtcEndTimeInMilliSecs)
		);
		if (cutType == "KeyFrameSeeking" || cutType == "FrameAccurateWithoutEncoding" || cutType == "KeyFrameSeekingInterval")
		{
			string outputFileFormat;
			field = "outputFileFormat";
			outputFileFormat = JSONUtils::asString(parametersRoot, field, "");

			SPDLOG_INFO(
				string() + "1 manageCutMediaThread new start/end" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey)
			);

			// this is a cut so destination file name shall have the same
			// extension as the source file name
			string fileFormat;
			if (outputFileFormat == "")
			{
				string sourceFileExtensionWithoutDot = sourceFileExtension.size() > 0 ? sourceFileExtension.substr(1) : sourceFileExtension;

				if (sourceFileExtensionWithoutDot == "m3u8")
					fileFormat = "ts";
				else
					fileFormat = sourceFileExtensionWithoutDot;
			}
			else
			{
				fileFormat = outputFileFormat;
			}

			SPDLOG_INFO(
				string() + "manageCutMediaThread file format" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", fileFormat: " + fileFormat
			);

			if (newUtcStartTimeInMilliSecs != -1 && newUtcEndTimeInMilliSecs != -1)
			{
				json destUserDataRoot;

				field = "userData";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
					destUserDataRoot = parametersRoot[field];

				json destMmsDataRoot;

				field = "mmsData";
				if (JSONUtils::isMetadataPresent(destUserDataRoot, field))
					destMmsDataRoot = destUserDataRoot[field];

				field = "utcStartTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = newUtcStartTimeInMilliSecs;

				field = "utcStartTimeInMilliSecs_str";
				{
					time_t newUtcStartTimeInSecs = newUtcStartTimeInMilliSecs / 1000;
					// i.e.: 2021-02-26T15:41:15Z
					string utcToUtcString = DateTime::utcToUtcString(newUtcStartTimeInSecs);
					utcToUtcString.insert(utcToUtcString.size() - 1, "." + to_string(newUtcStartTimeInMilliSecs % 1000));
					destMmsDataRoot[field] = utcToUtcString;
				}

				field = "utcEndTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = newUtcEndTimeInMilliSecs;

				field = "utcEndTimeInMilliSecs_str";
				{
					time_t newUtcEndTimeInSecs = newUtcEndTimeInMilliSecs / 1000;
					// i.e.: 2021-02-26T15:41:15Z
					string utcToUtcString = DateTime::utcToUtcString(newUtcEndTimeInSecs);
					utcToUtcString.insert(utcToUtcString.size() - 1, "." + to_string(newUtcEndTimeInMilliSecs % 1000));
					destMmsDataRoot[field] = utcToUtcString;
				}

				field = "mmsData";
				destUserDataRoot[field] = destMmsDataRoot;

				field = "userData";
				parametersRoot[field] = destUserDataRoot;
			}

			SPDLOG_INFO(
				string() + "manageCutMediaThread user data management" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey)
			);

			string localSourceFileName = to_string(ingestionJobKey) + "_cut" + "." + fileFormat // + "_source"
				;

			string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
			string cutMediaPathName = workspaceIngestionRepository + "/" + localSourceFileName;

			FFMpeg ffmpeg(_configurationRoot, _logger);
			ffmpeg.cutWithoutEncoding(
				ingestionJobKey, sourceAssetPathName, referenceContentType == MMSEngineDBFacade::ContentType::Video ? true : false, cutType,
				"", // startKeyFramesSeekingInterval,
				"", // endKeyFramesSeekingInterval,
				startTime, endTime, framesNumber, cutMediaPathName
			);

			SPDLOG_INFO(
				string() + "cut done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", cutMediaPathName: " + cutMediaPathName
			);

			string title;
			int64_t imageOfVideoMediaItemKey = -1;
			int64_t cutOfVideoMediaItemKey = -1;
			int64_t cutOfAudioMediaItemKey = -1;
			if (referenceContentType == MMSEngineDBFacade::ContentType::Video)
				cutOfVideoMediaItemKey = sourceMediaItemKey;
			else if (referenceContentType == MMSEngineDBFacade::ContentType::Audio)
				cutOfAudioMediaItemKey = sourceMediaItemKey;
			string mediaMetaDataContent = generateMediaMetadataToIngest(
				ingestionJobKey, fileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey,
				FFMpeg::timeToSeconds(ingestionJobKey, startTime).first, FFMpeg::timeToSeconds(ingestionJobKey, endTime).first, parametersRoot
			);

			{
				shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
					_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
					);

				localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

				localAssetIngestionEvent->setExternalReadOnlyStorage(false);
				localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
				localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setWorkspace(workspace);
				localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
				localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
				// to manage a ffmpeg bug generating a corrupted/wrong
				// avgFrameRate, we will force the concat file to have the same
				// avgFrameRate of the source media
				if (forcedAvgFrameRate != "" && referenceContentType == MMSEngineDBFacade::ContentType::Video)
					localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);

				localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

				shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
				_multiEventsSet->addEvent(event);

				SPDLOG_INFO(
					string() + "addEvent: EVENT_TYPE (LOCALASSETINGESTIONEVENT)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
					", getEventKey().second: " + to_string(event->getEventKey().second)
				);
			}
		}
		else
		{
			// FrameAccurateWithEncoding

			MMSEngineDBFacade::EncodingPriority encodingPriority;
			string field = "encodingPriority";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
			else
				encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

			int64_t encodingProfileKey;
			json encodingProfileDetailsRoot;
			{
				string keyField = "encodingProfileKey";
				string labelField = "encodingProfileLabel";
				if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
				{
					encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
				}
				else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
				{
					string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

					MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
					encodingProfileKey =
						_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
				}
				else
				{
					string errorMessage = string() + "Both fields are not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + keyField +
										  ", Field: " + labelField;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				{
					string jsonEncodingProfile;

					tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
						_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
					tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

					encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
				}
			}

			string encodedFileName;
			{
				/*
				string fileFormat =
				JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat",
				""); string fileFormatLowerCase;
				fileFormatLowerCase.resize(fileFormat.size());
				transform(fileFormat.begin(), fileFormat.end(),
				fileFormatLowerCase.begin(),
					[](unsigned char c){return tolower(c); } );
				*/

				encodedFileName = to_string(ingestionJobKey) + "_" + to_string(encodingProfileKey) +
								  getEncodedFileExtensionByEncodingProfile(encodingProfileDetailsRoot); // "." + fileFormatLowerCase
				;
			}

			string encodedTranscoderStagingAssetPathName; // used in case of
														  // external encoder
			fs::path encodedNFSStagingAssetPathName;
			{
				bool removeLinuxPathIfExist = false;
				bool neededForTranscoder = true;

				encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder,
					workspace->_directoryName,	// workspaceDirectoryName
					to_string(ingestionJobKey), // directoryNamePrefix
					"/",						// relativePath,
					// as specified by doc
					// (TASK_01_Add_Content_JSON_Format.txt), in case of hls
					// and external encoder (binary is ingested through
					// PUSH), the directory inside the tar.gz has to be
					// 'content'
					encodedFileName, // content
					-1,				 // _encodingItem->_mediaItemKey, not used because
									 // encodedFileName is not ""
					-1,				 // _encodingItem->_physicalPathKey, not used because
									 // encodedFileName is not ""
					removeLinuxPathIfExist
				);

				encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
			}

			_mmsEngineDBFacade->addEncoding_CutFrameAccurate(
				workspace, ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey, sourceAssetPathName, sourceDurationInMilliSecs,
				sourceFileExtension, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName, endTime, encodingProfileKey,
				encodingProfileDetailsRoot, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL,
				_mmsBinaryIngestionURL, _mmsIngestionURL, encodingPriority, newUtcStartTimeInMilliSecs, newUtcEndTimeInMilliSecs
			);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);

		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::manageEncodeTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No media received to be encoded" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::ContentType contentType;
		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			tie(ignore, contentType, ignore, ignore) = keyAndDependencyType;
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		{
			string field = "encodingPriority";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
			}
			else
			{
				encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
			}
		}

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot;
		{
			// This task shall contain EncodingProfileKey or
			// EncodingProfileLabel. We cannot have EncodingProfilesSetKey
			// because we replaced it with a GroupOfTasks
			//  having just EncodingProfileKey

			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				encodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, contentType, encodingProfileLabel);
			}
			else
			{
				string errorMessage = string() + "Both fields are not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + keyField +
									  ", Field: " + labelField;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		// it is not possible to manage more than one encode because:
		// 1. inside _mmsEngineDBFacade->addEncodingJob, the ingestionJob is
		// updated to encodingQueue
		//		and the second call will fail (because the update of the
		// ingestion was already done
		//	2. The ingestionJob mantains the status of the encoding, how would
		// be managed 		the status in case of more than one encoding?
		//	3. EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg
		// saves 		encoder URL and staging directory into database to
		// recover the scenario 		where the engine reboot.
		// 2021-06-07: we have the need to manage more than one encoding.
		//	For example, we have the I-Frame task and, on success, we want to
		// encode 	all the images generated by the I-Frames task. 	In this
		// scenario the Encode task receives a lot of images as input. 	Solution
		// no. 1: 		we manage all the inputs sequentially (as it is doing
		// the RemoveContent task). 		This is not a good solution because,
		// in case of the Encode task and in case of videos, 		every
		// encoding would take a lot of time. Manage all these encodings
		// sequentially 		is not what the User expect to see. 	Solution
		// no.
		// 2: 		we can create one EncodingJob for each encoding. This is
		// exactly
		// what the User expects 		because the encodings will run in
		// parallel.
		//
		//		Issue 1: How to manage the ingestionJob status in case of
		// multiple encodings? 		Issue 2: GUI and API are planned to manage
		// one EncodingJob for each IngestionJob
		//
		//	2021-08-25: In case the Encode Task is received by the MMS with
		// multiple References 	as input during the ingestion, it will be
		// automatically converted with a 	GroupOfTasks with all the Encode
		// Tasks as children (just done). 	The problem is when the input
		// references are generated dinamically as output 	of the parent task.
		// We will manage this issue ONLY in case of images doing 	the encoding
		// sequentially. 	For video/audio we cannot manage it sequentially
		// (like images) mainly because 	the encoder URL and the staging
		// directory are saved into the database 	to manage the recovering in
		// case of reboot of the Engine. 	This recovery is very important and
		// I
		// do not know how to manage it 	in case the task has in his queue a
		// list of encodings to do!!! 	We should save into DB also the specific
		// encoding it is doing?!?!??! 	Also che encoding progress would not
		// have sense in the "sequential/queue" scenario
		json sourcesToBeEncodedRoot = json::array();
		{
			// 2022-12-10: next for and the sourcesToBeEncodedRoot structure
			// sarebbe inutile per
			//	l'encoding di video/audio ma serve invece per l'encoding di
			// picture
			for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
			{
				bool stopIfReferenceProcessingError;
				int64_t sourceMediaItemKey;
				int64_t sourcePhysicalPathKey;
				string mmsSourceAssetPathName;
				string sourcePhysicalDeliveryURL;
				string sourceFileName;
				int64_t sourceDurationInMilliSecs;
				string sourceRelativePath;
				string sourceFileExtension;
				json videoTracksRoot = json::array();
				json audioTracksRoot = json::array();
				string sourceTranscoderStagingAssetPathName;
				string encodedTranscoderStagingAssetPathName; // used in case of
															  // external encoder
				string encodedNFSStagingAssetPathName;
				MMSEngineDBFacade::ContentType referenceContentType;
				try
				{
					tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool>
						dependencyInfo = processDependencyInfo(workspace, ingestionJobKey, keyAndDependencyType);
					tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, mmsSourceAssetPathName, sourceRelativePath, sourceFileName,
						sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
						stopIfReferenceProcessingError) = dependencyInfo;

					if (contentType != referenceContentType)
					{
						string errorMessage = string() + "Wrong content type" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", contentType: " + MMSEngineDBFacade::toString(contentType) +
											  ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					// check if the profile is already present for the source
					// content
					{
						try
						{
							bool warningIfMissing = true;
							int64_t localPhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
								sourceMediaItemKey, encodingProfileKey, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

							string errorMessage =
								string() + "Content profile is already present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
								", encodingProfileKey: " + to_string(encodingProfileKey);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
						catch (MediaItemKeyNotFound &e)
						{
						}
					}

					string encodedFileName;
					string fileFormat;
					{
						fileFormat = JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");

						encodedFileName = to_string(ingestionJobKey) + "_" + to_string(encodingProfileKey) +
										  getEncodedFileExtensionByEncodingProfile(encodingProfileDetailsRoot);

						/*
						if (fileFormat == "hls" || fileFormat == "dash")
						{
							;
						}
						else
						{
							encodedFileName.append(".");
							encodedFileName.append(fileFormat);
						}
						*/
					}

					{
						bool removeLinuxPathIfExist = false;
						bool neededForTranscoder = true;
						encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
							neededForTranscoder,
							workspace->_directoryName,	// workspaceDirectoryName
							to_string(ingestionJobKey), // directoryNamePrefix
							"/",						// relativePath,
							// as specified by doc
							// (TASK_01_Add_Content_JSON_Format.txt), in
							// case of hls the directory inside the tar.gz
							// has to be 'content'
							(fileFormat == "hls" || fileFormat == "dash") ? "content" : encodedFileName,
							-1, // _encodingItem->_mediaItemKey, not used
								// because encodedFileName is not ""
							-1, // _encodingItem->_physicalPathKey, not used
								// because encodedFileName is not ""
							removeLinuxPathIfExist
						);

						neededForTranscoder = false;
						encodedNFSStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
							neededForTranscoder,
							workspace->_directoryName,	// workspaceDirectoryName
							to_string(ingestionJobKey), // directoryNamePrefix
							"/",						// relativePath,
							encodedFileName,			// fileName
							-1,							// _encodingItem->_mediaItemKey, not used
														// because encodedFileName is not ""
							-1,							// _encodingItem->_physicalPathKey, not used
														// because encodedFileName is not ""
							removeLinuxPathIfExist
						);
					}

					{
						if (contentType == MMSEngineDBFacade::ContentType::Video)
						{
							vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							// int64_t sourceMediaItemKey = -1;
							_mmsEngineDBFacade->getVideoDetails(
								-1, sourcePhysicalPathKey,
								// 2022-12-18: MIK potrebbe essere stato appena
								// aggiunto
								true, videoTracks, audioTracks
							);

							for (tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack : videoTracks)
							{
								int trackIndex;
								tie(ignore, trackIndex, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = videoTrack;

								if (trackIndex != -1)
								{
									json videoTrackRoot;

									string field = "trackIndex";
									videoTrackRoot[field] = trackIndex;

									videoTracksRoot.push_back(videoTrackRoot);
								}
							}

							for (tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack : audioTracks)
							{
								int trackIndex;
								string language;
								tie(ignore, trackIndex, ignore, ignore, ignore, ignore, ignore, language) = audioTrack;

								if (trackIndex != -1 && language != "")
								{
									json audioTrackRoot;

									string field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.push_back(audioTrackRoot);
								}
							}
						}
						else if (contentType == MMSEngineDBFacade::ContentType::Audio)
						{
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							// int64_t sourceMediaItemKey = -1;
							_mmsEngineDBFacade->getAudioDetails(
								-1, sourcePhysicalPathKey,
								// 2022-12-18: MIK potrebbe essere stato appena
								// aggiunto
								true, audioTracks
							);

							for (tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack : audioTracks)
							{
								int trackIndex;
								string language;
								tie(ignore, trackIndex, ignore, ignore, ignore, ignore, ignore, language) = audioTrack;

								if (trackIndex != -1 && language != "")
								{
									json audioTrackRoot;

									string field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.push_back(audioTrackRoot);
								}
							}
						}
					}

					json sourceRoot;

					string field = "stopIfReferenceProcessingError";
					sourceRoot[field] = stopIfReferenceProcessingError;

					field = "sourceMediaItemKey";
					sourceRoot[field] = sourceMediaItemKey;

					field = "sourcePhysicalPathKey";
					sourceRoot[field] = sourcePhysicalPathKey;

					field = "mmsSourceAssetPathName";
					sourceRoot[field] = mmsSourceAssetPathName;

					field = "sourcePhysicalDeliveryURL";
					sourceRoot[field] = sourcePhysicalDeliveryURL;

					field = "sourceDurationInMilliSecs";
					sourceRoot[field] = sourceDurationInMilliSecs;

					field = "sourceFileName";
					sourceRoot[field] = sourceFileName;

					field = "sourceRelativePath";
					sourceRoot[field] = sourceRelativePath;

					field = "sourceFileExtension";
					sourceRoot[field] = sourceFileExtension;

					field = "videoTracks";
					sourceRoot[field] = videoTracksRoot;

					field = "audioTracks";
					sourceRoot[field] = audioTracksRoot;

					field = "sourceTranscoderStagingAssetPathName";
					sourceRoot[field] = sourceTranscoderStagingAssetPathName;

					field = "encodedTranscoderStagingAssetPathName";
					sourceRoot[field] = encodedTranscoderStagingAssetPathName;

					field = "encodedNFSStagingAssetPathName";
					sourceRoot[field] = encodedNFSStagingAssetPathName;

					sourcesToBeEncodedRoot.push_back(sourceRoot);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						string() + "processing media input failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", referenceContentType: " +
						MMSEngineDBFacade::toString(referenceContentType) + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
					);

					if (stopIfReferenceProcessingError)
						throw e;
				}
			}
		}

		if (sourcesToBeEncodedRoot.size() == 0)
		{
			// dependecies.size() > 0 perchè è stato già verificato inizialmente
			// Se sourcesToBeEncodedRoot.size() == 0 vuol dire che
			// l'encodingProfileKey era già presente
			//	per il MediaItem

			string errorMessage = string() + "Content profile is already present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		_mmsEngineDBFacade->addEncodingJob(
			workspace, ingestionJobKey, contentType, encodingPriority, encodingProfileKey, encodingProfileDetailsRoot,

			sourcesToBeEncodedRoot,

			_mmsWorkflowIngestionURL, _mmsBinaryIngestionURL, _mmsIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageEncodeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageEncodeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageVideoSpeedTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong media number to be encoded" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		else
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType referenceContentType;
		string sourceAssetPathName;
		string sourceRelativePath;
		string sourceFileName;
		string sourceFileExtension;
		int64_t sourceDurationInMilliSeconds;
		string sourcePhysicalDeliveryURL;
		string sourceTranscoderStagingAssetPathName;
		bool stopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
			sourceFileExtension, sourceDurationInMilliSeconds, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
			stopIfReferenceProcessingError) = dependencyInfo;

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			// This task shall contain EncodingProfileKey or
			// EncodingProfileLabel. We cannot have EncodingProfilesSetKey
			// because we replaced it with a GroupOfTasks
			//  having just EncodingProfileKey

			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, referenceContentType, encodingProfileLabel);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		// Since it was a copy and past, next commant has to be checked.
		// It is not possible to manage more than one encode because:
		// 1. inside _mmsEngineDBFacade->addEncodingJob, the ingestionJob is
		// updated to encodingQueue
		//		and the second call will fail (because the update of the
		// ingestion was already done
		//	2. The ingestionJob mantains the status of the encoding, how would
		// be managed 		the status in case of more than one encoding?
		// for
		// (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>&
		// 		keyAndDependencyType: dependencies)

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da
		// sourceFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_videoSpeed" + sourceFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
		}

		// 2021-08-26: si dovrebbe cambiare l'implementazione:
		//	aggiungere la gestione di multi-video-speed: l'encoding dovrebbe
		// eseguire il processing in modo sequenziale e utilizzare il bool
		//	stopIfReferenceProcessingError per decidere se interrompere in caso
		// di errore
		_mmsEngineDBFacade->addEncoding_VideoSpeed(
			workspace, ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey, sourceAssetPathName, sourceDurationInMilliSeconds,
			sourceFileExtension, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName, encodingProfileKey, encodingProfileDetailsRoot,
			encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL,
			_mmsIngestionURL, encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageAddSilentAudioTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "Wrong media number to be encoded" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		else
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			// This task shall contain EncodingProfileKey or
			// EncodingProfileLabel. We cannot have EncodingProfilesSetKey
			// because we replaced it with a GroupOfTasks
			//  having just EncodingProfileKey

			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				MMSEngineDBFacade::ContentType referenceContentType;

				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, referenceContentType, encodingProfileLabel, false);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		json sourcesRoot = json::array();
		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> dependency : dependencies)
		{
			MMSEngineDBFacade::ContentType referenceContentType;
			int64_t sourceMediaItemKey;
			int64_t sourcePhysicalPathKey;
			string sourceAssetPathName;
			string sourceRelativePath;
			string sourceFileName;
			string sourceFileExtension;
			int64_t sourceDurationInMilliSeconds;
			string sourcePhysicalDeliveryURL;
			string sourceTranscoderStagingAssetPathName;
			bool stopIfReferenceProcessingError;
			tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
				processDependencyInfo(workspace, ingestionJobKey, dependency);
			tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
				sourceFileExtension, sourceDurationInMilliSeconds, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
				stopIfReferenceProcessingError) = dependencyInfo;

			string encodedFileName =
				to_string(ingestionJobKey) + "_" + to_string(dependencyIndex++) + "_addSilentAudio" +
				(encodingProfileDetailsRoot == nullptr ? sourceFileExtension : getEncodedFileExtensionByEncodingProfile(encodingProfileDetailsRoot));

			string encodedTranscoderStagingAssetPathName; // used in case of
														  // external encoder
			fs::path encodedNFSStagingAssetPathName;
			{
				bool removeLinuxPathIfExist = false;
				bool neededForTranscoder = true;

				encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder,
					workspace->_directoryName,	// workspaceDirectoryName
					to_string(ingestionJobKey), // directoryNamePrefix
					"/",						// relativePath,
					// as specified by doc
					// (TASK_01_Add_Content_JSON_Format.txt), in case of hls
					// and external encoder (binary is ingested through
					// PUSH), the directory inside the tar.gz has to be
					// 'content'
					encodedFileName, // content
					-1,				 // _encodingItem->_mediaItemKey, not used because
									 // encodedFileName is not ""
					-1,				 // _encodingItem->_physicalPathKey, not used because
									 // encodedFileName is not ""
					removeLinuxPathIfExist
				);

				encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
			}

			json sourceRoot;
			sourceRoot["stopIfReferenceProcessingError"] = stopIfReferenceProcessingError;
			sourceRoot["sourceMediaItemKey"] = sourceMediaItemKey;
			sourceRoot["sourcePhysicalPathKey"] = sourcePhysicalPathKey;
			sourceRoot["sourceAssetPathName"] = sourceAssetPathName;
			sourceRoot["sourceDurationInMilliSeconds"] = sourceDurationInMilliSeconds;
			sourceRoot["sourceFileExtension"] = sourceFileExtension;
			sourceRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
			sourceRoot["sourceTranscoderStagingAssetPathName"] = sourceTranscoderStagingAssetPathName;
			sourceRoot["encodedTranscoderStagingAssetPathName"] = encodedTranscoderStagingAssetPathName;
			sourceRoot["encodedNFSStagingAssetPathName"] = encodedNFSStagingAssetPathName.string();

			sourcesRoot.push_back(sourceRoot);
		}

		_mmsEngineDBFacade->addEncoding_AddSilentAudio(
			workspace, ingestionJobKey, sourcesRoot, encodingProfileKey, encodingProfileDetailsRoot, _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL,
			_mmsIngestionURL, encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageAddSilentAudioTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageAddSilentAudioTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::managePictureInPictureTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 2)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t sourceMediaItemKey_1;
		int64_t sourcePhysicalPathKey_1;
		MMSEngineDBFacade::ContentType referenceContentType_1;
		string sourceAssetPathName_1;
		string sourceRelativePath_1;
		string sourceFileName_1;
		string sourceFileExtension_1;
		int64_t sourceDurationInMilliSeconds_1;
		string sourcePhysicalDeliveryURL_1;
		string sourceTranscoderStagingAssetPathName_1;
		bool stopIfReferenceProcessingError_1;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_1 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey_1, sourcePhysicalPathKey_1, referenceContentType_1, sourceAssetPathName_1, sourceRelativePath_1, sourceFileName_1,
			sourceFileExtension_1, sourceDurationInMilliSeconds_1, sourcePhysicalDeliveryURL_1, sourceTranscoderStagingAssetPathName_1,
			stopIfReferenceProcessingError_1) = dependencyInfo_1;

		int64_t sourceMediaItemKey_2;
		int64_t sourcePhysicalPathKey_2;
		MMSEngineDBFacade::ContentType referenceContentType_2;
		string sourceAssetPathName_2;
		string sourceRelativePath_2;
		string sourceFileName_2;
		string sourceFileExtension_2;
		int64_t sourceDurationInMilliSeconds_2;
		string sourcePhysicalDeliveryURL_2;
		string sourceTranscoderStagingAssetPathName_2;
		bool stopIfReferenceProcessingError_2;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_2 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[1]);
		tie(sourceMediaItemKey_2, sourcePhysicalPathKey_2, referenceContentType_2, sourceAssetPathName_2, sourceRelativePath_2, sourceFileName_2,
			sourceFileExtension_2, sourceDurationInMilliSeconds_2, sourcePhysicalDeliveryURL_2, sourceTranscoderStagingAssetPathName_2,
			stopIfReferenceProcessingError_2) = dependencyInfo_2;

		field = "SecondVideoOverlayedOnFirst";
		bool secondVideoOverlayedOnFirst = JSONUtils::asBool(parametersRoot, field, true);

		field = "SoundOfFirstVideo";
		bool soundOfFirstVideo = JSONUtils::asBool(parametersRoot, field, true);

		int64_t mainSourceMediaItemKey;
		int64_t mainSourcePhysicalPathKey;
		string mainSourceAssetPathName;
		int64_t mainSourceDurationInMilliSeconds;
		string mainSourceFileExtension;
		string mainSourcePhysicalDeliveryURL;
		string mainSourceTranscoderStagingAssetPathName;
		int64_t overlaySourceMediaItemKey;
		int64_t overlaySourcePhysicalPathKey;
		string overlaySourceAssetPathName;
		int64_t overlaySourceDurationInMilliSeconds;
		string overlaySourceFileExtension;
		string overlaySourcePhysicalDeliveryURL;
		string overlaySourceTranscoderStagingAssetPathName;
		bool soundOfMain;

		if (secondVideoOverlayedOnFirst)
		{
			mainSourceMediaItemKey = sourceMediaItemKey_1;
			mainSourcePhysicalPathKey = sourcePhysicalPathKey_1;
			mainSourceAssetPathName = sourceAssetPathName_1;
			mainSourceDurationInMilliSeconds = sourceDurationInMilliSeconds_1;
			mainSourceFileExtension = sourceFileExtension_1;
			mainSourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
			mainSourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_1;

			overlaySourceMediaItemKey = sourceMediaItemKey_2;
			overlaySourcePhysicalPathKey = sourcePhysicalPathKey_2;
			overlaySourceAssetPathName = sourceAssetPathName_2;
			overlaySourceDurationInMilliSeconds = sourceDurationInMilliSeconds_2;
			overlaySourceFileExtension = sourceFileExtension_2;
			overlaySourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
			overlaySourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_2;

			if (soundOfFirstVideo)
				soundOfMain = true;
			else
				soundOfMain = false;
		}
		else
		{
			mainSourceMediaItemKey = sourceMediaItemKey_2;
			mainSourcePhysicalPathKey = sourcePhysicalPathKey_2;
			mainSourceAssetPathName = sourceAssetPathName_2;
			mainSourceDurationInMilliSeconds = sourceDurationInMilliSeconds_2;
			mainSourceFileExtension = sourceFileExtension_2;
			mainSourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
			mainSourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_2;

			overlaySourceMediaItemKey = sourceMediaItemKey_1;
			overlaySourcePhysicalPathKey = sourcePhysicalPathKey_1;
			overlaySourceAssetPathName = sourceAssetPathName_1;
			overlaySourceDurationInMilliSeconds = sourceDurationInMilliSeconds_1;
			overlaySourceFileExtension = sourceFileExtension_1;
			overlaySourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
			overlaySourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_1;

			if (soundOfFirstVideo)
				soundOfMain = false;
			else
				soundOfMain = true;
		}

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			// This task shall contain EncodingProfileKey or
			// EncodingProfileLabel. We cannot have EncodingProfilesSetKey
			// because we replaced it with a GroupOfTasks
			//  having just EncodingProfileKey

			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				encodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
					workspace->_workspaceKey, MMSEngineDBFacade::ContentType::Video, encodingProfileLabel
				);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da
		// mainSourceFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_pictureInPicture" + mainSourceFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
		}

		_mmsEngineDBFacade->addEncoding_PictureInPictureJob(
			workspace, ingestionJobKey, mainSourceMediaItemKey, mainSourcePhysicalPathKey, mainSourceAssetPathName, mainSourceDurationInMilliSeconds,
			mainSourceFileExtension, mainSourcePhysicalDeliveryURL, mainSourceTranscoderStagingAssetPathName, overlaySourceMediaItemKey,
			overlaySourcePhysicalPathKey, overlaySourceAssetPathName, overlaySourceDurationInMilliSeconds, overlaySourceFileExtension,
			overlaySourcePhysicalDeliveryURL, overlaySourceTranscoderStagingAssetPathName, soundOfMain, encodingProfileKey,
			encodingProfileDetailsRoot, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL,
			_mmsBinaryIngestionURL, _mmsIngestionURL, encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageIntroOutroOverlayTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 3)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t introSourceMediaItemKey;
		int64_t introSourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType introReferenceContentType;
		string introSourceAssetPathName;
		string introSourceRelativePath;
		string introSourceFileName;
		string introSourceFileExtension;
		int64_t introSourceDurationInMilliSeconds;
		string introSourcePhysicalDeliveryURL;
		string introSourceTranscoderStagingAssetPathName;
		bool introStopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> introDependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(introSourceMediaItemKey, introSourcePhysicalPathKey, introReferenceContentType, introSourceAssetPathName, introSourceRelativePath,
			introSourceFileName, introSourceFileExtension, introSourceDurationInMilliSeconds, introSourcePhysicalDeliveryURL,
			introSourceTranscoderStagingAssetPathName, introStopIfReferenceProcessingError) = introDependencyInfo;

		int64_t mainSourceMediaItemKey;
		int64_t mainSourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType mainReferenceContentType;
		string mainSourceAssetPathName;
		string mainSourceRelativePath;
		string mainSourceFileName;
		string mainSourceFileExtension;
		int64_t mainSourceDurationInMilliSeconds;
		string mainSourcePhysicalDeliveryURL;
		string mainSourceTranscoderStagingAssetPathName;
		bool mainStopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> mainDependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[1]);
		tie(mainSourceMediaItemKey, mainSourcePhysicalPathKey, mainReferenceContentType, mainSourceAssetPathName, mainSourceRelativePath,
			mainSourceFileName, mainSourceFileExtension, mainSourceDurationInMilliSeconds, mainSourcePhysicalDeliveryURL,
			mainSourceTranscoderStagingAssetPathName, mainStopIfReferenceProcessingError) = mainDependencyInfo;

		int64_t outroSourceMediaItemKey;
		int64_t outroSourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType outroReferenceContentType;
		string outroSourceAssetPathName;
		string outroSourceRelativePath;
		string outroSourceFileName;
		string outroSourceFileExtension;
		int64_t outroSourceDurationInMilliSeconds;
		string outroSourcePhysicalDeliveryURL;
		string outroSourceTranscoderStagingAssetPathName;
		bool outroStopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> outroDependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[2]);
		tie(outroSourceMediaItemKey, outroSourcePhysicalPathKey, outroReferenceContentType, outroSourceAssetPathName, outroSourceRelativePath,
			outroSourceFileName, outroSourceFileExtension, outroSourceDurationInMilliSeconds, outroSourcePhysicalDeliveryURL,
			outroSourceTranscoderStagingAssetPathName, outroStopIfReferenceProcessingError) = outroDependencyInfo;

		int64_t encodingProfileKey;
		json encodingProfileDetailsRoot;
		{
			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
			}
			else
			{
				string errorMessage = string() + "Both fields are not present or it is null" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + keyField +
									  ", Field: " + labelField;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		string encodedFileName = to_string(ingestionJobKey) + "_introOutroOverlay" +
								 getEncodedFileExtensionByEncodingProfile(encodingProfileDetailsRoot); // mainSourceFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
		}

		_mmsEngineDBFacade->addEncoding_IntroOutroOverlayJob(
			workspace, ingestionJobKey, encodingProfileKey, encodingProfileDetailsRoot,

			introSourcePhysicalPathKey, introSourceAssetPathName, introSourceFileExtension, introSourceDurationInMilliSeconds,
			introSourcePhysicalDeliveryURL, introSourceTranscoderStagingAssetPathName,

			mainSourcePhysicalPathKey, mainSourceAssetPathName, mainSourceFileExtension, mainSourceDurationInMilliSeconds,
			mainSourcePhysicalDeliveryURL, mainSourceTranscoderStagingAssetPathName,

			outroSourcePhysicalPathKey, outroSourceAssetPathName, outroSourceFileExtension, outroSourceDurationInMilliSeconds,
			outroSourcePhysicalDeliveryURL, outroSourceTranscoderStagingAssetPathName,

			encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL,
			_mmsIngestionURL,

			encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageOverlayImageOnVideoTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 2)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t sourceMediaItemKey_1;
		int64_t sourcePhysicalPathKey_1;
		MMSEngineDBFacade::ContentType referenceContentType_1;
		string mmsSourceAssetPathName_1;
		string sourceFileName_1;
		string sourceFileExtension_1;
		int64_t sourceDurationInMilliSecs_1;
		string sourcePhysicalDeliveryURL_1;
		string sourceTranscoderStagingAssetPathName_1;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_1 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey_1, sourcePhysicalPathKey_1, referenceContentType_1, mmsSourceAssetPathName_1, ignore, sourceFileName_1,
			sourceFileExtension_1, sourceDurationInMilliSecs_1, sourcePhysicalDeliveryURL_1, sourceTranscoderStagingAssetPathName_1, ignore) =
			dependencyInfo_1;

		int64_t sourceMediaItemKey_2;
		int64_t sourcePhysicalPathKey_2;
		MMSEngineDBFacade::ContentType referenceContentType_2;
		string mmsSourceAssetPathName_2;
		string sourceFileName_2;
		string sourceFileExtension_2;
		int64_t sourceDurationInMilliSecs_2;
		string sourcePhysicalDeliveryURL_2;
		string sourceTranscoderStagingAssetPathName_2;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_2 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[1]);
		tie(sourceMediaItemKey_2, sourcePhysicalPathKey_2, referenceContentType_2, mmsSourceAssetPathName_2, ignore, sourceFileName_2,
			sourceFileExtension_2, sourceDurationInMilliSecs_2, sourcePhysicalDeliveryURL_2, sourceTranscoderStagingAssetPathName_2, ignore) =
			dependencyInfo_2;

		int64_t sourceVideoMediaItemKey;
		int64_t sourceVideoPhysicalPathKey;
		string mmsSourceVideoAssetPathName;
		string sourceVideoPhysicalDeliveryURL;
		string sourceVideoFileName;
		string sourceVideoFileExtension;
		string sourceVideoTranscoderStagingAssetPathName; // used in case of
														  // external encoder
		int64_t videoDurationInMilliSeconds;

		int64_t sourceImageMediaItemKey;
		int64_t sourceImagePhysicalPathKey;
		string mmsSourceImageAssetPathName;
		string sourceImagePhysicalDeliveryURL;

		if (referenceContentType_1 == MMSEngineDBFacade::ContentType::Video && referenceContentType_2 == MMSEngineDBFacade::ContentType::Image)
		{
			sourceVideoMediaItemKey = sourceMediaItemKey_1;
			sourceVideoPhysicalPathKey = sourcePhysicalPathKey_1;
			mmsSourceVideoAssetPathName = mmsSourceAssetPathName_1;
			sourceVideoPhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
			sourceVideoFileName = sourceFileName_1;
			sourceVideoFileExtension = sourceFileExtension_1;
			sourceVideoTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_1;
			videoDurationInMilliSeconds = sourceDurationInMilliSecs_1;

			sourceImageMediaItemKey = sourceMediaItemKey_2;
			sourceImagePhysicalPathKey = sourcePhysicalPathKey_2;
			mmsSourceImageAssetPathName = mmsSourceAssetPathName_2;
			sourceImagePhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
		}
		else if (referenceContentType_1 == MMSEngineDBFacade::ContentType::Image && referenceContentType_2 == MMSEngineDBFacade::ContentType::Video)
		{
			sourceVideoMediaItemKey = sourceMediaItemKey_2;
			sourceVideoPhysicalPathKey = sourcePhysicalPathKey_2;
			mmsSourceVideoAssetPathName = mmsSourceAssetPathName_2;
			sourceVideoPhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
			sourceVideoFileName = sourceFileName_2;
			sourceVideoFileExtension = sourceFileExtension_2;
			sourceVideoTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_2;
			videoDurationInMilliSeconds = sourceDurationInMilliSecs_2;

			sourceImageMediaItemKey = sourceMediaItemKey_1;
			sourceImagePhysicalPathKey = sourcePhysicalPathKey_1;
			mmsSourceImageAssetPathName = mmsSourceAssetPathName_1;
			sourceImagePhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
		}
		else
		{
			string errorMessage =
				string() + "OverlayImageOnVideo is not receiving one Video and one Image" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", referenceContentType_1: " + MMSEngineDBFacade::toString(referenceContentType_1) +
				", sourceMediaItemKey_1: " + to_string(sourceMediaItemKey_1) + ", sourcePhysicalPathKey_1: " + to_string(sourcePhysicalPathKey_1) +
				", contentType_2: " + MMSEngineDBFacade::toString(referenceContentType_2) +
				", sourceMediaItemKey_2: " + to_string(sourceMediaItemKey_2) + ", sourcePhysicalPathKey_2: " + to_string(sourcePhysicalPathKey_2);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da
		// sourceVideoFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_overlayedimage" + sourceVideoFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			fs::path workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
			encodedNFSStagingAssetPathName = workspaceIngestionRepository / encodedFileName;
		}

		_mmsEngineDBFacade->addEncoding_OverlayImageOnVideoJob(
			workspace, ingestionJobKey, encodingProfileKey, encodingProfileDetailsRoot, sourceVideoMediaItemKey, sourceVideoPhysicalPathKey,
			videoDurationInMilliSeconds, mmsSourceVideoAssetPathName, sourceVideoPhysicalDeliveryURL, sourceVideoFileExtension,
			sourceImageMediaItemKey, sourceImagePhysicalPathKey, mmsSourceImageAssetPathName, sourceImagePhysicalDeliveryURL,
			sourceVideoTranscoderStagingAssetPathName, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(),
			encodingPriority, _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL, _mmsIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageOverlayTextOnVideoTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType referenceContentType;
		string sourceAssetPathName;
		string sourceRelativePath;
		string sourceFileName;
		string sourceFileExtension;
		int64_t sourceDurationInMilliSecs;
		string sourcePhysicalDeliveryURL;
		string sourceTranscoderStagingAssetPathName;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
			sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName, ignore) = dependencyInfo;

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da
		// sourceFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_overlayedText" + sourceFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
		}

		SPDLOG_INFO(
			string() + "addEncoding_OverlayTextOnVideoJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingPriority: " + MMSEngineDBFacade::toString(encodingPriority)
		);
		_mmsEngineDBFacade->addEncoding_OverlayTextOnVideoJob(
			workspace, ingestionJobKey, encodingPriority,

			encodingProfileKey, encodingProfileDetailsRoot,

			sourceAssetPathName, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceFileExtension,

			sourceTranscoderStagingAssetPathName, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(),
			_mmsWorkflowIngestionURL, _mmsBinaryIngestionURL, _mmsIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::emailNotificationThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "emailNotificationThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		string sParameters = JSONUtils::toString(parametersRoot);

		SPDLOG_INFO(
			string() + "emailNotificationThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count()) +
			", dependencies.size: " + to_string(dependencies.size()) + ", sParameters: " + sParameters
		);

		string sDependencies;
		{
			for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
			{
				try
				{
					int64_t key;
					MMSEngineDBFacade::ContentType referenceContentType;
					Validator::DependencyType dependencyType;
					bool stopIfReferenceProcessingError;

					tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

					if (dependencyType == Validator::DependencyType::MediaItemKey)
					{
						bool warningIfMissing = false;

						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						MMSEngineDBFacade::ContentType contentType;
						string title;
						string userData;
						string ingestionDate;
						int64_t localIngestionJobKey;
						tie(contentType, title, userData, ingestionDate, ignore, localIngestionJobKey) = mediaItemDetails;

						sDependencies += string("MediaItemKey") + ", mediaItemKey: " + to_string(key) + ", title: " + title + ". ";
					}
					else if (dependencyType == Validator::DependencyType::PhysicalPathKey)
					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						int64_t mediaItemKey;
						string title;
						MMSEngineDBFacade::ContentType localContentType;
						string userData;
						string ingestionDate;
						int64_t localIngestionJobKey;
						tie(mediaItemKey, localContentType, title, userData, ingestionDate, localIngestionJobKey, ignore, ignore, ignore) =
							mediaItemDetails;

						sDependencies += string("PhysicalPathKey") + ", physicalPathKey: " + to_string(key) + ", title: " + title + ". ";
					}
					else // if (dependencyType ==
						 // Validator::DependencyType::IngestionJobKey)
					{
						bool warningIfMissing = false;
						tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
							_mmsEngineDBFacade->getIngestionJobDetails(
								workspace->_workspaceKey, key,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						string label;
						MMSEngineDBFacade::IngestionType ingestionType;
						MMSEngineDBFacade::IngestionStatus ingestionStatus;
						string metaDataContent;
						string errorMessage;

						tie(label, ingestionType, ingestionStatus, metaDataContent, errorMessage) = ingestionJobDetails;

						sDependencies += string("<br>IngestionJob") + ", dependencyType: " + to_string(static_cast<int>(dependencyType)) +
										 ", ingestionJobKey: " + to_string(key) + ", label: " + label + ". ";
					}
				}
				catch (...)
				{
					string errorMessage = string("Exception processing dependencies") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey);
					SPDLOG_ERROR(string() + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}

		string sReferencies;
		string checkStreaming_streamingName;
		string checkStreaming_streamingUrl;
		{
			string field = "references";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json referencesRoot = parametersRoot[field];
				for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
				{
					try
					{
						json referenceRoot = referencesRoot[referenceIndex];
						field = "ingestionJobKey";
						if (JSONUtils::isMetadataPresent(referenceRoot, field))
						{
							int64_t referenceIngestionJobKey = JSONUtils::asInt64(referenceRoot, field, 0);

							string referenceLabel;
							MMSEngineDBFacade::IngestionType ingestionType;
							string parameters;
							string referenceErrorMessage;

							tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
								_mmsEngineDBFacade->getIngestionJobDetails(
									workspace->_workspaceKey, referenceIngestionJobKey,
									// 2022-12-18: MIK potrebbe essere stato
									// appena aggiunto
									true
								);
							tie(referenceLabel, ingestionType, ignore, parameters, referenceErrorMessage) = ingestionJobDetails;

							sReferencies += string("<br>IngestionJob") + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType) +
											", ingestionJobKey: " + to_string(referenceIngestionJobKey) + ", label: " + referenceLabel +
											", errorMessage: " + referenceErrorMessage + ". ";

							if (ingestionType == MMSEngineDBFacade::IngestionType::CheckStreaming)
							{
								json parametersRoot = JSONUtils::toJson(parameters);

								string inputType;
								field = "inputType";
								inputType = JSONUtils::asString(parametersRoot, field, "");

								if (inputType == "Channel")
								{
									field = "channelConfigurationLabel";
									if (JSONUtils::isMetadataPresent(parametersRoot, field))
									{
										checkStreaming_streamingName = JSONUtils::asString(parametersRoot, field, "");

										bool warningIfMissing = false;
										tuple<
											int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int,
											int, int64_t>
											channelDetails = _mmsEngineDBFacade->getStreamDetails(
												workspace->_workspaceKey, checkStreaming_streamingName, warningIfMissing
											);

										string streamSourceType;
										tie(ignore, streamSourceType, ignore, checkStreaming_streamingUrl, ignore, ignore, ignore, ignore, ignore,
											ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = channelDetails;
									}
								}
								else
								{
									field = "streamingName";
									if (JSONUtils::isMetadataPresent(parametersRoot, field))
										checkStreaming_streamingName = JSONUtils::asString(parametersRoot, field, "");
									field = "streamingUrl";
									if (JSONUtils::isMetadataPresent(parametersRoot, field))
										checkStreaming_streamingUrl = JSONUtils::asString(parametersRoot, field, "");
								}
							}
						}
					}
					catch (...)
					{
						string errorMessage = string("Exception processing referencies") +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey);
						SPDLOG_ERROR(string() + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}

		string field = "configurationLabel";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string configurationLabel = JSONUtils::asString(parametersRoot, field, "");

		string tosCommaSeparated;
		string subject;
		string message;
		tuple<string, string, string> email = _mmsEngineDBFacade->getEMailByConfigurationLabel(workspace->_workspaceKey, configurationLabel);
		tie(tosCommaSeparated, subject, message) = email;

		field = "UserSubstitutions";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			json userSubstitutionsRoot = parametersRoot[field];

			for (int userSubstitutionIndex = 0; userSubstitutionIndex < userSubstitutionsRoot.size(); userSubstitutionIndex++)
			{
				json userSubstitutionRoot = userSubstitutionsRoot[userSubstitutionIndex];

				field = "ToBeReplaced";
				if (!JSONUtils::isMetadataPresent(userSubstitutionRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
					_logger->warn(errorMessage);

					continue;
				}
				string strToBeReplaced = JSONUtils::asString(userSubstitutionRoot, field, "");

				field = "ReplaceWith";
				if (!JSONUtils::isMetadataPresent(userSubstitutionRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
					_logger->warn(errorMessage);

					continue;
				}
				string strToReplace = JSONUtils::asString(userSubstitutionRoot, field, "");

				SPDLOG_INFO(
					string() + "User substitution" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", strToBeReplaced: " + strToBeReplaced + ", strToReplace: " + strToReplace
				);
				if (strToBeReplaced != "")
				{
					while (subject.find(strToBeReplaced) != string::npos)
						subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
					while (message.find(strToBeReplaced) != string::npos)
						message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
				}
			}
		}
		else
		{
			SPDLOG_INFO(
				"NO User substitution"
				", ingestionJobKey: {}"
				", _processorIdentifier: {}",
				ingestionJobKey, _processorIdentifier
			);
		}

		{
			string strToBeReplaced = "${Dependencies}";
			string strToReplace = sDependencies;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}
		{
			string strToBeReplaced = "${Referencies}";
			string strToReplace = sReferencies;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}
		{
			string strToBeReplaced = "${CheckStreaming_streamingName}";
			string strToReplace = checkStreaming_streamingName;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}
		{
			string strToBeReplaced = "${CheckStreaming_streamingUrl}";
			string strToReplace = checkStreaming_streamingUrl;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}

		vector<string> emailBody;
		emailBody.push_back(message);

		SPDLOG_INFO(
			"Sending email..."
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", _emailProviderURL: {}"
			", _emailUserName: {}"
			", subject: {}"
			", emailBody: {}"
			// ", _emailPassword: {}"
			,
			_processorIdentifier, ingestionJobKey, _emailProviderURL, _emailUserName, subject,
			message //, _emailPassword
		);
		MMSCURL::sendEmail(
			_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
			_emailUserName,	   // i.e.: info@catramms-cloud.com
			tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, _emailPassword
		);
		// EMailSender emailSender(_logger, _configuration);
		// bool useMMSCCToo = false;
		// emailSender.sendEmail(emailAddresses, subject, emailBody,
		// useMMSCCToo);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "sendEmail failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "sendEmail failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}

void MMSEngineProcessor::checkStreamingThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "checkStreamingThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "checkStreamingThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string field = "inputType";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string inputType = JSONUtils::asString(parametersRoot, field, "Stream");

		string streamingUrl;
		if (inputType == "Stream")
		{
			string field = "configurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string configurationLabel = JSONUtils::asString(parametersRoot, field, "");

			bool warningIfMissing = false;
			tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
				ipChannelDetails = _mmsEngineDBFacade->getStreamDetails(workspace->_workspaceKey, configurationLabel, warningIfMissing);
			string streamSourceType;
			tie(ignore, streamSourceType, ignore, streamingUrl, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
				ignore, ignore, ignore, ignore) = ipChannelDetails;
		}
		else
		{
			// StreamingName is mandatory even if it is not used here
			// It is mandatory because in case into the workflow we have the
			// EMail task, the Email task may need the StreamingName information
			// to add it into the email
			string field = "streamingName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string streamingName = JSONUtils::asString(parametersRoot, field, "");

			field = "streamingUrl";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamingUrl = JSONUtils::asString(parametersRoot, field, "");
		}

		SPDLOG_INFO(
			string() + "checkStreamingThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", _ingestionJobKey: " + to_string(ingestionJobKey) + ", inputType: " + inputType + ", streamingUrl: " + streamingUrl
		);

		if (streamingUrl == "")
		{
			string errorMessage = string() + "streamingUrl is wrong" + ", streamingUrl: " + streamingUrl;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			SPDLOG_INFO(
				string() + "Calling ffmpeg.getMediaInfo" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", _ingestionJobKey: " + to_string(ingestionJobKey) + ", streamingUrl: " + streamingUrl
			);
			int timeoutInSeconds = 20;
			bool isMMSAssetPathName = false;
			tuple<int64_t, long, json> mediaInfoDetails;
			vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
			vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
			FFMpeg ffmpeg(_configurationRoot, _logger);
			mediaInfoDetails = ffmpeg.getMediaInfo(ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, streamingUrl, videoTracks, audioTracks);
			SPDLOG_INFO(
				"Called ffmpeg.getMediaInfo"
				", _processorIdentifier: {}"
				", _ingestionJobKey: {}"
				", streamingUrl: {}"
				", videoTracks.size: {}"
				", audioTracks.size: {}",
				_processorIdentifier, ingestionJobKey, streamingUrl, videoTracks.size(), audioTracks.size()
			);
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "checkStreamingThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "checkStreamingThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}

void MMSEngineProcessor::manageMediaCrossReferenceTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 2)
		{
			string errorMessage = string() +
								  "No configured Two Media in order to create the Cross "
								  "Reference" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string field = "type";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		MMSEngineDBFacade::CrossReferenceType crossReferenceType =
			MMSEngineDBFacade::toCrossReferenceType(JSONUtils::asString(parametersRoot, field, ""));
		if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfImage)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfVideo;
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::AudioOfImage)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfAudio;
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfPoster)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::PosterOfVideo;

		MMSEngineDBFacade::ContentType firstContentType;
		int64_t firstMediaItemKey;
		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			int64_t key;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, firstContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				firstMediaItemKey = key;

				// serve solo per verificare che il media item è del workspace
				// di cui si hanno ii diritti di accesso Se mediaitem non
				// appartiene al workspace avremo una eccezione
				// (MediaItemKeyNotFound)
				_mmsEngineDBFacade->getMediaItemKeyDetails(workspace->_workspaceKey, firstMediaItemKey, false, false);
			}
			else
			{
				int64_t physicalPathKey = key;

				bool warningIfMissing = false;
				tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, physicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

				tie(firstMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}
		}

		MMSEngineDBFacade::ContentType secondContentType;
		int64_t secondMediaItemKey;
		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[1];

			int64_t key;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, secondContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				secondMediaItemKey = key;

				// serve solo per verificare che il media item è del workspace
				// di cui si hanno ii diritti di accesso Se mediaitem non
				// appartiene al workspace avremo una eccezione
				// (MediaItemKeyNotFound)
				_mmsEngineDBFacade->getMediaItemKeyDetails(workspace->_workspaceKey, secondMediaItemKey, false, false);
			}
			else
			{
				int64_t physicalPathKey = key;

				bool warningIfMissing = false;
				tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, physicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

				tie(secondMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}
		}

		if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::ImageOfVideo ||
			crossReferenceType == MMSEngineDBFacade::CrossReferenceType::FaceOfVideo ||
			crossReferenceType == MMSEngineDBFacade::CrossReferenceType::PosterOfVideo)
		{
			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Video && secondContentType == MMSEngineDBFacade::ContentType::Image)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(secondMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(firstMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Image && secondContentType == MMSEngineDBFacade::ContentType::Video)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(firstMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(secondMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::ImageOfAudio)
		{
			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Audio && secondContentType == MMSEngineDBFacade::ContentType::Image)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(secondMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(firstMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Image && secondContentType == MMSEngineDBFacade::ContentType::Audio)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(firstMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(secondMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfVideo)
		{
			if (firstContentType != MMSEngineDBFacade::ContentType::Video || secondContentType != MMSEngineDBFacade::ContentType::Video)
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "parameters";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage =
					string() + "Cross Reference Parameters are not present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
					", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
					", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
					", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
					", firstMediaItemKey: " + to_string(firstMediaItemKey) + ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json crossReferenceParametersRoot = parametersRoot[field];

			_mmsEngineDBFacade->addCrossReference(
				ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
			);
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfAudio)
		{
			if (firstContentType != MMSEngineDBFacade::ContentType::Audio || secondContentType != MMSEngineDBFacade::ContentType::Audio)
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "parameters";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage =
					string() + "Cross Reference Parameters are not present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
					", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
					", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
					", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
					", firstMediaItemKey: " + to_string(firstMediaItemKey) + ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json crossReferenceParametersRoot = parametersRoot[field];

			_mmsEngineDBFacade->addCrossReference(
				ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
			);
		}
		else
		{
			string errorMessage = string() + "Wrong type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
								  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
								  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
								  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
								  ", firstMediaItemKey: " + to_string(firstMediaItemKey) + ", secondMediaItemKey: " + to_string(secondMediaItemKey);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (DeadlockFound &e)
	{
		SPDLOG_ERROR(
			string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
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

void MMSEngineProcessor::handleCheckEncodingEvent()
{
	try
	{
		if (isMaintenanceMode())
		{
			SPDLOG_INFO(
				string() +
				"Received handleCheckEncodingEvent, not managed it because of "
				"MaintenanceMode" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			return;
		}

		SPDLOG_INFO(string() + "Received handleCheckEncodingEvent" + ", _processorIdentifier: " + to_string(_processorIdentifier));

		vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> encodingItems;

		_mmsEngineDBFacade->getEncodingJobs(_processorMMS, encodingItems, _timeBeforeToPrepareResourcesInMinutes, _maxEncodingJobsPerEvent);

		SPDLOG_INFO(
			string() + "_pActiveEncodingsManager->addEncodingItems" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", encodingItems.size: " + to_string(encodingItems.size())
		);

		_pActiveEncodingsManager->addEncodingItems(encodingItems);

		SPDLOG_INFO(
			string() + "getEncodingJobs result" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", encodingItems.size: " + to_string(encodingItems.size())
		);
	}
	catch (AlreadyLocked &e)
	{
		_logger->warn(
			string() + "getEncodingJobs was not done" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
		);

		return;
		// throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(string() + "getEncodingJobs failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what());

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(string() + "getEncodingJobs failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what());

		throw e;
	}
}

void MMSEngineProcessor::handleContentRetentionEventThread(shared_ptr<long> processorsThreadsNumber)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleContentRetentionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		-1 // ingestionJobKey
	);

	SPDLOG_INFO(
		string() + "handleContentRetentionEventThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
	);

	chrono::system_clock::time_point start = chrono::system_clock::now();

	{
		vector<tuple<shared_ptr<Workspace>, int64_t, int64_t>> mediaItemKeyOrPhysicalPathKeyToBeRemoved;
		bool moreRemoveToBeDone = true;

		while (moreRemoveToBeDone)
		{
			try
			{
				int maxMediaItemKeysNumber = 100;

				mediaItemKeyOrPhysicalPathKeyToBeRemoved.clear();
				_mmsEngineDBFacade->getExpiredMediaItemKeysCheckingDependencies(
					_processorMMS, mediaItemKeyOrPhysicalPathKeyToBeRemoved, maxMediaItemKeysNumber
				);

				if (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() == 0)
					moreRemoveToBeDone = false;
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "getExpiredMediaItemKeysCheckingDependencies failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
				break;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "getExpiredMediaItemKeysCheckingDependencies failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
				break;
			}

			for (tuple<shared_ptr<Workspace>, int64_t, int64_t> workspaceMediaItemKeyOrPhysicalPathKey : mediaItemKeyOrPhysicalPathKeyToBeRemoved)
			{
				shared_ptr<Workspace> workspace;
				int64_t mediaItemKey;
				int64_t physicalPathKey;

				tie(workspace, mediaItemKey, physicalPathKey) = workspaceMediaItemKeyOrPhysicalPathKey;

				SPDLOG_INFO(
					string() + "Removing because of ContentRetention" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", workspace->_name: " + workspace->_name +
					", mediaItemKey: " + to_string(mediaItemKey) + ", physicalPathKey: " + to_string(physicalPathKey)
				);

				try
				{
					if (physicalPathKey == -1)
						_mmsStorage->removeMediaItem(mediaItemKey);
					else
						_mmsStorage->removePhysicalPath(physicalPathKey);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						string() + "_mmsStorage->removeMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", workspace->_name: " + workspace->_name +
						", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
						", exception: " + e.what()
					);

					try
					{
						string processorMMSForRetention = "";
						_mmsEngineDBFacade->updateMediaItem(mediaItemKey, processorMMSForRetention);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
						);
					}

					// one remove failed, procedure has to go ahead to try all
					// the other removes moreRemoveToBeDone = false; break;

					continue;
					// no throw since it is running in a detached thread
					// throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						string() + "_mmsStorage->removeMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", workspace->_name: " + workspace->_name +
						", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey)
					);

					try
					{
						string processorMMSForRetention = "";
						_mmsEngineDBFacade->updateMediaItem(mediaItemKey, processorMMSForRetention);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
						);
					}

					// one remove failed, procedure has to go ahead to try all
					// the other removes moreRemoveToBeDone = false; break;

					continue;
					// no throw since it is running in a detached thread
					// throw e;
				}
			}
		}

		chrono::system_clock::time_point end = chrono::system_clock::now();
		SPDLOG_INFO(
			string() + "Content retention finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
		);
	}

	/* Already done by the crontab script
	{
		SPDLOG_INFO(string() + "Staging Retention started"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
			+ ", _mmsStorage->getStagingRootRepository(): " +
_mmsStorage->getStagingRootRepository()
		);

		try
		{
			chrono::system_clock::time_point tpNow =
chrono::system_clock::now();

			FileIO::DirectoryEntryType_t detDirectoryEntryType;
			shared_ptr<FileIO::Directory> directory = FileIO::openDirectory
(_mmsStorage->getStagingRootRepository());

			bool scanDirectoryFinished = false;
			while (!scanDirectoryFinished)
			{
				string directoryEntry;
				try
				{
					string directoryEntry = FileIO::readDirectory (directory,
						&detDirectoryEntryType);

//                    if (detDirectoryEntryType !=
FileIO::TOOLS_FILEIO_REGULARFILE)
//                        continue;

					string pathName = _mmsStorage->getStagingRootRepository()
							+ directoryEntry;
					chrono::system_clock::time_point tpLastModification =
							FileIO:: getFileTime (pathName);

					int elapsedInHours =
chrono::duration_cast<chrono::hours>(tpNow - tpLastModification).count(); double
elapsedInDays =  elapsedInHours / 24; if (elapsedInDays >=
_stagingRetentionInDays)
					{
						if (detDirectoryEntryType == FileIO::
TOOLS_FILEIO_DIRECTORY)
						{
							SPDLOG_INFO(string() + "Removing staging
directory because of Retention"
								+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
								+ ", pathName: " + pathName
								+ ", elapsedInDays: " + to_string(elapsedInDays)
								+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
							);

							try
							{
								bool removeRecursively = true;

								FileIO::removeDirectory(pathName,
removeRecursively);
							}
							catch(runtime_error& e)
							{
								_logger->warn(string() + "Error removing
staging directory because of Retention"
									+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
									+ ", pathName: " + pathName
									+ ", elapsedInDays: " +
to_string(elapsedInDays)
									+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception& e)
							{
								_logger->warn(string() + "Error removing
staging directory because of Retention"
									+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
									+ ", pathName: " + pathName
									+ ", elapsedInDays: " +
to_string(elapsedInDays)
									+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
									+ ", e.what(): " + e.what()
								);
							}
						}
						else
						{
							SPDLOG_INFO(string() + "Removing staging file
because of Retention"
								+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
								+ ", pathName: " + pathName
								+ ", elapsedInDays: " + to_string(elapsedInDays)
								+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
							);

							bool exceptionInCaseOfError = false;

							FileIO::remove(pathName, exceptionInCaseOfError);
						}
					}
				}
				catch(DirectoryListFinished& e)
				{
					scanDirectoryFinished = true;
				}
				catch(runtime_error& e)
				{
					string errorMessage = string() + "listing directory
failed"
						+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
						   + ", e.what(): " + e.what()
					;
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
				catch(exception& e)
				{
					string errorMessage = string() + "listing directory
failed"
						+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
						   + ", e.what(): " + e.what()
					;
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
			}

			FileIO::closeDirectory (directory);
		}
		catch(runtime_error& e)
		{
			SPDLOG_ERROR(string() + "removeHavingPrefixFileName failed"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
				+ ", e.what(): " + e.what()
			);
		}
		catch(exception& e)
		{
			SPDLOG_ERROR(string() + "removeHavingPrefixFileName failed"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}

		SPDLOG_INFO(string() + "Staging Retention finished"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
		);
	}
	*/
}

void MMSEngineProcessor::handleDBDataRetentionEventThread()
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleDBDataRetentionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		-1 // ingestionJobKey,
	);

	bool alreadyExecuted = true;

	try
	{
		SPDLOG_INFO(string() + "DBDataRetention: oncePerDayExecution" + ", _processorIdentifier: " + to_string(_processorIdentifier));

		alreadyExecuted = _mmsEngineDBFacade->oncePerDayExecution(MMSEngineDBFacade::OncePerDayType::DBDataRetention);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", exception: " + e.what()
		);

		// no throw since it is running in a detached thread
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", exception: " + e.what()
		);

		// no throw since it is running in a detached thread
		// throw e;
	}

	if (!alreadyExecuted)
	{
		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(string() + "DBDataRetention: Ingestion Data started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			try
			{
				_mmsEngineDBFacade->retentionOfIngestionData();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() + "DBDataRetention: Ingestion Data finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(string() + "DBDataRetention: Delivery Autorization started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			try
			{
				_mmsEngineDBFacade->retentionOfDeliveryAuthorization();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Delivery Autorization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Delivery Autorization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() + "DBDataRetention: Delivery Autorization finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(string() + "DBDataRetention: Statistic Data started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			try
			{
				_mmsEngineDBFacade->retentionOfStatisticData();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Statistic Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Statistic Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() + "DBDataRetention: Statistic Data finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of EncodingJobs having wrong status "
				"started" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				// Scenarios: IngestionJob in final status but EncodingJob not
				// in final status
				_mmsEngineDBFacade->fixEncodingJobsHavingWrongStatus();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of EncodingJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of EncodingJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of EncodingJobs having wrong status "
				"finished" +
				", _processorIdentifier: " + to_string(_processorIdentifier) + ", @MMS statistics@ - duration (secs): @" +
				to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of IngestionJobs having wrong status "
				"started" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				// Scenarios: EncodingJob in final status but IngestionJob not
				// in final status
				//		even it it was passed long time
				_mmsEngineDBFacade->fixIngestionJobsHavingWrongStatus();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of IngestionJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of IngestionJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of IngestionJobs having wrong status "
				"finished" +
				", _processorIdentifier: " + to_string(_processorIdentifier) + ", @MMS statistics@ - duration (secs): @" +
				to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}
	}
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

tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool> MMSEngineProcessor::getMediaSourceDetails(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot
)
{
	// only in case of externalReadOnlyStorage, nextIngestionStatus does not
	// change and we do not need it So I set it just to a state
	MMSEngineDBFacade::IngestionStatus nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::Start_TaskQueued;
	string mediaSourceURL;
	string mediaFileFormat;
	bool externalReadOnlyStorage;

	string field;
	if (ingestionType != MMSEngineDBFacade::IngestionType::AddContent)
	{
		string errorMessage = string() + "ingestionType is wrong" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	externalReadOnlyStorage = false;
	{
		field = "sourceURL";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
			mediaSourceURL = JSONUtils::asString(parametersRoot, field, "");

		field = "fileFormat";
		mediaFileFormat = JSONUtils::asString(parametersRoot, field, "");

		// string httpPrefix("http://");
		// string httpsPrefix("https://");
		// // string ftpPrefix("ftp://");
		// string ftpsPrefix("ftps://");
		// string movePrefix("move://"); // move:///dir1/dir2/.../file
		// string mvPrefix("mv://");
		// string copyPrefix("copy://");
		// string cpPrefix("cp://");
		// string externalStoragePrefix("externalStorage://");
		// if ((mediaSourceURL.size() >= httpPrefix.size() && 0 == mediaSourceURL.compare(0, httpPrefix.size(), httpPrefix)) ||
		// 	(mediaSourceURL.size() >= httpsPrefix.size() && 0 == mediaSourceURL.compare(0, httpsPrefix.size(), httpsPrefix)) ||
		// 	(mediaSourceURL.size() >= ftpPrefix.size() && 0 == mediaSourceURL.compare(0, ftpPrefix.size(), ftpPrefix)) ||
		// 	(mediaSourceURL.size() >= ftpsPrefix.size() && 0 == mediaSourceURL.compare(0, ftpsPrefix.size(), ftpsPrefix)))
		if (StringUtils::startWith(mediaSourceURL, "http://")
			|| StringUtils::startWith(mediaSourceURL, "https://")
			|| StringUtils::startWith(mediaSourceURL, "ftp://")
			|| StringUtils::startWith(mediaSourceURL, "ftps://")
			)
		{
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress;
		}
		else if (StringUtils::startWith(mediaSourceURL, "move://")
			|| StringUtils::startWith(mediaSourceURL, "mv://")
			)
		{
		// else if ((mediaSourceURL.size() >= movePrefix.size() && 0 == mediaSourceURL.compare(0, movePrefix.size(), movePrefix)) ||
			// 	 (mediaSourceURL.size() >= mvPrefix.size() && 0 == mediaSourceURL.compare(0, mvPrefix.size(), mvPrefix)))
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress;
		}
		else if (StringUtils::startWith(mediaSourceURL, "copy://")
			|| StringUtils::startWith(mediaSourceURL, "cp://")
			)
		// else if ((mediaSourceURL.size() >= copyPrefix.size() && 0 == mediaSourceURL.compare(0, copyPrefix.size(), copyPrefix)) ||
			// 	 (mediaSourceURL.size() >= cpPrefix.size() && 0 == mediaSourceURL.compare(0, cpPrefix.size(), cpPrefix)))
		{
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress;
		}
		else if (StringUtils::startWith(mediaSourceURL, "externalStorage://"))
		// else if (mediaSourceURL.size() >= externalStoragePrefix.size() &&
			// 	 0 == mediaSourceURL.compare(0, externalStoragePrefix.size(), externalStoragePrefix))
		{
			externalReadOnlyStorage = true;
		}
		else
		{
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress;
		}
	}

	string md5FileCheckSum;
	field = "MD5FileCheckSum";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		// MD5         md5;
		// char        md5RealDigest [32 + 1];

		md5FileCheckSum = JSONUtils::asString(parametersRoot, field, "");
	}

	int fileSizeInBytes = -1;
	field = "FileSizeInBytes";
	fileSizeInBytes = JSONUtils::asInt(parametersRoot, field, -1);

	/*
	tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int>
	mediaSourceDetails; get<0>(mediaSourceDetails) = nextIngestionStatus;
	get<1>(mediaSourceDetails) = mediaSourceURL;
	get<2>(mediaSourceDetails) = mediaFileFormat;
	get<3>(mediaSourceDetails) = md5FileCheckSum;
	get<4>(mediaSourceDetails) = fileSizeInBytes;
	*/

	SPDLOG_INFO(
		string() + "media source details" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(ingestionJobKey) + ", nextIngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
		", mediaSourceURL: " + mediaSourceURL + ", mediaFileFormat: " + mediaFileFormat + ", md5FileCheckSum: " + md5FileCheckSum +
		", fileSizeInBytes: " + to_string(fileSizeInBytes) + ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage)
	);

	return make_tuple(nextIngestionStatus, mediaSourceURL, mediaFileFormat, md5FileCheckSum, fileSizeInBytes, externalReadOnlyStorage);
}

void MMSEngineProcessor::validateMediaSourceFile(
	int64_t ingestionJobKey, string mediaSourcePathName, string mediaFileFormat, string md5FileCheckSum, int fileSizeInBytes
)
{

	if (mediaFileFormat == "m3u8-tar.gz")
	{
		// in this case it is a directory with segments inside
		bool dirExists = false;
		{
			chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
			do
			{
				if (fs::exists(mediaSourcePathName))
				{
					dirExists = true;
					break;
				}

				this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
			} while (chrono::system_clock::now() < end);
		}

		if (!dirExists)
		{
			string errorMessage = string() +
								  "Media Source directory does not exist (it was not uploaded "
								  "yet)" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", mediaSourcePathName: " + mediaSourcePathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else
	{
		// we added the following two parameters for the FileIO::fileExisting
		// method because, in the scenario where still MMS generates the file to
		// be ingested (i.e.: generate frames task and other tasks), and the NFS
		// is used, we saw sometimes FileIO::fileExisting returns false even if
		// the file is there. This is due because of NFS delay to present the
		// file
		bool fileExists = false;
		{
			chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
			do
			{
				if (fs::exists(mediaSourcePathName))
				{
					fileExists = true;
					break;
				}

				this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
			} while (chrono::system_clock::now() < end);
		}
		if (!fileExists)
		{
			string errorMessage = string() + "Media Source file does not exist (it was not uploaded yet)" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", mediaSourcePathName: " + mediaSourcePathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	// we just simplify and md5FileCheck is not done in case of segments
	if (mediaFileFormat != "m3u8-tar.gz" && md5FileCheckSum != "")
	{
		EVP_MD_CTX *ctx = EVP_MD_CTX_create();
		const EVP_MD *mdType = EVP_md5();
		EVP_MD_CTX_init(ctx);
		EVP_DigestInit_ex(ctx, mdType, nullptr);

		std::ifstream ifs;
		ifs.open(mediaSourcePathName, ios::binary | ios::in);
		if (!ifs.good())
		{
			string errorMessage = string() + "Media files to be opened" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSourcePathName: " + mediaSourcePathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		char data[MD5BUFFERSIZE];
		size_t size;
		do
		{
			ifs.read(data, MD5BUFFERSIZE);
			size = ifs.gcount();
			for (size_t bytes_done = 0; bytes_done < size;)
			{
				size_t bytes = 4096;
				auto missing = size - bytes_done;

				if (missing < bytes)
					bytes = missing;

				auto dataStart = static_cast<void *>(static_cast<char *>(data) + bytes_done);
				EVP_DigestUpdate(ctx, dataStart, bytes);
				bytes_done += bytes;
			}
		} while (ifs.good());
		ifs.close();

		std::vector<unsigned char> md(EVP_MD_size(mdType));
		EVP_DigestFinal_ex(ctx, md.data(), nullptr);
		EVP_MD_CTX_destroy(ctx);

		std::ostringstream hashBuffer;

		for (auto c : md)
			hashBuffer << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(c);

		string md5RealDigest = hashBuffer.str();

		bool isCaseInsensitiveEqual =
			md5FileCheckSum.length() != md5RealDigest.length()
				? false
				: equal(
					  md5FileCheckSum.begin(), md5FileCheckSum.end(), md5RealDigest.begin(), [](int c1, int c2) { return toupper(c1) == toupper(c2); }
				  );

		if (!isCaseInsensitiveEqual)
		{
			string errorMessage = string() + "MD5 check failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSourcePathName: " + mediaSourcePathName +
								  ", md5FileCheckSum: " + md5FileCheckSum + ", md5RealDigest: " + md5RealDigest;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	// we just simplify and file size check is not done in case of segments
	if (mediaFileFormat != "m3u8-tar.gz" && fileSizeInBytes != -1)
	{
		unsigned long downloadedFileSizeInBytes = fs::file_size(mediaSourcePathName);

		if (fileSizeInBytes != downloadedFileSizeInBytes)
		{
			string errorMessage = string() + "FileSize check failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSourcePathName: " + mediaSourcePathName +
								  ", metadataFileSizeInBytes: " + to_string(fileSizeInBytes) +
								  ", downloadedFileSizeInBytes: " + to_string(downloadedFileSizeInBytes);
			SPDLOG_ERROR(errorMessage);
			throw runtime_error(errorMessage);
		}
	}
}

size_t curlDownloadCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	chrono::system_clock::time_point start = chrono::system_clock::now();

	MMSEngineProcessor::CurlDownloadData *curlDownloadData = (MMSEngineProcessor::CurlDownloadData *)f;

	auto logger = spdlog::get("mmsEngineService");

	if (curlDownloadData->currentChunkNumber == 0)
	{
		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::trunc);
		curlDownloadData->currentChunkNumber += 1;

		SPDLOG_INFO(
			"Opening binary file"
			", curlDownloadData -> ingestionJobKey: {}"
			", curlDownloadData -> destBinaryPathName: {}"
			", curlDownloadData->currentChunkNumber: {}"
			", curlDownloadData->currentTotalSize: {}"
			", curlDownloadData->maxChunkFileSize: {}",
			curlDownloadData->ingestionJobKey, curlDownloadData->destBinaryPathName, curlDownloadData->currentChunkNumber,
			curlDownloadData->currentTotalSize, curlDownloadData->maxChunkFileSize
		);
	}
	else if (curlDownloadData->currentTotalSize >= curlDownloadData->currentChunkNumber * curlDownloadData->maxChunkFileSize)
	{
		(curlDownloadData->mediaSourceFileStream).close();

		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::app);
		curlDownloadData->currentChunkNumber += 1;

		SPDLOG_INFO(
			"Opening binary file"
			", curlDownloadData -> ingestionJobKey: {}"
			", curlDownloadData -> destBinaryPathName: {}"
			", curlDownloadData->currentChunkNumber: {}"
			", curlDownloadData->currentTotalSize: {}"
			", curlDownloadData->maxChunkFileSize: {}"
			", tellp: {}",
			curlDownloadData->ingestionJobKey, curlDownloadData->destBinaryPathName, curlDownloadData->currentChunkNumber,
			curlDownloadData->currentTotalSize, curlDownloadData->maxChunkFileSize, (curlDownloadData->mediaSourceFileStream).tellp()
		);
	}

	curlDownloadData->mediaSourceFileStream.write(ptr, size * nmemb);
	curlDownloadData->currentTotalSize += (size * nmemb);

	// debug perchè avremmo tantissimi log con elapsed 0
	SPDLOG_DEBUG(
		"curlDownloadCallback"
		", ingestionJobKey: {}"
		", bytes written: {}"
		", elapsed (millisecs): {}",
		curlDownloadData->ingestionJobKey, size * nmemb, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start).count()
	);

	return size * nmemb;
};

void MMSEngineProcessor::downloadMediaSourceFileThread(
	shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, bool regenerateTimestamps, int m3u8TarGzOrM3u8Streaming,
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "downloadMediaSourceFileThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	bool downloadingCompleted = false;

	SPDLOG_INFO(
		string() + "downloadMediaSourceFileThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(ingestionJobKey) + ", m3u8TarGzOrM3u8Streaming: " + to_string(m3u8TarGzOrM3u8Streaming) +
		", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
	);
	/*
		- aggiungere un timeout nel caso nessun pacchetto è ricevuto entro XXXX
	seconds
		- per il resume:
			l'apertura dello stream of dovrà essere fatta in append in questo
	caso usare l'opzione CURLOPT_RESUME_FROM o CURLOPT_RESUME_FROM_LARGE (>2GB)
	per dire da dove ripartire per ftp vedere
	https://raw.githubusercontent.com/curl/curl/master/docs/examples/ftpuploadresume.c

	RESUMING FILE TRANSFERS

	 To continue a file transfer where it was previously aborted, curl supports
	 resume on http(s) downloads as well as ftp uploads and downloads.

	 Continue downloading a document:

			curl -C - -o file ftp://ftp.server.com/path/file

	 Continue uploading a document(*1):

			curl -C - -T file ftp://ftp.server.com/path/file

	 Continue downloading a document from a web server(*2):

			curl -C - -o file http://www.server.com/

	 (*1) = This requires that the ftp server supports the non-standard command
			SIZE. If it doesn't, curl will say so.

	 (*2) = This requires that the web server supports at least HTTP/1.1. If it
			doesn't, curl will say so.
	 */

	string localSourceReferenceURL = sourceReferenceURL;
	int localM3u8TarGzOrM3u8Streaming = m3u8TarGzOrM3u8Streaming;
	// in case of youtube url, the real URL to be used has to be calcolated
	{
			if (StringUtils::startWith(sourceReferenceURL, "https://www.youtube.com/")
			|| StringUtils::startWith(sourceReferenceURL, "https://youtu.be/")
		)
		// string youTubePrefix1("https://www.youtube.com/");
		// string youTubePrefix2("https://youtu.be/");
		// if ((sourceReferenceURL.size() >= youTubePrefix1.size() && 0 == sourceReferenceURL.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
		// 	(sourceReferenceURL.size() >= youTubePrefix2.size() && 0 == sourceReferenceURL.compare(0, youTubePrefix2.size(), youTubePrefix2)))
		{
			try
			{
				FFMpeg ffmpeg(_configurationRoot, _logger);
				pair<string, string> streamingURLDetails = ffmpeg.retrieveStreamingYouTubeURL(ingestionJobKey, sourceReferenceURL);

				string streamingYouTubeURL;
				tie(streamingYouTubeURL, ignore) = streamingURLDetails;

				SPDLOG_INFO(
					string() + "downloadMediaSourceFileThread. YouTube URL calculation" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " + to_string(ingestionJobKey) +
					", initial YouTube URL: " + sourceReferenceURL + ", streaming YouTube URL: " + streamingYouTubeURL
				);

				localSourceReferenceURL = streamingYouTubeURL;

				// for sure localM3u8TarGzOrM3u8Streaming has to be false
				localM3u8TarGzOrM3u8Streaming = 0;
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "ffmpeg.retrieveStreamingYouTubeURL failed" + ", may be the YouTube URL is not available anymore" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", YouTube URL: " + sourceReferenceURL +
									  ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				SPDLOG_INFO(
					string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
					to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage
				);
				try
				{
					// to hide ffmpeg staff
					errorMessage = string() + "retrieveStreamingYouTubeURL failed" + ", may be the YouTube URL is not available anymore" +
								   ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", YouTube URL: " + sourceReferenceURL + ", e.what(): " + e.what();
					_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage);
				}
				catch (runtime_error &re)
				{
					SPDLOG_INFO(
						string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
					);
				}
				catch (exception &ex)
				{
					SPDLOG_INFO(
						string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
					);
				}

				return;
			}
		}
	}

	string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
	string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
	// 0: no m3u8
	// 1: m3u8 by .tar.gz
	// 2: m3u8 by streaming (it will be saved as .mp4)
	// .mp4 is used in
	// 1. downloadMediaSourceFileThread (when the m3u8-streaming is downloaded
	// in a .mp4 file
	// 2. handleLocalAssetIngestionEvent (when the IngestionRepository file name
	//		is built "consistent" with the above step no. 1)
	// 3. here, handleLocalAssetIngestionEvent (when the MMS file name is
	// generated)
	if (localM3u8TarGzOrM3u8Streaming == 1)
		destBinaryPathName = destBinaryPathName + ".tar.gz";
	else if (localM3u8TarGzOrM3u8Streaming == 2)
		destBinaryPathName = destBinaryPathName + ".mp4";

	if (localM3u8TarGzOrM3u8Streaming == 2)
	{
		try
		{
			SPDLOG_INFO(
				string() + "ffmpeg.streamingToFile" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", destBinaryPathName: " + destBinaryPathName
			);

			// regenerateTimestamps (see
			// docs/TASK_01_Add_Content_JSON_Format.txt)
			FFMpeg ffmpeg(_configurationRoot, _logger);
			ffmpeg.streamingToFile(ingestionJobKey, regenerateTimestamps, sourceReferenceURL, destBinaryPathName);

			downloadingCompleted = true;

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", destBinaryPathName: " + destBinaryPathName +
				", downloadingCompleted: " + to_string(downloadingCompleted)
			);
			_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, downloadingCompleted);
		}
		catch (runtime_error &e)
		{
			string errorMessage = string() + "ffmpeg.streamingToFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL +
								  ", destBinaryPathName: " + destBinaryPathName + ", e.what(): " + e.what();
			SPDLOG_ERROR(errorMessage);

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage
			);
			try
			{
				// to hide ffmpeg staff
				errorMessage = string() + "streamingToFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							   ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL +
							   ", destBinaryPathName: " + destBinaryPathName + ", e.what(): " + e.what();
				_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
				);
			}

			return;
		}
	}
	else
	{
		for (int attemptIndex = 0; attemptIndex < _maxDownloadAttemptNumber && !downloadingCompleted; attemptIndex++)
		{
			// 2023-12-20: questa variabile viene inizializzata a true nel
			// metodo progressDownloadCallback
			//	nel caso l'utente cancelli il download. Ci sono due problemi:
			//	1. l'utente non puo cancellare il download perchè attualmente la
			// GUI permette il kill/cancel 		in caso di encodingJob e, nel
			// Download, non abbiamo alcun encodingJob
			//	2. anche se questa variabile viene passata come 'reference' nel
			// metodo progressDownloadCallback 		il suo valore modificato non
			// ritorna qui, per cui la gestione di downloadingStoppedByUser =
			// true 		nella eccezione sotto non funziona
			bool downloadingStoppedByUser = false;

			try
			{
				SPDLOG_INFO(
					string() + "Downloading" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL +
					", destBinaryPathName: " + destBinaryPathName + ", attempt: " + to_string(attemptIndex + 1) +
					", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
				);

				if (attemptIndex == 0)
				{
					CurlDownloadData curlDownloadData;
					curlDownloadData.ingestionJobKey = ingestionJobKey;
					curlDownloadData.currentChunkNumber = 0;
					curlDownloadData.currentTotalSize = 0;
					curlDownloadData.destBinaryPathName = destBinaryPathName;
					curlDownloadData.maxChunkFileSize = _downloadChunkSizeInMegaBytes * 1000000;

					// fstream mediaSourceFileStream(destBinaryPathName,
					// ios::binary | ios::out);
					// mediaSourceFileStream.exceptions(ios::badbit |
					// ios::failbit);   // setting the exception mask FILE
					// *mediaSourceFileStream =
					// fopen(destBinaryPathName.c_str(), "wb");

					curlpp::Cleanup cleaner;
					curlpp::Easy request;

					// Set the writer callback to enable cURL
					// to write result in a memory area
					// request.setOpt(new
					// curlpp::options::WriteStream(&mediaSourceFileStream));

					// which timeout we have to use here???
					// request.setOpt(new
					// curlpp::options::Timeout(curlTimeoutInSeconds));

					curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
					curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
					request.setOpt(curlDownloadCallbackFunction);
					request.setOpt(curlDownloadDataData);

					// localSourceReferenceURL:
					// ftp://user:password@host:port/path nel caso in cui
					// 'password' contenga '@', questo deve essere encodato con
					// %40
					request.setOpt(new curlpp::options::Url(localSourceReferenceURL));
			if (StringUtils::startWith(localSourceReferenceURL, "https"))
					// string httpsPrefix("https");
					// if (localSourceReferenceURL.size() >= httpsPrefix.size() &&
					// 	0 == localSourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix))
					{
						// disconnect if we can't validate server's cert
						bool bSslVerifyPeer = false;
						curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
						request.setOpt(sslVerifyPeer);

						curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
						request.setOpt(sslVerifyHost);
					}

					chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
					double lastPercentageUpdated = -1.0;
					curlpp::types::ProgressFunctionFunctor functor = bind(
						&MMSEngineProcessor::progressDownloadCallback, this, ingestionJobKey, lastProgressUpdate, lastPercentageUpdated,
						downloadingStoppedByUser, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4
					);
					request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
					request.setOpt(new curlpp::options::NoProgress(0L));

					SPDLOG_INFO(
						string() + "Downloading media file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL
					);
					request.perform();

					(curlDownloadData.mediaSourceFileStream).close();
				}
				else
				{
					_logger->warn(
						string() + "Coming from a download failure, trying to Resume" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey)
					);

					// FILE *mediaSourceFileStream =
					// fopen(destBinaryPathName.c_str(), "wb+");
					long long fileSize;
					{
						ofstream mediaSourceFileStream(destBinaryPathName, ofstream::binary | ofstream::app);
						fileSize = mediaSourceFileStream.tellp();
						mediaSourceFileStream.close();
					}

					CurlDownloadData curlDownloadData;
					curlDownloadData.ingestionJobKey = ingestionJobKey;
					curlDownloadData.destBinaryPathName = destBinaryPathName;
					curlDownloadData.maxChunkFileSize = _downloadChunkSizeInMegaBytes * 1000000;

					curlDownloadData.currentChunkNumber = fileSize % curlDownloadData.maxChunkFileSize;
					// fileSize = curlDownloadData.currentChunkNumber *
					// curlDownloadData.maxChunkFileSize;
					curlDownloadData.currentTotalSize = fileSize;

					SPDLOG_INFO(
						"Coming from a download failure, trying to Resume"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", destBinaryPathName: {}"
						", curlDownloadData.currentTotalSize/fileSize: {}"
						", curlDownloadData.currentChunkNumber: {}",
						_processorIdentifier, ingestionJobKey, destBinaryPathName, fileSize, curlDownloadData.currentChunkNumber
					);

					curlpp::Cleanup cleaner;
					curlpp::Easy request;

					// Set the writer callback to enable cURL
					// to write result in a memory area
					// request.setOpt(new
					// curlpp::options::WriteStream(&mediaSourceFileStream));

					curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
					curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
					request.setOpt(curlDownloadCallbackFunction);
					request.setOpt(curlDownloadDataData);

					// which timeout we have to use here???
					// request.setOpt(new
					// curlpp::options::Timeout(curlTimeoutInSeconds));

					// Setting the URL to retrive.
					request.setOpt(new curlpp::options::Url(localSourceReferenceURL));
					// string httpsPrefix("https");
					// if (localSourceReferenceURL.size() >= httpsPrefix.size() &&
					// 	0 == localSourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix))
			if (StringUtils::startWith(localSourceReferenceURL, "https"))
					{
						SPDLOG_INFO(string() + "Setting SslEngineDefault" + ", _processorIdentifier: " + to_string(_processorIdentifier));
						request.setOpt(new curlpp::options::SslEngineDefault());
					}

					chrono::system_clock::time_point lastTimeProgressUpdate = chrono::system_clock::now();
					double lastPercentageUpdated = -1.0;
					curlpp::types::ProgressFunctionFunctor functor = bind(
						&MMSEngineProcessor::progressDownloadCallback, this, ingestionJobKey, lastTimeProgressUpdate, lastPercentageUpdated,
						downloadingStoppedByUser, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4
					);
					request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
					request.setOpt(new curlpp::options::NoProgress(0L));

					if (fileSize > 2 * 1000 * 1000 * 1000)
						request.setOpt(new curlpp::options::ResumeFromLarge(fileSize));
					else
						request.setOpt(new curlpp::options::ResumeFrom(fileSize));

					SPDLOG_INFO(
						string() + "Resume Download media file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL +
						", resuming from fileSize: " + to_string(fileSize)
					);
					request.perform();

					(curlDownloadData.mediaSourceFileStream).close();
				}

				if (localM3u8TarGzOrM3u8Streaming == 1)
				{
					try
					{
						// by a convention, the directory inside the tar file
						// has to be named as 'content'
						string sourcePathName = "/content.tar.gz";

						_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
							ingestionJobKey, destBinaryPathName, workspaceIngestionRepository, sourcePathName
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments "
													 "failed") +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", localSourceReferenceURL: " + localSourceReferenceURL;

						SPDLOG_ERROR(string() + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				downloadingCompleted = true;

				SPDLOG_INFO(
					string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", destBinaryPathName: " + destBinaryPathName +
					", downloadingCompleted: " + to_string(downloadingCompleted)
				);
				_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, downloadingCompleted);
			}
			catch (curlpp::LogicError &e)
			{
				SPDLOG_ERROR(
					"Download failed (LogicError)"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", localSourceReferenceURL: {}"
					", downloadingStoppedByUser: {}"
					", exception: {}",
					_processorIdentifier, ingestionJobKey, localSourceReferenceURL, downloadingStoppedByUser, e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_ERROR(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
							);
						}

						return;
					}
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
			catch (curlpp::RuntimeError &e)
			{
				SPDLOG_ERROR(
					"Download failed (RuntimeError)"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", localSourceReferenceURL: {}"
					", downloadingStoppedByUser: {}"
					", exception: {}",
					_processorIdentifier, ingestionJobKey, localSourceReferenceURL, downloadingStoppedByUser, e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_INFO(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
							);
						}

						return;
					}
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
			catch (runtime_error e)
			{
				SPDLOG_ERROR(
					string() + "Download failed (runtime_error)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL +
					", exception: " + e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_INFO(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
							);
						}

						return;
					}
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
			catch (exception e)
			{
				SPDLOG_ERROR(
					string() + "Download failed (exception)" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
					to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL + ", exception: " + e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_INFO(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
							);
						}

						return;
					}
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
		}
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
			placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4
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

void MMSEngineProcessor::postVideoOnFacebook(
	string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string facebookConfigurationLabel,
	string facebookDestination, string facebookNodeId
)
{

	string facebookURL;
	string sResponse;

	try
	{
		SPDLOG_INFO(
			string() + "postVideoOnFacebook" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName + ", sizeInBytes: " + to_string(sizeInBytes) +
			", facebookDestination: " + facebookDestination + ", facebookConfigurationLabel: " + facebookConfigurationLabel
		);

		string facebookToken;
		if (facebookDestination == "Page")
			facebookToken = getFacebookPageToken(ingestionJobKey, workspace, facebookConfigurationLabel, facebookNodeId);
		else // if (facebookDestination == "User")
			facebookToken = _mmsEngineDBFacade->getFacebookUserAccessTokenByConfigurationLabel(workspace->_workspaceKey, facebookConfigurationLabel);
		/* 2023-01-08: capire se bisogna recuperare un altro tipo di token
		else if (facebookDestination == "Event")
		{
		}
		else // if (facebookDestination == "Group")
		{
		}
		*/

		string fileFormat;
		{
			size_t extensionIndex = mmsAssetPathName.find_last_of(".");
			if (extensionIndex == string::npos)
			{
				string errorMessage = string() +
									  "No fileFormat (extension of the file) found in "
									  "mmsAssetPathName" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
		}

		/*
			details:
		   https://developers.facebook.com/docs/video-api/guides/publishing/

			curl \
				-X POST
		   "https://graph-video.facebook.com/v2.3/1533641336884006/videos"  \
				-F "access_token=XXXXXXXXX" \
				-F "upload_phase=start" \
				-F "file_size=152043520"

				{"upload_session_id":"1564747013773438","video_id":"1564747010440105","start_offset":"0","end_offset":"52428800"}
		*/
		string uploadSessionId;
		string videoId;
		int64_t startOffset;
		int64_t endOffset;
		// start
		{
			string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";

			facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIVideoHostName + ":" + to_string(_facebookGraphAPIPort) + facebookURI;

			vector<pair<string, string>> formData;
			formData.push_back(make_pair("access_token", facebookToken));
			formData.push_back(make_pair("upload_phase", "start"));
			formData.push_back(make_pair("file_size", to_string(sizeInBytes)));

			json facebookResponseRoot =
				MMSCURL::httpPostFormDataAndGetJson(_logger, ingestionJobKey, facebookURL, formData, _facebookGraphAPITimeoutInSeconds);

			string field = "upload_session_id";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			uploadSessionId = JSONUtils::asString(facebookResponseRoot, field, "");

			field = "video_id";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			videoId = JSONUtils::asString(facebookResponseRoot, field, "");

			field = "start_offset";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sStartOffset = JSONUtils::asString(facebookResponseRoot, field, "");
			startOffset = stoll(sStartOffset);

			field = "end_offset";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sEndOffset = JSONUtils::asString(facebookResponseRoot, field, "");
			endOffset = stoll(sEndOffset);
		}

		while (startOffset < endOffset)
		{
			/*
				curl \
					-X POST
			   "https://graph-video.facebook.com/v2.3/1533641336884006/videos" \
					-F "access_token=XXXXXXX" \
					-F "upload_phase=transfer" \
					-F “start_offset=0" \
					-F "upload_session_id=1564747013773438" \
					-F "video_file_chunk=@chunk1.mp4"
			*/
			// transfer
			{
				string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";

				facebookURL =
					_facebookGraphAPIProtocol + "://" + _facebookGraphAPIVideoHostName + ":" + to_string(_facebookGraphAPIPort) + facebookURI;

				string mediaContentType = string("video") + "/" + fileFormat;

				vector<pair<string, string>> formData;
				formData.push_back(make_pair("access_token", facebookToken));
				formData.push_back(make_pair("upload_phase", "transfer"));
				formData.push_back(make_pair("start_offset", to_string(startOffset)));
				formData.push_back(make_pair("upload_session_id", uploadSessionId));

				json facebookResponseRoot = MMSCURL::httpPostFileByFormDataAndGetJson(
					_logger, ingestionJobKey, facebookURL, formData, _facebookGraphAPITimeoutInSeconds, mmsAssetPathName, sizeInBytes,
					mediaContentType,
					1,	// maxRetryNumber
					15, // secondsToWaitBeforeToRetry
					startOffset, endOffset
				);

				string field = "start_offset";
				if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field +
										  ", facebookResponseRoot: " + JSONUtils::toString(facebookResponseRoot);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sStartOffset = JSONUtils::asString(facebookResponseRoot, field, "");
				startOffset = stoll(sStartOffset);

				field = "end_offset";
				if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sEndOffset = JSONUtils::asString(facebookResponseRoot, field, "");
				endOffset = stoll(sEndOffset);
			}
		}

		/*
			curl \
				-X POST
		   "https://graph-video.facebook.com/v2.3/1533641336884006/videos"  \
				-F "access_token=XXXXXXXX" \
				-F "upload_phase=finish" \
				-F "upload_session_id=1564747013773438"

			{"success":true}
		*/
		// finish: pubblica il video e mettilo in coda per la codifica asincrona
		bool success;
		{
			string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";

			facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIVideoHostName + ":" + to_string(_facebookGraphAPIPort) + facebookURI;

			vector<pair<string, string>> formData;
			formData.push_back(make_pair("access_token", facebookToken));
			formData.push_back(make_pair("upload_phase", "finish"));
			formData.push_back(make_pair("upload_session_id", uploadSessionId));

			json facebookResponseRoot =
				MMSCURL::httpPostFormDataAndGetJson(_logger, ingestionJobKey, facebookURL, formData, _facebookGraphAPITimeoutInSeconds);

			string field = "success";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			success = JSONUtils::asBool(facebookResponseRoot, field, false);

			if (!success)
			{
				string errorMessage = string() + "Post Video on Facebook failed" + ", Field: " + field + ", success: " + to_string(success) +
									  ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = string() + "Post video on Facebook failed (runtime_error)" + ", facebookURL: " + facebookURL +
							  ", exception: " + e.what() + ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "Post video on Facebook failed (exception)" + ", facebookURL: " + facebookURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

size_t curlUploadVideoOnYouTubeCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	MMSEngineProcessor::CurlUploadYouTubeData *curlUploadData = (MMSEngineProcessor::CurlUploadYouTubeData *)f;

	auto logger = spdlog::get("mmsEngineService");

	int64_t currentFilePosition = curlUploadData->mediaSourceFileStream.tellg();

	/*
	logger->info(string() + "curlUploadVideoOnYouTubeCallback"
		+ ", currentFilePosition: " + to_string(currentFilePosition)
		+ ", size: " + to_string(size)
		+ ", nmemb: " + to_string(nmemb)
		+ ", curlUploadData->fileSizeInBytes: " +
	to_string(curlUploadData->fileSizeInBytes)
	);
	*/

	if (currentFilePosition + (size * nmemb) <= curlUploadData->fileSizeInBytes)
		curlUploadData->mediaSourceFileStream.read(ptr, size * nmemb);
	else
		curlUploadData->mediaSourceFileStream.read(ptr, curlUploadData->fileSizeInBytes - currentFilePosition);

	int64_t charsRead = curlUploadData->mediaSourceFileStream.gcount();

	return charsRead;
};

void MMSEngineProcessor::postVideoOnYouTube(
	string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string youTubeConfigurationLabel,
	string youTubeTitle, string youTubeDescription, json youTubeTags, int youTubeCategoryId, string youTubePrivacy, bool youTubeMadeForKids
)
{

	string youTubeURL;
	string youTubeUploadURL;
	string sResponse;

	try
	{
		SPDLOG_INFO(
			string() + "postVideoOnYouTubeThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count()) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName + ", sizeInBytes: " + to_string(sizeInBytes) +
			", youTubeConfigurationLabel: " + youTubeConfigurationLabel + ", youTubeTitle: " + youTubeTitle +
			", youTubeDescription: " + youTubeDescription + ", youTubeCategoryId: " + to_string(youTubeCategoryId) +
			", youTubePrivacy: " + youTubePrivacy + ", youTubeMadeForKids: " + to_string(youTubeMadeForKids)
		);

		// 1. get refresh_token from the configuration
		// 2. call google API
		// 3. the response will have the access token to be used
		string youTubeAccessToken = getYouTubeAccessTokenByConfigurationLabel(ingestionJobKey, workspace, youTubeConfigurationLabel);

		string fileFormat;
		{
			size_t extensionIndex = mmsAssetPathName.find_last_of(".");
			if (extensionIndex == string::npos)
			{
				string errorMessage = string() +
									  "No fileFormat (extension of the file) found in "
									  "mmsAssetPathName" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
		}

		/*
			POST
		   /upload/youtube/v3/videos?uploadType=resumable&part=snippet,status,contentDetails
		   HTTP/1.1 Host: www.googleapis.com Authorization: Bearer AUTH_TOKEN
			Content-Length: 278
			Content-Type: application/json; charset=UTF-8
			X-Upload-Content-Length: 3000000
			X-Upload-Content-Type: video/*

			{
			  "snippet": {
				"title": "My video title",
				"description": "This is a description of my video",
				"tags": ["cool", "video", "more keywords"],
				"categoryId": 22
			  },
			  "status": {
				"privacyStatus": "public",
				"embeddable": True,
				"license": "youtube"
			  }
			}

			HTTP/1.1 200 OK
			Location:
		   https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&upload_id=xa298sd_f&part=snippet,status,contentDetails
			Content-Length: 0
		*/
		string videoContentType = "video/*";
		{
			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPIUploadVideoURI;

			string body;
			{
				json bodyRoot;
				json snippetRoot;

				string field = "title";
				snippetRoot[field] = youTubeTitle;

				if (youTubeDescription != "")
				{
					field = "description";
					snippetRoot[field] = youTubeDescription;
				}

				if (youTubeTags != nullptr)
				{
					field = "tags";
					snippetRoot[field] = youTubeTags;
				}

				if (youTubeCategoryId != -1)
				{
					field = "categoryId";
					snippetRoot[field] = youTubeCategoryId;
				}

				field = "snippet";
				bodyRoot[field] = snippetRoot;

				json statusRoot;

				field = "privacyStatus";
				statusRoot[field] = youTubePrivacy;

				field = "selfDeclaredMadeForKids";
				statusRoot[field] = youTubeMadeForKids;

				field = "embeddable";
				statusRoot[field] = true;

				// field = "license";
				// statusRoot[field] = "youtube";

				field = "status";
				bodyRoot[field] = statusRoot;

				body = JSONUtils::toString(bodyRoot);
			}

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Content-Length: " + to_string(body.length());
				headerList.push_back(header);

				header = "X-Upload-Content-Length: " + to_string(sizeInBytes);
				headerList.push_back(header);

				header = string("X-Upload-Content-Type: ") + videoContentType;
				headerList.push_back(header);
			}

			pair<string, string> responseDetails = MMSCURL::httpPostString(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"application/json; charset=UTF-8", // contentType
				headerList
			);

			string sHeaderResponse;
			string sBodyResponse;

			tie(sHeaderResponse, sBodyResponse) = responseDetails;

			if (sHeaderResponse.find("Location: ") == string::npos && sHeaderResponse.find("location: ") == string::npos)
			{
				string errorMessage = string() + "'Location' response header is not present" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", youTubeURL: " + youTubeURL + ", sHeaderResponse: " + sHeaderResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			/* sResponse:
				HTTP/1.1 200 OK
				X-GUploader-UploadID:
			   AEnB2UqO5ml7GRPs5AjsOSPzSGwudclcEFbyXtEK_TLWRhggwxh9gTWBdusefTgmX2ul9axk4ztG_YBWQXGtm1M42Fz9QVE4xA
				Location:
			   https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status,contentDetails&upload_id=AEnB2UqO5ml7GRPs5AjsOSPzSGwudclcEFbyXtEK_TLWRhggwxh9gTWBdusefTgmX2ul9axk4ztG_YBWQXGtm1M42Fz9QVE4xA
				ETag: "XI7nbFXulYBIpL0ayR_gDh3eu1k/bpNRC6h7Ng2_S5XJ6YzbSMF0qXE"
				Vary: Origin
				Vary: X-Origin
				X-Goog-Correlation-Id: FGN7H2Vxp5I
				Cache-Control: no-cache, no-store, max-age=0, must-revalidate
				Pragma: no-cache
				Expires: Mon, 01 Jan 1990 00:00:00 GMT
				Date: Sun, 09 Dec 2018 09:15:41 GMT
				Content-Length: 0
				Server: UploadServer
				Content-Type: text/html; charset=UTF-8
				Alt-Svc: quic=":443"; ma=2592000; v="44,43,39,35"
			 */

			int locationStartIndex = sHeaderResponse.find("Location: ");
			if (locationStartIndex == string::npos)
				locationStartIndex = sHeaderResponse.find("location: ");
			locationStartIndex += string("Location: ").length();
			int locationEndIndex = sHeaderResponse.find("\r", locationStartIndex);
			if (locationEndIndex == string::npos)
				locationEndIndex = sHeaderResponse.find("\n", locationStartIndex);
			if (locationEndIndex == string::npos)
				youTubeUploadURL = sHeaderResponse.substr(locationStartIndex);
			else
				youTubeUploadURL = sHeaderResponse.substr(locationStartIndex, locationEndIndex - locationStartIndex);
		}

		bool contentCompletelyUploaded = false;
		CurlUploadYouTubeData curlUploadData;
		curlUploadData.mediaSourceFileStream.open(mmsAssetPathName, ios::binary);
		curlUploadData.lastByteSent = -1;
		curlUploadData.fileSizeInBytes = sizeInBytes;
		while (!contentCompletelyUploaded)
		{
			/*
				// In case of the first request
				PUT UPLOAD_URL HTTP/1.1
				Authorization: Bearer AUTH_TOKEN
				Content-Length: CONTENT_LENGTH
				Content-Type: CONTENT_TYPE

				BINARY_FILE_DATA

				// in case of resuming
				PUT UPLOAD_URL HTTP/1.1
				Authorization: Bearer AUTH_TOKEN
				Content-Length: REMAINING_CONTENT_LENGTH
				Content-Range: bytes FIRST_BYTE-LAST_BYTE/TOTAL_CONTENT_LENGTH

				PARTIAL_BINARY_FILE_DATA
			*/

			{
				list<string> headerList;
				headerList.push_back(string("Authorization: Bearer ") + youTubeAccessToken);
				if (curlUploadData.lastByteSent == -1)
					headerList.push_back(string("Content-Length: ") + to_string(sizeInBytes));
				else
					headerList.push_back(string("Content-Length: ") + to_string(sizeInBytes - curlUploadData.lastByteSent + 1));
				if (curlUploadData.lastByteSent == -1)
					headerList.push_back(string("Content-Type: ") + videoContentType);
				else
					headerList.push_back(
						string("Content-Range: bytes ") + to_string(curlUploadData.lastByteSent) + "-" + to_string(sizeInBytes - 1) + "/" +
						to_string(sizeInBytes)
					);

				curlpp::Cleanup cleaner;
				curlpp::Easy request;

				{
					curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadVideoOnYouTubeCallback);
					curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadData);
					request.setOpt(curlUploadCallbackFunction);
					request.setOpt(curlUploadDataData);

					bool upload = true;
					request.setOpt(new curlpp::options::Upload(upload));
				}

				request.setOpt(new curlpp::options::CustomRequest{"PUT"});
				request.setOpt(new curlpp::options::Url(youTubeUploadURL));
				request.setOpt(new curlpp::options::Timeout(_youTubeDataAPITimeoutInSecondsForUploadVideo));

				if (_youTubeDataAPIProtocol == "https")
				{
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLCERTPASSWD> SslCertPasswd;
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLKEY> SslKey; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLKEYTYPE> SslKeyType; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLKEYPASSWD> SslKeyPasswd;
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLENGINE> SslEngine; typedef
					//                curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
					//                SslEngineDefault; typedef
					//                curlpp::OptionTrait<long,
					//                CURLOPT_SSLVERSION> SslVersion; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_CAINFO> CaInfo; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_CAPATH> CaPath; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_RANDOM_FILE> RandomFile; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_EGDSOCKET> EgdSocket; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_SSL_CIPHER_LIST> SslCipherList;
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_KRB4LEVEL> Krb4Level;

					// cert is stored PEM coded in file...
					// since PEM is default, we needn't set it for PEM
					// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
					// curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
					// sslCertType("PEM"); equest.setOpt(sslCertType);

					// set the cert for client authentication
					// "testcert.pem"
					// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
					// curlpp::OptionTrait<string, CURLOPT_SSLCERT>
					// sslCert("cert.pem"); request.setOpt(sslCert);

					// sorry, for engine we must set the passphrase
					//   (if the key has one...)
					// const char *pPassphrase = NULL;
					// if(pPassphrase)
					//  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

					// if we use a key stored in a crypto engine,
					//   we must set the key type to "ENG"
					// pKeyType  = "PEM";
					// curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

					// set the private key (file or ID in engine)
					// pKeyName  = "testkey.pem";
					// curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

					// set the file with the certs vaildating the server
					// *pCACertFile = "cacert.pem";
					// curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

					// disconnect if we can't validate server's cert
					bool bSslVerifyPeer = false;
					curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
					request.setOpt(sslVerifyPeer);

					curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
					request.setOpt(sslVerifyHost);

					// request.setOpt(new curlpp::options::SslEngineDefault());
				}

				for (string headerMessage : headerList)
					SPDLOG_INFO(string() + "Adding header message: " + headerMessage);
				request.setOpt(new curlpp::options::HttpHeader(headerList));

				SPDLOG_INFO(
					string() + "Calling youTube (upload)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", youTubeUploadURL: " + youTubeUploadURL
				);
				request.perform();

				long responseCode = curlpp::infos::ResponseCode::get(request);

				SPDLOG_INFO(
					string() + "Called youTube (upload)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
				);

				if (responseCode == 200 || responseCode == 201)
				{
					SPDLOG_INFO(
						string() + "youTube upload successful" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
					);

					contentCompletelyUploaded = true;
				}
				else if (responseCode == 500 || responseCode == 502 || responseCode == 503 || responseCode == 504)
				{
					_logger->warn(
						string() + "youTube upload failed, trying to resume" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
					);

					/*
						PUT UPLOAD_URL HTTP/1.1
						Authorization: Bearer AUTH_TOKEN
						Content-Length: 0
						Content-Range: bytes *\/CONTENT_LENGTH

						308 Resume Incomplete
						Content-Length: 0
						Range: bytes=0-999999
					*/
					{
						list<string> headerList;
						headerList.push_back(string("Authorization: Bearer ") + youTubeAccessToken);
						headerList.push_back(string("Content-Length: 0"));
						headerList.push_back(string("Content-Range: bytes */") + to_string(sizeInBytes));

						curlpp::Cleanup cleaner;
						curlpp::Easy request;

						request.setOpt(new curlpp::options::CustomRequest{"PUT"});
						request.setOpt(new curlpp::options::Url(youTubeUploadURL));
						request.setOpt(new curlpp::options::Timeout(_youTubeDataAPITimeoutInSeconds));

						if (_youTubeDataAPIProtocol == "https")
						{
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLCERTPASSWD>
							//                SslCertPasswd; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLKEY> SslKey; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLKEYTYPE> SslKeyType;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLKEYPASSWD>
							//                SslKeyPasswd; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLENGINE> SslEngine;
							//                typedef
							//                curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
							//                SslEngineDefault; typedef
							//                curlpp::OptionTrait<long,
							//                CURLOPT_SSLVERSION> SslVersion;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_CAINFO> CaInfo; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_CAPATH> CaPath; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_RANDOM_FILE> RandomFile;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_EGDSOCKET> EgdSocket;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSL_CIPHER_LIST>
							//                SslCipherList; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_KRB4LEVEL> Krb4Level;

							// cert is stored PEM coded in file...
							// since PEM is default, we needn't set it for PEM
							// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE,
							// "PEM"); curlpp::OptionTrait<string,
							// CURLOPT_SSLCERTTYPE> sslCertType("PEM");
							// equest.setOpt(sslCertType);

							// set the cert for client authentication
							// "testcert.pem"
							// curl_easy_setopt(curl, CURLOPT_SSLCERT,
							// pCertFile); curlpp::OptionTrait<string,
							// CURLOPT_SSLCERT> sslCert("cert.pem");
							// request.setOpt(sslCert);

							// sorry, for engine we must set the passphrase
							//   (if the key has one...)
							// const char *pPassphrase = NULL;
							// if(pPassphrase)
							//  curl_easy_setopt(curl, CURLOPT_KEYPASSWD,
							//  pPassphrase);

							// if we use a key stored in a crypto engine,
							//   we must set the key type to "ENG"
							// pKeyType  = "PEM";
							// curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE,
							// pKeyType);

							// set the private key (file or ID in engine)
							// pKeyName  = "testkey.pem";
							// curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

							// set the file with the certs vaildating the server
							// *pCACertFile = "cacert.pem";
							// curl_easy_setopt(curl, CURLOPT_CAINFO,
							// pCACertFile);

							// disconnect if we can't validate server's cert
							bool bSslVerifyPeer = false;
							curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
							request.setOpt(sslVerifyPeer);

							curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
							request.setOpt(sslVerifyHost);

							// request.setOpt(new
							// curlpp::options::SslEngineDefault());
						}

						for (string headerMessage : headerList)
							SPDLOG_INFO(string() + "Adding header message: " + headerMessage);
						request.setOpt(new curlpp::options::HttpHeader(headerList));

						ostringstream response;
						request.setOpt(new curlpp::options::WriteStream(&response));

						// store response headers in the response
						// You simply have to set next option to prefix the
						// header to the normal body output.
						request.setOpt(new curlpp::options::Header(true));

						SPDLOG_INFO(
							string() + "Calling youTube check status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", youTubeUploadURL: " + youTubeUploadURL + ", _youTubeDataAPIProtocol: " + _youTubeDataAPIProtocol +
							", _youTubeDataAPIHostName: " + _youTubeDataAPIHostName + ", _youTubeDataAPIPort: " + to_string(_youTubeDataAPIPort)
						);
						request.perform();

						sResponse = response.str();
						long responseCode = curlpp::infos::ResponseCode::get(request);

						SPDLOG_INFO(
							string() + "Called youTube check status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode) + ", sResponse: " + sResponse
						);

						if (responseCode != 308 || sResponse.find("Range: bytes=") == string::npos)
						{
							// error
							string errorMessage(
								string() + "youTube check status failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						/* sResponse:
							HTTP/1.1 308 Resume Incomplete
							X-GUploader-UploadID:
						   AEnB2Ur8jQ5DSbXieg8krXWg0f7Bmawvf6XTacURJ7wbITyXdTv8ZeHpepaUwh6F9DB5TvBCzoS4quZMKegyo2x7H9EJOc6ozQ
							Range: bytes=0-1572863
							X-Range-MD5: d50bc8fc7ecc41926f841085db3909b3
							Content-Length: 0
							Date: Mon, 10 Dec 2018 13:09:51 GMT
							Server: UploadServer
							Content-Type: text/html; charset=UTF-8
							Alt-Svc: quic=":443"; ma=2592000; v="44,43,39,35"
						*/
						int rangeStartIndex = sResponse.find("Range: bytes=");
						rangeStartIndex += string("Range: bytes=").length();
						int rangeEndIndex = sResponse.find("\r", rangeStartIndex);
						if (rangeEndIndex == string::npos)
							rangeEndIndex = sResponse.find("\n", rangeStartIndex);
						string rangeHeader;
						if (rangeEndIndex == string::npos)
							rangeHeader = sResponse.substr(rangeStartIndex);
						else
							rangeHeader = sResponse.substr(rangeStartIndex, rangeEndIndex - rangeStartIndex);

						int rangeStartOffsetIndex = rangeHeader.find("-");
						if (rangeStartOffsetIndex == string::npos)
						{
							// error
							string errorMessage(
								string() + "youTube check status failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								", youTubeUploadURL: " + youTubeUploadURL + ", rangeHeader: " + rangeHeader
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						SPDLOG_INFO(
							string() + "Resuming" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeUploadURL: " + youTubeUploadURL +
							", rangeHeader: " + rangeHeader +
							", rangeHeader.substr(rangeStartOffsetIndex + "
							"1): " +
							rangeHeader.substr(rangeStartOffsetIndex + 1)
						);
						curlUploadData.lastByteSent = stoll(rangeHeader.substr(rangeStartOffsetIndex + 1)) + 1;
						curlUploadData.mediaSourceFileStream.seekg(curlUploadData.lastByteSent, ios::beg);
					}
				}
				else
				{
					// error
					string errorMessage(
						string() + "youTube upload failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
	}
	catch (curlpp::LogicError &e)
	{
		string errorMessage = string() + "Post video on YouTube failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (curlpp::RuntimeError &e)
	{
		string errorMessage = string() + "Post video on YouTube failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (runtime_error e)
	{
		string errorMessage = string() + "Post video on YouTube failed (runtime_error)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "Post video on YouTube failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

string MMSEngineProcessor::getYouTubeAccessTokenByConfigurationLabel(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string youTubeConfigurationLabel
)
{
	string youTubeURL;
	string sResponse;

	try
	{
		tuple<string, string, string> youTubeDetails =
			_mmsEngineDBFacade->getYouTubeDetailsByConfigurationLabel(workspace->_workspaceKey, youTubeConfigurationLabel);

		string youTubeTokenType;
		string youTubeRefreshToken;
		string youTubeAccessToken;
		tie(youTubeTokenType, youTubeRefreshToken, youTubeAccessToken) = youTubeDetails;

		if (youTubeTokenType == "AccessToken")
		{
			SPDLOG_INFO(
				string() + "Using the youTube access token" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", youTubeAccessToken: " + youTubeAccessToken
			);

			return youTubeAccessToken;
		}

		youTubeURL =
			_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPIRefreshTokenURI;

		string body = string("client_id=") + _youTubeDataAPIClientId + "&client_secret=" + _youTubeDataAPIClientSecret +
					  "&refresh_token=" + youTubeRefreshToken + "&grant_type=refresh_token";

		/*
		list<string> headerList;
		{
			// header = "Content-Length: " + to_string(body.length());
			// headerList.push_back(header);

			string header = "Content-Type: application/x-www-form-urlencoded";
			headerList.push_back(header);
		}
		*/

		vector<string> otherHeaders;
		json youTubeResponseRoot = MMSCURL::httpPostStringAndGetJson(
			_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
			"application/x-www-form-urlencoded", // contentType
			otherHeaders
		);

		/*
			{
			  "access_token":
		   "ya29.GlxvBv2JUSUGmxHncG7KK118PHh4IY3ce6hbSRBoBjeXMiZjD53y3ZoeGchIkyJMb2rwQHlp-tQUZcIJ5zrt6CL2iWj-fV_2ArlAOCTy8y2B0_3KeZrbbJYgoFXCYA",
			  "expires_in": 3600,
			  "scope": "https://www.googleapis.com/auth/youtube
		   https://www.googleapis.com/auth/youtube.upload", "token_type":
		   "Bearer"
			}
		*/

		string field = "access_token";
		if (!JSONUtils::isMetadataPresent(youTubeResponseRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		return JSONUtils::asString(youTubeResponseRoot, field, "");
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("youTube refresh token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse + ", e.what(): " + e.what();
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		string errorMessage = string("youTube refresh token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
}

string MMSEngineProcessor::getFacebookPageToken(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string facebookConfigurationLabel, string facebookPageId
)
{
	string facebookURL;
	json responseRoot;

	try
	{
		string userAccessToken =
			_mmsEngineDBFacade->getFacebookUserAccessTokenByConfigurationLabel(workspace->_workspaceKey, facebookConfigurationLabel);

		// curl -i -X GET "https://graph.facebook.com/PAGE-ID?
		// fields=access_token&
		// access_token=USER-ACCESS-TOKEN"

		facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIHostName + ":" + to_string(_facebookGraphAPIPort) + "/" +
					  _facebookGraphAPIVersion + "/" + facebookPageId + "?fields=access_token" + "&access_token=" + curlpp::escape(userAccessToken);

		SPDLOG_INFO(string() + "Retrieve page token" + ", facebookURL: " + facebookURL);

		vector<string> otherHeaders;
		json responseRoot = MMSCURL::httpGetJson(_logger, ingestionJobKey, facebookURL, _mmsAPITimeoutInSeconds, "", "", otherHeaders);

		/*
		{
		"access_token":"PAGE-ACCESS-TOKEN",
		"id":"PAGE-ID"
		}
		*/

		string field = "access_token";
		if (!JSONUtils::isMetadataPresent(responseRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		return JSONUtils::asString(responseRoot, field, "");
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("facebook access token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", facebookURL: " + facebookURL + ", response: " + JSONUtils::toString(responseRoot) + ", e.what(): " + e.what();
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		string errorMessage = string("facebook access token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", facebookURL: " + facebookURL + ", response: " + JSONUtils::toString(responseRoot);
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
}

void MMSEngineProcessor::userHttpCallback(
	int64_t ingestionJobKey, string httpProtocol, string httpHostName, int httpPort, string httpURI, string httpURLParameters, bool formData,
	string httpMethod, long callbackTimeoutInSeconds, json userHeadersRoot, string &httpBody, string userName, string password, int maxRetries
)
{

	string userURL;

	try
	{
		SPDLOG_INFO(
			"userHttpCallback"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", httpProtocol: {}"
			", httpHostName: {}"
			", httpPort: {}"
			", httpURI: {}"
			", httpURLParameters: {}"
			", formData: {}"
			", maxRetries: {}",
			_processorIdentifier, ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, maxRetries
		);

		userURL = httpProtocol + "://" + httpHostName + ":" + to_string(httpPort) + httpURI + (formData ? "" : httpURLParameters);

		vector<string> otherHeaders;
		for (int userHeaderIndex = 0; userHeaderIndex < userHeadersRoot.size(); ++userHeaderIndex)
		{
			string userHeader = JSONUtils::asString(userHeadersRoot[userHeaderIndex]);

			otherHeaders.push_back(userHeader);
		}

		if (httpMethod == "PUT")
		{
			if (formData)
			{
				vector<pair<string, string>> formData;
				{
					json formDataParametersRoot = JSONUtils::toJson(httpBody);
					for (auto &[keyRoot, valRoot] : formDataParametersRoot.items())
					{
						string name = JSONUtils::asString(keyRoot, "", "");
						string value = JSONUtils::asString(valRoot, "", "");

						if (name != "")
							formData.push_back(make_pair(name, value));
					}
				}

				MMSCURL::httpPutFormData(_logger, ingestionJobKey, userURL, formData, callbackTimeoutInSeconds, maxRetries);
			}
			else
			{
				string contentType;
				if (httpBody != "")
					contentType = "application/json";

				MMSCURL::httpPutString(
					_logger, ingestionJobKey, userURL, callbackTimeoutInSeconds, userName, password, httpBody, contentType, otherHeaders, maxRetries
				);
			}
		}
		else if (httpMethod == "POST")
		{
			if (formData)
			{
				vector<pair<string, string>> formData;
				{
					json formDataParametersRoot = JSONUtils::toJson(httpBody);
					for (auto &[keyRoot, valRoot] : formDataParametersRoot.items())
					{
						string name = JSONUtils::asString(keyRoot, "", "");
						string value = JSONUtils::asString(valRoot, "", "");

						if (name != "")
							formData.push_back(make_pair(name, value));
					}
				}

				MMSCURL::httpPostFormData(_logger, ingestionJobKey, userURL, formData, callbackTimeoutInSeconds, maxRetries);
			}
			else
			{
				string contentType;
				if (httpBody != "")
					contentType = "application/json";

				MMSCURL::httpPostString(
					_logger, ingestionJobKey, userURL, callbackTimeoutInSeconds, userName, password, httpBody, contentType, otherHeaders, maxRetries
				);
			}
		}
		else // if (httpMethod == "GET")
		{
			vector<string> otherHeaders;
			MMSCURL::httpGet(_logger, ingestionJobKey, userURL, callbackTimeoutInSeconds, userName, password, otherHeaders, maxRetries);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = string() + "User Callback URL failed (runtime_error)" + ", userURL: " + userURL +
							  ", maxRetries: " + to_string(maxRetries) + ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "User Callback URL failed (exception)" + ", userURL: " + userURL + ", maxRetries: " + to_string(maxRetries) +
							  ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

void MMSEngineProcessor::moveMediaSourceFileThread(
	shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, int m3u8TarGzOrM3u8Streaming, int64_t ingestionJobKey,
	shared_ptr<Workspace> workspace
)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "moveMediaSourceFileThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "moveMediaSourceFileThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
		string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
		// 0: no m3u8
		// 1: m3u8 by .tar.gz
		// 2: m3u8 by streaming (it will be saved as .mp4)
		if (m3u8TarGzOrM3u8Streaming == 1)
			destBinaryPathName = destBinaryPathName + ".tar.gz";

		string movePrefix("move://");
		string mvPrefix("mv://");
		// if (!(sourceReferenceURL.size() >= movePrefix.size() && 0 == sourceReferenceURL.compare(0, movePrefix.size(), movePrefix)) &&
		// 	!(sourceReferenceURL.size() >= mvPrefix.size() && 0 == sourceReferenceURL.compare(0, mvPrefix.size(), mvPrefix)))
			if (!StringUtils::startWith(sourceReferenceURL, movePrefix)
			&& !StringUtils::startWith(sourceReferenceURL, mvPrefix)
			)
		{
			string errorMessage = string("sourceReferenceURL is not a move reference") +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourceReferenceURL: " + sourceReferenceURL;

			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourcePathName;
			if (StringUtils::startWith(sourceReferenceURL, movePrefix))
		// if (sourceReferenceURL.size() >= movePrefix.size() && 0 == sourceReferenceURL.compare(0, movePrefix.size(), movePrefix))
			sourcePathName = sourceReferenceURL.substr(movePrefix.length());
		else
			sourcePathName = sourceReferenceURL.substr(mvPrefix.length());

		SPDLOG_INFO(
			string() + "Moving" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName
		);

		int64_t elapsedInSeconds = MMSStorage::move(ingestionJobKey, sourcePathName, destBinaryPathName, _logger);

		if (m3u8TarGzOrM3u8Streaming)
		{
			try
			{
				_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
					ingestionJobKey, destBinaryPathName, workspaceIngestionRepository, sourcePathName
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed") +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL;

				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey)
			// + ", movingCompleted: " + to_string(true)
			+ ", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName +
			", @MMS MOVE statistics@ - movingDuration (secs): @" + to_string(elapsedInSeconds) + "@"
		);
		_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, true);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "Moving failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			string() + "Moving failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}

void MMSEngineProcessor::copyMediaSourceFileThread(
	shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, int m3u8TarGzOrM3u8Streaming, int64_t ingestionJobKey,
	shared_ptr<Workspace> workspace
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "copyMediaSourceFileThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "copyMediaSourceFileThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
		string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
		// 0: no m3u8
		// 1: m3u8 by .tar.gz
		// 2: m3u8 by streaming (it will be saved as .mp4)
		if (m3u8TarGzOrM3u8Streaming == 1)
			destBinaryPathName = destBinaryPathName + ".tar.gz";

		string copyPrefix("copy://");
		string cpPrefix("cp://");
		if (!(sourceReferenceURL.size() >= copyPrefix.size() && 0 == sourceReferenceURL.compare(0, copyPrefix.size(), copyPrefix)) &&
			!(sourceReferenceURL.size() >= cpPrefix.size() && 0 == sourceReferenceURL.compare(0, cpPrefix.size(), cpPrefix)))
		{
			string errorMessage = string("sourceReferenceURL is not a copy reference") +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourceReferenceURL: " + sourceReferenceURL;

			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourcePathName;
		if (sourceReferenceURL.size() >= copyPrefix.size() && 0 == sourceReferenceURL.compare(0, copyPrefix.size(), copyPrefix))
			sourcePathName = sourceReferenceURL.substr(copyPrefix.length());
		else
			sourcePathName = sourceReferenceURL.substr(cpPrefix.length());

		SPDLOG_INFO(
			string() + "Coping" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName
		);

		chrono::system_clock::time_point startCoping = chrono::system_clock::now();
		fs::copy(sourcePathName, destBinaryPathName, fs::copy_options::recursive);
		chrono::system_clock::time_point endCoping = chrono::system_clock::now();

		if (m3u8TarGzOrM3u8Streaming == 1)
		{
			try
			{
				_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
					ingestionJobKey, destBinaryPathName, workspaceIngestionRepository, sourcePathName
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed") +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL;

				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey)
			// + ", movingCompleted: " + to_string(true)
			+ ", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName +
			", @MMS COPY statistics@ - copingDuration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endCoping - startCoping).count()) + "@"
		);

		_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, true);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}

int MMSEngineProcessor::progressDownloadCallback(
	int64_t ingestionJobKey, chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated, bool &downloadingStoppedByUser,
	double dltotal, double dlnow, double ultotal, double ulnow
)
{

	chrono::system_clock::time_point now = chrono::system_clock::now();

	if (dltotal != 0 && (dltotal == dlnow || now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds)))
	{
		double progress = (dlnow / dltotal) * 100;
		// int downloadingPercentage = floorf(progress * 100) / 100;
		// this is to have one decimal in the percentage
		double downloadingPercentage = ((double)((int)(progress * 10))) / 10;

		SPDLOG_INFO(
			"Download still running"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", downloadingPercentage: {}"
			", lastPercentageUpdated: {}"
			", downloadingStoppedByUser: {}"
			", dltotal: {}"
			", dlnow: {}"
			", ultotal: {}"
			", ulnow: {}",
			_processorIdentifier, ingestionJobKey, downloadingPercentage, lastPercentageUpdated, downloadingStoppedByUser, dltotal, dlnow, ultotal,
			ulnow
		);

		lastTimeProgressUpdate = now;

		if (lastPercentageUpdated != downloadingPercentage)
		{
			SPDLOG_INFO(
				"Update IngestionJob"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", downloadingPercentage: {}",
				_processorIdentifier, ingestionJobKey, downloadingPercentage
			);
			downloadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceDownloadingInProgress(ingestionJobKey, downloadingPercentage);

			lastPercentageUpdated = downloadingPercentage;
		}

		if (downloadingStoppedByUser)
		{
			SPDLOG_INFO(
				"Download canceled by user"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", downloadingPercentage: {}"
				", downloadingStoppedByUser: {}",
				_processorIdentifier, ingestionJobKey, downloadingPercentage, downloadingStoppedByUser
			);

			return 1; // stop downloading
		}
	}

	return 0;
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

			bool warningIfMissing = true;
			tuple<int64_t, int, string, string, int64_t, bool, int64_t> physicalPathDetails = _mmsEngineDBFacade->getSourcePhysicalPath(
				mediaItemKey, warningIfMissing,
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
			"MMS_SignedToken",	// deliveryType,

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
