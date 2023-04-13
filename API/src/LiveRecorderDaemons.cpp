
#include "LiveRecorderDaemons.h"

#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include <sstream>
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/DateTime.h"

LiveRecorderDaemons::LiveRecorderDaemons(
	Json::Value configuration,
	mutex* liveRecordingMutex,                                                                            
	vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>* liveRecordingsCapability,
	shared_ptr<spdlog::logger> logger):
	FFMPEGEncoderBase(configuration, logger)
{
	try
	{
		_liveRecordingMutex			= liveRecordingMutex;
		_liveRecordingsCapability	= liveRecordingsCapability;

		_liveRecorderChunksIngestionThreadShutdown = false;
		_liveRecorderVirtualVODIngestionThreadShutdown = false;

		_liveRecorderChunksIngestionCheckInSeconds =  JSONUtils::asInt(configuration["ffmpeg"], "liveRecorderChunksIngestionCheckInSeconds", 5);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", ffmpeg->liveRecorderChunksIngestionCheckInSeconds: " + to_string(_liveRecorderChunksIngestionCheckInSeconds)
		);

		_liveRecorderVirtualVODRetention = JSONUtils::asString(configuration["ffmpeg"], "liveRecorderVirtualVODRetention", "15m");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", ffmpeg->liveRecorderVirtualVODRetention: " + _liveRecorderVirtualVODRetention
		);
		_liveRecorderVirtualVODIngestionInSeconds = JSONUtils::asInt(configuration["ffmpeg"], "liveRecorderVirtualVODIngestionInSeconds", 5);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", ffmpeg->liveRecorderVirtualVODIngestionInSeconds: " + to_string(_liveRecorderVirtualVODIngestionInSeconds)
		);
	}
	catch(runtime_error e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

LiveRecorderDaemons::~LiveRecorderDaemons()
{
	try
	{
	}
	catch(runtime_error e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

void LiveRecorderDaemons::startChunksIngestionThread()
{

	while(!_liveRecorderChunksIngestionThreadShutdown)
	{
		try
		{
			chrono::system_clock::time_point startAllChannelsIngestionChunks = chrono::system_clock::now();

			lock_guard<mutex> locker(*_liveRecordingMutex);

			for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_childPid != 0)	// running
				{
					_logger->info(__FILEREF__ + "processSegmenterOutput ..."
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						+ ", liveRecording->_segmenterType: " + liveRecording->_segmenterType
					);

					chrono::system_clock::time_point startSingleChannelIngestionChunks = chrono::system_clock::now();

					try
					{
						if (liveRecording->_encodingParametersRoot != Json::nullValue)
						{
							int segmentDurationInSeconds;
							string outputFileFormat;
							{
								string field = "SegmentDuration";
								segmentDurationInSeconds = JSONUtils::asInt(liveRecording->_ingestedParametersRoot, field, -1);

								field = "OutputFileFormat";
								outputFileFormat = JSONUtils::asString(liveRecording->_ingestedParametersRoot, field, "ts");
							}

							tuple<string, int, int64_t> lastRecordedAssetInfo;

							if (liveRecording->_segmenterType == "streamSegmenter")
							{
								lastRecordedAssetInfo = processStreamSegmenterOutput(
									liveRecording->_ingestionJobKey,
									liveRecording->_encodingJobKey,
									liveRecording->_streamSourceType,
									liveRecording->_externalEncoder,
									segmentDurationInSeconds, outputFileFormat,                                                                              
									liveRecording->_encodingParametersRoot,
									liveRecording->_ingestedParametersRoot,

									liveRecording->_chunksTranscoderStagingContentsPath,
									liveRecording->_chunksNFSStagingContentsPath,
									liveRecording->_segmentListFileName,
									liveRecording->_recordedFileNamePrefix,
									liveRecording->_lastRecordedAssetFileName,
									liveRecording->_lastRecordedAssetDurationInSeconds,
									liveRecording->_lastRecordedSegmentUtcStartTimeInMillisecs);
							}
							else // if (liveRecording->_segmenterType == "hlsSegmenter")
							{
								lastRecordedAssetInfo = processHLSSegmenterOutput(
									liveRecording->_ingestionJobKey,
									liveRecording->_encodingJobKey,
									liveRecording->_streamSourceType,
									liveRecording->_externalEncoder,
									segmentDurationInSeconds, outputFileFormat,                                                                              
									liveRecording->_encodingParametersRoot,
									liveRecording->_ingestedParametersRoot,

									liveRecording->_chunksTranscoderStagingContentsPath,
									liveRecording->_chunksNFSStagingContentsPath,
									liveRecording->_segmentListFileName,
									liveRecording->_recordedFileNamePrefix,
									liveRecording->_lastRecordedAssetFileName,
									liveRecording->_lastRecordedAssetDurationInSeconds,
									liveRecording->_lastRecordedSegmentUtcStartTimeInMillisecs);
							}

							tie(liveRecording->_lastRecordedAssetFileName,
								liveRecording->_lastRecordedAssetDurationInSeconds,
								liveRecording->_lastRecordedSegmentUtcStartTimeInMillisecs)
								= lastRecordedAssetInfo;
							// liveRecording->_lastRecordedAssetFileName			= lastRecordedAssetInfo.first;
							// liveRecording->_lastRecordedAssetDurationInSeconds	= lastRecordedAssetInfo.second;
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("processSegmenterOutput failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("processSegmenterOutput failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}

					_logger->info(__FILEREF__ + "Single Channel Ingestion Chunks"
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						+ ", @MMS statistics@ - elapsed time: @" + to_string(
							chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
								- startSingleChannelIngestionChunks).count()
						) + "@"
					);
				}
			}

			_logger->info(__FILEREF__ + "All Channels Ingestion Chunks"
				+ ", @MMS statistics@ - elapsed time: @" + to_string(
					chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
						- startAllChannelsIngestionChunks).count()
				) + "@"
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("liveRecorderChunksIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("liveRecorderChunksIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		this_thread::sleep_for(chrono::seconds(_liveRecorderChunksIngestionCheckInSeconds));
	}
}

void LiveRecorderDaemons::stopChunksIngestionThread()
{
	_liveRecorderChunksIngestionThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_liveRecorderChunksIngestionCheckInSeconds));
}

void LiveRecorderDaemons::startVirtualVODIngestionThread()
{

	while(!_liveRecorderVirtualVODIngestionThreadShutdown)
	{
		int virtualVODsNumber = 0;

		try
		{
			// this is to have a copy of LiveRecording
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> copiedRunningLiveRecordingCapability;

			// this is to have access to running and _proxyStart
			//	to check if it is changed. In case the process is killed, it will access
			//	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> sourceLiveRecordingCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveRecordingVirtualVODCounter = 0;
			{
				lock_guard<mutex> locker(*_liveRecordingMutex);

				int liveRecordingNotVirtualVODCounter = 0;

				for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording: *_liveRecordingsCapability)
				{
					if (liveRecording->_childPid != 0 && liveRecording->_virtualVOD
						&& startClone > liveRecording->_recordingStart)
					{
						liveRecordingVirtualVODCounter++;

						copiedRunningLiveRecordingCapability.push_back(
							liveRecording->cloneForMonitorAndVirtualVOD());
						sourceLiveRecordingCapability.push_back(
                            liveRecording);
					}
					else
					{
						liveRecordingNotVirtualVODCounter++;
					}
				}
				_logger->info(__FILEREF__ + "virtualVOD, numbers"
					+ ", total LiveRecording: " + to_string(liveRecordingVirtualVODCounter
						+ liveRecordingNotVirtualVODCounter)
					+ ", liveRecordingVirtualVODCounter: " + to_string(liveRecordingVirtualVODCounter)
					+ ", liveRecordingNotVirtualVODCounter: " + to_string(liveRecordingNotVirtualVODCounter)
				);
			}
			_logger->info(__FILEREF__ + "virtualVOD clone"
				+ ", copiedRunningLiveRecordingCapability.size: " + to_string(copiedRunningLiveRecordingCapability.size())
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - startClone).count())
			);

			chrono::system_clock::time_point startAllChannelsVirtualVOD = chrono::system_clock::now();

			for (int liveRecordingIndex = 0;
				liveRecordingIndex < copiedRunningLiveRecordingCapability.size();
				liveRecordingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveRecording> copiedLiveRecording
					= copiedRunningLiveRecordingCapability[liveRecordingIndex];
				shared_ptr<FFMPEGEncoderBase::LiveRecording> sourceLiveRecording
					= sourceLiveRecordingCapability[liveRecordingIndex];

				_logger->info(__FILEREF__ + "virtualVOD"
					+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
					+ ", channelLabel: " + copiedLiveRecording->_channelLabel
				);

				if (sourceLiveRecording->_childPid == 0 ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "virtualVOD. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_childPid: " + to_string(sourceLiveRecording->_childPid)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): "
							+ to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): "
							+ to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				{
					chrono::system_clock::time_point startSingleChannelVirtualVOD = chrono::system_clock::now();

					virtualVODsNumber++;

					_logger->info(__FILEREF__ + "buildAndIngestVirtualVOD ..."
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", externalEncoder: " + to_string(copiedLiveRecording->_externalEncoder)
						+ ", childPid: " + to_string(copiedLiveRecording->_childPid)
						+ ", virtualVOD: " + to_string(copiedLiveRecording->_virtualVOD)
						+ ", virtualVODsNumber: " + to_string(virtualVODsNumber)
						+ ", monitorVirtualVODManifestDirectoryPath: "
							+ copiedLiveRecording->_monitorVirtualVODManifestDirectoryPath
						+ ", monitorVirtualVODManifestFileName: "
							+ copiedLiveRecording->_monitorVirtualVODManifestFileName
						+ ", virtualVODStagingContentsPath: "
							+ copiedLiveRecording->_virtualVODStagingContentsPath
					);

					long segmentsNumber = 0;

					try
					{
						int64_t recordingCode = JSONUtils::asInt64(
							copiedLiveRecording->_ingestedParametersRoot, "recordingCode", 0);
						string ingestionJobLabel = JSONUtils::asString(copiedLiveRecording->_encodingParametersRoot,
							"ingestionJobLabel", "");
						string liveRecorderVirtualVODUniqueName = ingestionJobLabel + "("
							+ to_string(recordingCode) + "_" + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ")";

						int64_t userKey;
						string apiKey;
						{
							string field = "internalMMS";
							if (JSONUtils::isMetadataPresent(copiedLiveRecording->_ingestedParametersRoot, field))
							{
								Json::Value internalMMSRoot = copiedLiveRecording->_ingestedParametersRoot[field];

								field = "credentials";
								if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
								{
									Json::Value credentialsRoot = internalMMSRoot[field];

									field = "userKey";
									userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

									field = "apiKey";
									string apiKeyEncrypted = JSONUtils::asString(credentialsRoot, field, "");
									apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
								}
							}
						}

						string mmsWorkflowIngestionURL;
						string mmsBinaryIngestionURL;
						{
							string field = "mmsWorkflowIngestionURL";
							if (!JSONUtils::isMetadataPresent(copiedLiveRecording->_encodingParametersRoot,
								field))
							{
								string errorMessage = __FILEREF__ + "Field is not present or it is null"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", Field: " + field;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
							mmsWorkflowIngestionURL = JSONUtils::asString(copiedLiveRecording->_encodingParametersRoot, field, "");

							field = "mmsBinaryIngestionURL";
							if (!JSONUtils::isMetadataPresent(copiedLiveRecording->_encodingParametersRoot,
								field))
							{
								string errorMessage = __FILEREF__ + "Field is not present or it is null"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", Field: " + field;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
							mmsBinaryIngestionURL = JSONUtils::asString(copiedLiveRecording->_encodingParametersRoot, field, "");
						}

						segmentsNumber = buildAndIngestVirtualVOD(
							copiedLiveRecording->_ingestionJobKey,
							copiedLiveRecording->_encodingJobKey,
							copiedLiveRecording->_externalEncoder,

							copiedLiveRecording->_monitorVirtualVODManifestDirectoryPath,
							copiedLiveRecording->_monitorVirtualVODManifestFileName,
							copiedLiveRecording->_virtualVODStagingContentsPath,

							recordingCode,
							ingestionJobLabel,
							liveRecorderVirtualVODUniqueName,
							_liveRecorderVirtualVODRetention,
							copiedLiveRecording->_liveRecorderVirtualVODImageMediaItemKey,
							userKey,
							apiKey,
							mmsWorkflowIngestionURL,
							mmsBinaryIngestionURL);
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("buildAndIngestVirtualVOD failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("buildAndIngestVirtualVOD failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}

					_logger->info(__FILEREF__ + "Single Channel Virtual VOD"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", segmentsNumber: " + to_string(segmentsNumber)
						+ ", @MMS statistics@ - elapsed time (secs): @" + to_string(
							chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
								- startSingleChannelVirtualVOD).count()
						) + "@"
					);
				}
			}

			_logger->info(__FILEREF__ + "All Channels Virtual VOD"
				+ ", @MMS statistics@ - elapsed time: @" + to_string(
					chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
						- startAllChannelsVirtualVOD).count()
				) + "@"
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("liveRecorderVirtualVODIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("liveRecorderVirtualVODIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		if (virtualVODsNumber < 5)
			this_thread::sleep_for(chrono::seconds(_liveRecorderVirtualVODIngestionInSeconds * 2));
		else
			this_thread::sleep_for(chrono::seconds(_liveRecorderVirtualVODIngestionInSeconds));
	}
}

void LiveRecorderDaemons::stopVirtualVODIngestionThread()
{
	_liveRecorderVirtualVODIngestionThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_liveRecorderVirtualVODIngestionInSeconds));
}

tuple<string, double, int64_t> LiveRecorderDaemons::processStreamSegmenterOutput(
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
	double lastRecordedAssetDurationInSeconds,
	int64_t lastRecordedSegmentUtcStartTimeInMillisecs)
{

	// it is assigned to lastRecordedAssetFileName because in case no new files are present,
	// the same lastRecordedAssetFileName has to be returned
	string newLastRecordedAssetFileName = lastRecordedAssetFileName;
	double newLastRecordedAssetDurationInSeconds = lastRecordedAssetDurationInSeconds;
	int64_t newLastRecordedSegmentUtcStartTimeInMillisecs = lastRecordedSegmentUtcStartTimeInMillisecs;
    try
    {
		_logger->info(__FILEREF__ + "processStreamSegmenterOutput"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			// + ", highAvailability: " + to_string(highAvailability)
			// + ", main: " + to_string(main)
			+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
			+ ", outputFileFormat: " + outputFileFormat
			+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
			+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
			+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
			+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
			+ ", lastRecordedAssetDurationInSeconds: " + to_string(lastRecordedAssetDurationInSeconds)
		);

		ifstream segmentList(chunksTranscoderStagingContentsPath + segmentListFileName);
		if (!segmentList)
        {
            string errorMessage = __FILEREF__ + "No segment list file found yet"
				+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
				+ ", segmentListFileName: " + segmentListFileName
				+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName;
            _logger->warn(errorMessage);

			return make_tuple(lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs);
            // throw runtime_error(errorMessage);
        }

		bool reachedNextFileToProcess = false;
		string currentRecordedAssetFileName;
		while(getline(segmentList, currentRecordedAssetFileName))
		{
			if (!reachedNextFileToProcess)
			{
				if (lastRecordedAssetFileName == "")
				{
					reachedNextFileToProcess = true;
				}
				else if (currentRecordedAssetFileName == lastRecordedAssetFileName)
				{
					reachedNextFileToProcess = true;

					continue;
				}
				else
				{
					continue;
				}
			}

			_logger->info(__FILEREF__ + "processing LiveRecorder file"
				+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName);

			if (!fs::exists(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName))
			{
				// it could be the scenario where mmsEngineService is restarted,
				// the segments list file still contains obsolete filenames
				_logger->error(__FILEREF__ + "file not existing"
						", currentRecordedAssetPathName: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName
				);

				continue;
			}

			bool isFirstChunk = (lastRecordedAssetFileName == "");

			time_t utcCurrentRecordedFileCreationTime = getMediaLiveRecorderStartTime(
				ingestionJobKey, encodingJobKey, currentRecordedAssetFileName, segmentDurationInSeconds,
				isFirstChunk);

			/*
			time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
			if (utcNow - utcCurrentRecordedFileCreationTime < _secondsToWaitNFSBuffers)
			{
				long secondsToWait = _secondsToWaitNFSBuffers
					- (utcNow - utcCurrentRecordedFileCreationTime);

				_logger->info(__FILEREF__ + "processing LiveRecorder file too young"
					+ ", secondsToWait: " + to_string(secondsToWait));
				this_thread::sleep_for(chrono::seconds(secondsToWait));
			}
			*/

			bool ingestionRowToBeUpdatedAsSuccess = isLastLiveRecorderFile(
					ingestionJobKey, encodingJobKey, utcCurrentRecordedFileCreationTime,
					chunksTranscoderStagingContentsPath, recordedFileNamePrefix,
					segmentDurationInSeconds, isFirstChunk);
			_logger->info(__FILEREF__ + "isLastLiveRecorderFile"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
					+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
					+ ", ingestionRowToBeUpdatedAsSuccess: " + to_string(ingestionRowToBeUpdatedAsSuccess));

			newLastRecordedAssetFileName = currentRecordedAssetFileName;
			newLastRecordedAssetDurationInSeconds = segmentDurationInSeconds;
			newLastRecordedSegmentUtcStartTimeInMillisecs = utcCurrentRecordedFileCreationTime * 1000;

			/*
			 * 2019-10-17: we just saw that, even if the real duration is 59 seconds,
			 * next utc time inside the chunk file name is still like +60 from the previuos chunk utc.
			 * For this reason next code was commented.
			try
			{
				int64_t durationInMilliSeconds;


				_logger->info(__FILEREF__ + "Calling ffmpeg.getMediaInfo"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", chunksTranscoderStagingContentsPath + currentRecordedAssetFileName: "
						+ (chunksTranscoderStagingContentsPath + currentRecordedAssetFileName)
				);
				FFMpeg ffmpeg (_configuration, _logger);
				tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
					ffmpeg.getMediaInfo(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);

				tie(durationInMilliSeconds, ignore,
					ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore, ignore) = mediaInfo;

				newLastRecordedAssetDurationInSeconds = durationInMilliSeconds / 1000;

				if (newLastRecordedAssetDurationInSeconds != segmentDurationInSeconds)
				{
					_logger->warn(__FILEREF__ + "segment duration is different from file duration"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", durationInMilliSeconds: " + to_string (durationInMilliSeconds)
						+ ", newLastRecordedAssetDurationInSeconds: "
							+ to_string (newLastRecordedAssetDurationInSeconds)
						+ ", segmentDurationInSeconds: " + to_string (segmentDurationInSeconds)
					);
				}
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", chunksTranscoderStagingContentsPath + currentRecordedAssetFileName: "
						+ (chunksTranscoderStagingContentsPath + currentRecordedAssetFileName)
				);
			}
			*/

			time_t utcCurrentRecordedFileLastModificationTime =
				utcCurrentRecordedFileCreationTime + newLastRecordedAssetDurationInSeconds;
			/*
			time_t utcCurrentRecordedFileLastModificationTime = getMediaLiveRecorderEndTime(
				currentRecordedAssetPathName);
			*/

			string uniqueName;
			{
				int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);

				uniqueName = to_string(recordingCode);
				uniqueName += " - ";
				uniqueName += to_string(utcCurrentRecordedFileCreationTime);
			}

			string ingestionJobLabel = JSONUtils::asString(encodingParametersRoot, "ingestionJobLabel", "");

			// UserData
			Json::Value userDataRoot;
			{
				if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserData"))
					userDataRoot = ingestedParametersRoot["UserData"];

				Json::Value mmsDataRoot;
				mmsDataRoot["dataType"] = "liveRecordingChunk";
				/*
				mmsDataRoot["streamSourceType"] = streamSourceType;
				if (streamSourceType == "IP_PULL")
					mmsDataRoot["ipConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
				else if (streamSourceType == "TV")
					mmsDataRoot["satConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
				else // if (streamSourceType == "IP_PUSH")
				*/
				{
					int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot,
						"recordingCode", 0);
					mmsDataRoot["recordingCode"] = recordingCode;
				}
				mmsDataRoot["ingestionJobLabel"] = ingestionJobLabel;
				// mmsDataRoot["main"] = main;
				mmsDataRoot["main"] = true;
				// if (!highAvailability)
				{
					bool validated = true;
					mmsDataRoot["validated"] = validated;
				}
				mmsDataRoot["ingestionJobKey"] = (int64_t) (ingestionJobKey);
				mmsDataRoot["utcPreviousChunkStartTime"] =
					(time_t) (utcCurrentRecordedFileCreationTime - lastRecordedAssetDurationInSeconds);
				mmsDataRoot["utcChunkStartTime"] = utcCurrentRecordedFileCreationTime;
				mmsDataRoot["utcChunkEndTime"] = utcCurrentRecordedFileLastModificationTime;

				mmsDataRoot["uniqueName"] = uniqueName;

				userDataRoot["mmsData"] = mmsDataRoot;
			}

			// Title
			string addContentTitle;
			{
				/*
				if (streamSourceType == "IP_PUSH")
				{
					int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot,
						"recordingCode", 0);
					addContentTitle = to_string(recordingCode);
				}
				else
				{
					// 2021-02-03: in this case, we will use the 'ConfigurationLabel' that
					// it is much better that a code. Who will see the title of the chunks will recognize
					// easily the recording
					addContentTitle = ingestedParametersRoot.get("configurationLabel", "").asString();
				}
				*/
				// string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();
				if (ingestionJobLabel == "")
				{
					int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot,
						"recordingCode", 0);
					addContentTitle = to_string(recordingCode);
				}
				else
					addContentTitle = ingestionJobLabel;

				addContentTitle += " - ";

				{
					tm		tmDateTime;
					char	strCurrentRecordedFileTime [64];

					// from utc to local time
					localtime_r (&utcCurrentRecordedFileCreationTime, &tmDateTime);

					/*
					sprintf (strCurrentRecordedFileTime,
						"%04d-%02d-%02d %02d:%02d:%02d",
						tmDateTime. tm_year + 1900,
						tmDateTime. tm_mon + 1,
						tmDateTime. tm_mday,
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);
					*/

					sprintf (strCurrentRecordedFileTime,
						"%02d:%02d:%02d",
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);

					addContentTitle += strCurrentRecordedFileTime;	// local time
				}

				addContentTitle += " - ";

				{
					tm		tmDateTime;
					char	strCurrentRecordedFileTime [64];

					// from utc to local time
					localtime_r (&utcCurrentRecordedFileLastModificationTime, &tmDateTime);

					/*
					sprintf (strCurrentRecordedFileTime,
						"%04d-%02d-%02d %02d:%02d:%02d",
						tmDateTime. tm_year + 1900,
						tmDateTime. tm_mon + 1,
						tmDateTime. tm_mday,
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);
					*/
					sprintf (strCurrentRecordedFileTime,
						"%02d:%02d:%02d",
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);

					addContentTitle += strCurrentRecordedFileTime;	// local time
				}

				// if (!main)
				// 	addContentTitle += " (BCK)";
			}

			if (isFirstChunk)
			{
				_logger->info(__FILEREF__ + "The first asset file name is not ingested because it does not contain the entire period and it will be removed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", currentRecordedAssetPathName: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName
					+ ", title: " + addContentTitle
				);

				_logger->info(__FILEREF__ + "Remove"
					+ ", currentRecordedAssetPathName: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName
				);

                fs::remove_all(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);
			}
			else
			{
				try
				{
					_logger->info(__FILEREF__ + "ingest Recorded media"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
						+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
						+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
						+ ", addContentTitle: " + addContentTitle
					);

					if (externalEncoder)
						ingestRecordedMediaInCaseOfExternalTranscoder(ingestionJobKey,
							chunksTranscoderStagingContentsPath, currentRecordedAssetFileName,
							addContentTitle, uniqueName, /* highAvailability, */ userDataRoot, outputFileFormat,
							ingestedParametersRoot, encodingParametersRoot);
					else
						ingestRecordedMediaInCaseOfInternalTranscoder(ingestionJobKey,
							chunksTranscoderStagingContentsPath, currentRecordedAssetFileName,
							chunksNFSStagingContentsPath,
							addContentTitle, uniqueName, /* highAvailability, */ userDataRoot, outputFileFormat,
							ingestedParametersRoot, encodingParametersRoot,
							false);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "ingestRecordedMedia failed"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", externalEncoder: " + to_string(externalEncoder)
						+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
						+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
						+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
						+ ", addContentTitle: " + addContentTitle
						+ ", outputFileFormat: " + outputFileFormat
						+ ", e.what(): " + e.what()
					);

					// throw e;
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__ + "ingestRecordedMedia failed"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", externalEncoder: " + to_string(externalEncoder)
						+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
						+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
						+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
						+ ", addContentTitle: " + addContentTitle
						+ ", outputFileFormat: " + outputFileFormat
					);
                
					// throw e;
				}
			}
		}

		if (reachedNextFileToProcess == false)
		{
			// this scenario should never happens, we have only one option when mmEngineService
			// is restarted, the new LiveRecorder is not started and the segments list file
			// contains still old files. So newLastRecordedAssetFileName is initialized
			// with the old file that will never be found once LiveRecorder starts and reset
			// the segment list file
			// In this scenario, we will reset newLastRecordedAssetFileName
			newLastRecordedAssetFileName	= "";
		}
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processStreamSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
			+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processStreamSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
			+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
        );
                
        throw e;
    }

	return make_tuple(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs);
}

tuple<string, double, int64_t> LiveRecorderDaemons::processHLSSegmenterOutput(
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
	double lastRecordedAssetDurationInSeconds,
	int64_t lastRecordedSegmentUtcStartTimeInMillisecs)
{

	string newLastRecordedAssetFileName = lastRecordedAssetFileName;
	double newLastRecordedAssetDurationInSeconds = lastRecordedAssetDurationInSeconds;
	int64_t newLastRecordedSegmentUtcStartTimeInMillisecs = lastRecordedSegmentUtcStartTimeInMillisecs;

    try
    {
		_logger->info(__FILEREF__ + "processHLSSegmenterOutput"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			// + ", highAvailability: " + to_string(highAvailability)
			// + ", main: " + to_string(main)
			+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
			+ ", outputFileFormat: " + outputFileFormat
			+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
			+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
			+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
			+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
			+ ", lastRecordedAssetDurationInSeconds: " + to_string(lastRecordedAssetDurationInSeconds)
		);

		double toBeIngestedSegmentDuration = -1.0;
		int64_t toBeIngestedSegmentUtcStartTimeInMillisecs = -1;
		string toBeIngestedSegmentFileName;
		{
			double currentSegmentDuration = -1.0;
			int64_t currentSegmentUtcStartTimeInMillisecs = -1;
			string currentSegmentFileName;

			bool toBeIngested = false;

			_logger->info(__FILEREF__ + "Reading manifest"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", chunksTranscoderStagingContentsPath + segmentListFileName: " + chunksTranscoderStagingContentsPath + segmentListFileName
			);

			ifstream segmentList;
			segmentList.open(chunksTranscoderStagingContentsPath + segmentListFileName, ifstream::in);
			if (!segmentList)
			{
				string errorMessage = __FILEREF__ + "No segment list file found yet"
					+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
					+ ", segmentListFileName: " + segmentListFileName
					+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName;
				_logger->warn(errorMessage);

				return make_tuple(lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds,
					newLastRecordedSegmentUtcStartTimeInMillisecs);
				// throw runtime_error(errorMessage);
			}

			int ingestionNumber = 0;
			string manifestLine;
			while(getline(segmentList, manifestLine))
			{
				_logger->info(__FILEREF__ + "Reading manifest line"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", manifestLine: " + manifestLine
					+ ", toBeIngested: " + to_string(toBeIngested)
					+ ", toBeIngestedSegmentDuration: " + to_string(toBeIngestedSegmentDuration)
					+ ", toBeIngestedSegmentUtcStartTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcStartTimeInMillisecs)
					+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
					+ ", currentSegmentDuration: " + to_string(currentSegmentDuration)
					+ ", currentSegmentUtcStartTimeInMillisecs: " + to_string(currentSegmentUtcStartTimeInMillisecs)
					+ ", currentSegmentFileName: " + currentSegmentFileName
					+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
					+ ", newLastRecordedAssetFileName: " + newLastRecordedAssetFileName
					+ ", newLastRecordedSegmentUtcStartTimeInMillisecs: " + to_string(newLastRecordedSegmentUtcStartTimeInMillisecs)
				);

				// #EXTINF:14.640000,
				// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
				// <segment file name>

				if (manifestLine.size() == 0)
					continue;

				string durationPrefix ("#EXTINF:");
				string dateTimePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
				if (manifestLine.size() >= durationPrefix.size()
					&& 0 == manifestLine.compare(0, durationPrefix.size(), durationPrefix))
				{
					size_t endOfSegmentDuration = manifestLine.find(",");
					if (endOfSegmentDuration == string::npos)
					{
						string errorMessage = string("wrong manifest line format")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", manifestLine: " + manifestLine
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}

					if (toBeIngested)
						toBeIngestedSegmentDuration = stod(manifestLine.substr(durationPrefix.size(),
							endOfSegmentDuration - durationPrefix.size()));
					else
						currentSegmentDuration = stod(manifestLine.substr(durationPrefix.size(),
							endOfSegmentDuration - durationPrefix.size()));
				}
				else if (manifestLine.size() >= dateTimePrefix.size() && 0 == manifestLine.compare(0, dateTimePrefix.size(),
					dateTimePrefix))
				{
					if (toBeIngested)
						toBeIngestedSegmentUtcStartTimeInMillisecs = DateTime::sDateMilliSecondsToUtc(
							manifestLine.substr(dateTimePrefix.size()));
					else
						currentSegmentUtcStartTimeInMillisecs = DateTime::sDateMilliSecondsToUtc(
							manifestLine.substr(dateTimePrefix.size()));
				}
				else if (manifestLine[0] != '#')
				{
					if (toBeIngested)
						toBeIngestedSegmentFileName = manifestLine;
					else
						currentSegmentFileName = manifestLine;
				}
				else
				{
					_logger->info(__FILEREF__ + "manifest line not used by our algorithm"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", manifestLine: " + manifestLine
					);

					continue;
				}

				if (
					// case 1: we are in the toBeIngested status (part of the playlist after the last ingested segment)
					//	and we are all the details of the new ingested segment
					(
						toBeIngested
						&& toBeIngestedSegmentDuration != -1.0
						&& toBeIngestedSegmentUtcStartTimeInMillisecs != -1
						&& toBeIngestedSegmentFileName != ""
					)
					||
					// case 2: we are NOT in the toBeIngested status
					//	but we just started to ingest (lastRecordedAssetFileName == "")
					//	and we have all the details of the ingested segment
					(
						!toBeIngested
						&& currentSegmentDuration != -1.0
						&& currentSegmentUtcStartTimeInMillisecs != -1
						&& currentSegmentFileName != ""
						&& lastRecordedAssetFileName == ""
					)
				)
				{
					// if we are in case 2, let's initialize variables like we are in case 1
					if (!toBeIngested)
					{
						toBeIngestedSegmentDuration = currentSegmentDuration;
						toBeIngestedSegmentUtcStartTimeInMillisecs = currentSegmentUtcStartTimeInMillisecs;
						toBeIngestedSegmentFileName = currentSegmentFileName;

						toBeIngested = true;
					}

					// ingest the asset and initilize
					// newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds and
					// newLastRecordedSegmentUtcStartTimeInMillisecs
					{
						int64_t toBeIngestedSegmentUtcEndTimeInMillisecs =
							toBeIngestedSegmentUtcStartTimeInMillisecs + (toBeIngestedSegmentDuration * 1000);

						_logger->info(__FILEREF__ + "processing LiveRecorder file"
							+ ", toBeIngestedSegmentDuration: " + to_string(toBeIngestedSegmentDuration)
							+ ", toBeIngestedSegmentUtcStartTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcStartTimeInMillisecs)
							+ ", toBeIngestedSegmentUtcEndTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcEndTimeInMillisecs)
							+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
						);

						if (!fs::exists(chunksTranscoderStagingContentsPath + toBeIngestedSegmentFileName))
						{
							// it could be the scenario where mmsEngineService is restarted,
							// the segments list file still contains obsolete filenames
							_logger->error(__FILEREF__ + "file not existing"
								", currentRecordedAssetPathName: " + chunksTranscoderStagingContentsPath + toBeIngestedSegmentFileName
							);

							return make_tuple(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds,
								newLastRecordedSegmentUtcStartTimeInMillisecs);
						}
						else if (toBeIngestedSegmentUtcStartTimeInMillisecs <= newLastRecordedSegmentUtcStartTimeInMillisecs)
						{
							// toBeIngestedSegmentUtcStartTimeInMillisecs: indica il nuovo RecordedSegmentUtcStartTime
							// newLastRecordedSegmentUtcStartTimeInMillisecs: indica il precedente RecordedSegmentUtcStartTime
							// 2023-03-28: nella registrazione delle partite ho notato che una volta che la partita è terminata,
							//		poichè per sicurezza avevo messo un orario di fine registrazione parecchio piu
							//		avanti, sono ancora stati ingestati 200 media items con orari simili, l'ultimo media item
							//		di soli 30 minuti piu avanti anzicchè 200 minuti piu avanti.
							//		Questo controllo garantisce anche che i tempi 'start time' siano consistenti (sempre crescenti)

							_logger->warn(__FILEREF__ + "media item not ingested because his start time <= last ingested start time"
								+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
								+ ", toBeIngestedSegmentUtcStartTimeInMillisecs (new start time): " + to_string(toBeIngestedSegmentUtcStartTimeInMillisecs)
								+ ", newLastRecordedSegmentUtcStartTimeInMillisecs (previous start time): " + to_string(newLastRecordedSegmentUtcStartTimeInMillisecs)
							);

							return make_tuple(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds,
								newLastRecordedSegmentUtcStartTimeInMillisecs);
						}

						// initialize metadata and ingest the asset
						{
							string uniqueName;
							{
								int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);

								uniqueName = to_string(recordingCode);
								uniqueName += " - ";
								uniqueName += to_string(toBeIngestedSegmentUtcStartTimeInMillisecs);
							}

							string ingestionJobLabel = JSONUtils::asString(encodingParametersRoot, "ingestionJobLabel", "");

							// UserData
							Json::Value userDataRoot;
							{
								if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserData"))
									userDataRoot = ingestedParametersRoot["UserData"];

								Json::Value mmsDataRoot;
								mmsDataRoot["dataType"] = "liveRecordingChunk";
								/*
								mmsDataRoot["streamSourceType"] = streamSourceType;
								if (streamSourceType == "IP_PULL")
									mmsDataRoot["ipConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
								else if (streamSourceType == "TV")
									mmsDataRoot["satConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
								else // if (streamSourceType == "IP_PUSH")
								*/
								{
									int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot,
										"recordingCode", 0);
									mmsDataRoot["recordingCode"] = recordingCode;
								}
								mmsDataRoot["ingestionJobLabel"] = ingestionJobLabel;
								// mmsDataRoot["main"] = main;
								mmsDataRoot["main"] = true;
								// if (!highAvailability)
								{
									bool validated = true;
									mmsDataRoot["validated"] = validated;
								}
								mmsDataRoot["ingestionJobKey"] = (int64_t) (ingestionJobKey);
								/*
								mmsDataRoot["utcPreviousChunkStartTime"] =
									(time_t) (utcCurrentRecordedFileCreationTime - lastRecordedAssetDurationInSeconds);
								*/
								mmsDataRoot["utcChunkStartTime"] =
									(int64_t) (toBeIngestedSegmentUtcStartTimeInMillisecs / 1000);
								mmsDataRoot["utcStartTimeInMilliSecs"] =
									toBeIngestedSegmentUtcStartTimeInMillisecs;

								mmsDataRoot["utcChunkEndTime"] =
									(int64_t) (toBeIngestedSegmentUtcEndTimeInMillisecs / 1000);
								mmsDataRoot["utcEndTimeInMilliSecs"] =
									toBeIngestedSegmentUtcEndTimeInMillisecs;

								mmsDataRoot["uniqueName"] = uniqueName;

								userDataRoot["mmsData"] = mmsDataRoot;
							}

							// Title
							string addContentTitle;
							{
								if (ingestionJobLabel == "")
								{
									int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot,
										"recordingCode", 0);
									addContentTitle = to_string(recordingCode);
								}
								else
									addContentTitle = ingestionJobLabel;

								addContentTitle += " - ";

								{
									tm		tmDateTime;
									char	strCurrentRecordedFileTime [64];

									time_t toBeIngestedSegmentUtcStartTimeInSeconds =
										toBeIngestedSegmentUtcStartTimeInMillisecs / 1000;
									int toBeIngestedSegmentMilliSecs = toBeIngestedSegmentUtcStartTimeInMillisecs % 1000;

									// from utc to local time
									localtime_r (&toBeIngestedSegmentUtcStartTimeInSeconds, &tmDateTime);

									/*
									sprintf (strCurrentRecordedFileTime,
										"%04d-%02d-%02d %02d:%02d:%02d",
										tmDateTime. tm_year + 1900,
										tmDateTime. tm_mon + 1,
										tmDateTime. tm_mday,
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec);
									*/

									sprintf (strCurrentRecordedFileTime,
										"%02d:%02d:%02d.%03d",
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec,
										toBeIngestedSegmentMilliSecs
										);

									addContentTitle += strCurrentRecordedFileTime;	// local time
								}

								addContentTitle += " - ";

								{
									tm		tmDateTime;
									char	strCurrentRecordedFileTime [64];

									time_t toBeIngestedSegmentUtcEndTimeInSeconds =
										toBeIngestedSegmentUtcEndTimeInMillisecs / 1000;
									int toBeIngestedSegmentMilliSecs = toBeIngestedSegmentUtcEndTimeInMillisecs % 1000;

									// from utc to local time
									localtime_r (&toBeIngestedSegmentUtcEndTimeInSeconds, &tmDateTime);

									/*
									sprintf (strCurrentRecordedFileTime,
										"%04d-%02d-%02d %02d:%02d:%02d",
										tmDateTime. tm_year + 1900,
										tmDateTime. tm_mon + 1,
										tmDateTime. tm_mday,
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec);
									*/
									sprintf (strCurrentRecordedFileTime,
										"%02d:%02d:%02d.%03d",
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec,
										toBeIngestedSegmentMilliSecs);

									addContentTitle += strCurrentRecordedFileTime;	// local time
								}

								// if (!main)
								// 	addContentTitle += " (BCK)";
							}

							{
								try
								{
									_logger->info(__FILEREF__ + "ingest Recorded media"
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
										+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
										+ ", addContentTitle: " + addContentTitle
									);

									if (externalEncoder)
										ingestRecordedMediaInCaseOfExternalTranscoder(ingestionJobKey,
											chunksTranscoderStagingContentsPath, toBeIngestedSegmentFileName,
											addContentTitle, uniqueName, userDataRoot, outputFileFormat,
											ingestedParametersRoot, encodingParametersRoot);
									else
										ingestRecordedMediaInCaseOfInternalTranscoder(ingestionJobKey,
											chunksTranscoderStagingContentsPath, toBeIngestedSegmentFileName,
											chunksNFSStagingContentsPath,
											addContentTitle, uniqueName, userDataRoot, outputFileFormat,
											ingestedParametersRoot, encodingParametersRoot,
											true);
								}
								catch(runtime_error e)
								{
									_logger->error(__FILEREF__ + "ingestRecordedMedia failed"
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", externalEncoder: " + to_string(externalEncoder)
										+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
										+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
										+ ", addContentTitle: " + addContentTitle
										+ ", outputFileFormat: " + outputFileFormat
										+ ", e.what(): " + e.what()
									);

									// throw e;
								}
								catch(exception e)
								{
									_logger->error(__FILEREF__ + "ingestRecordedMedia failed"
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", externalEncoder: " + to_string(externalEncoder)
										+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
										+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
										+ ", addContentTitle: " + addContentTitle
										+ ", outputFileFormat: " + outputFileFormat
									);
                
									// throw e;
								}
							}
						}

						newLastRecordedAssetFileName = toBeIngestedSegmentFileName;
						newLastRecordedAssetDurationInSeconds = toBeIngestedSegmentDuration;
						newLastRecordedSegmentUtcStartTimeInMillisecs = toBeIngestedSegmentUtcStartTimeInMillisecs;
					}

					ingestionNumber++;

					toBeIngestedSegmentDuration = -1.0;
					toBeIngestedSegmentUtcStartTimeInMillisecs = -1;
					toBeIngestedSegmentFileName = "";
				}
				else if (lastRecordedAssetFileName == currentSegmentFileName)
				{
					toBeIngested = true;
				}
			}

			// Scenario:
			//	we have lastRecordedAssetFileName with a filename that does not exist into the playlist
			// This is a scenario that should never happen but, in case it happens, we have to manage otherwise
			// no chunks will be ingested
			// lastRecordedAssetFileName has got from playlist in the previous processHLSSegmenterOutput call
			if (lastRecordedAssetFileName != ""
				&& !toBeIngested					// file name does not exist into the playlist
			)
			{
				// 2022-08-12: this scenario happens when the 'monitor process' kills the recording process,
				//	so the playlist is reset and start from scratch.

				_logger->warn(__FILEREF__ + "Filename not found: probable the playlist was reset (may be because of a kill of the monitor process)"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", toBeIngested: " + to_string(toBeIngested)
					+ ", toBeIngestedSegmentDuration: " + to_string(toBeIngestedSegmentDuration)
					+ ", toBeIngestedSegmentUtcStartTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcStartTimeInMillisecs)
					+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
					+ ", currentSegmentDuration: " + to_string(currentSegmentDuration)
					+ ", currentSegmentUtcStartTimeInMillisecs: " + to_string(currentSegmentUtcStartTimeInMillisecs)
					+ ", currentSegmentFileName: " + currentSegmentFileName
					+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
					+ ", newLastRecordedAssetFileName: " + newLastRecordedAssetFileName
					+ ", newLastRecordedSegmentUtcStartTimeInMillisecs: " + to_string(newLastRecordedSegmentUtcStartTimeInMillisecs)
				);

				newLastRecordedAssetFileName = "";
				newLastRecordedAssetDurationInSeconds = 0.0;
				newLastRecordedSegmentUtcStartTimeInMillisecs = -1;
			}
		}
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processHLSSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
			+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processHLSSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
			+ ", chunksNFSStagingContentsPath: " + chunksNFSStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
        );
                
        throw e;
    }

	return make_tuple(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds,
		newLastRecordedSegmentUtcStartTimeInMillisecs);
}

void LiveRecorderDaemons::ingestRecordedMediaInCaseOfInternalTranscoder(
	int64_t ingestionJobKey,
	string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName,
	string chunksNFSStagingContentsPath,
	string addContentTitle,
	string uniqueName,
	// bool highAvailability,
	Json::Value userDataRoot,
	string fileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot,
	bool copy)
{
	try
	{
		// moving chunk from transcoder staging path to shared staging path.
		// This is done because the AddContent task has a move://... url
		if (copy)
		{
			_logger->info(__FILEREF__ + "Chunk copying"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
				+ ", source: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + chunksNFSStagingContentsPath
			);

			chrono::system_clock::time_point startCopying = chrono::system_clock::now();
			fs::copy(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName, chunksNFSStagingContentsPath);
			chrono::system_clock::time_point endCopying = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Chunk copied"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + chunksNFSStagingContentsPath
				+ ", @MMS COPY statistics@ - copyingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endCopying - startCopying).count()) + "@"
			);
		}
		else
		{
			_logger->info(__FILEREF__ + "Chunk moving"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
				+ ", source: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + chunksNFSStagingContentsPath
			);

			chrono::system_clock::time_point startMoving = chrono::system_clock::now();
			MMSStorage::move(ingestionJobKey, chunksTranscoderStagingContentsPath + currentRecordedAssetFileName,
				chunksNFSStagingContentsPath, _logger);
			chrono::system_clock::time_point endMoving = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Chunk moved"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + chunksNFSStagingContentsPath
				+ ", @MMS MOVE statistics@ - movingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endMoving - startMoving).count()) + "@"
			);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = e.what();
		_logger->error(__FILEREF__ + "Coping/Moving of the chunk failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", exception: " + errorMessage
		);
		if (errorMessage.find(string("errno: 28")) != string::npos)
			_logger->error(__FILEREF__ + "No space left on storage"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", exception: " + errorMessage
			);


		_logger->info(__FILEREF__ + "remove"
			+ ", generated chunk: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName 
		);
		fs::remove_all(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", exception: " + e.what()
		);

		_logger->info(__FILEREF__ + "remove"
			+ ", generated chunk: " + chunksTranscoderStagingContentsPath + currentRecordedAssetFileName 
		);
		fs::remove_all(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);

		throw e;
	}

	string mmsWorkflowIngestionURL;
	string workflowMetadata;
	try
	{
		workflowMetadata = buildChunkIngestionWorkflow(
			ingestionJobKey,
			false,	// externalEncoder,
			currentRecordedAssetFileName,
			chunksNFSStagingContentsPath,
			addContentTitle,
			uniqueName,
			userDataRoot,
			fileFormat,
			ingestedParametersRoot,
			encodingParametersRoot
		);

		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value credentialsRoot = internalMMSRoot[field];

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		vector<string> otherHeaders;
		string sResponse = MMSCURL::httpPostString(
			_logger,
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			otherHeaders,
			3 // maxRetryNumber
		).second;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

void LiveRecorderDaemons::ingestRecordedMediaInCaseOfExternalTranscoder(
	int64_t ingestionJobKey,
	string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName,
	string addContentTitle,
	string uniqueName,
	Json::Value userDataRoot,
	string fileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot)
{
	string workflowMetadata;
	int64_t userKey;
	string apiKey;
	int64_t addContentIngestionJobKey = -1;
	string mmsWorkflowIngestionURL;
	// create the workflow and ingest it
	try
	{
		workflowMetadata = buildChunkIngestionWorkflow(
			ingestionJobKey,
			true,	// externalEncoder,
			"",	// currentRecordedAssetFileName,
			"",	// chunksNFSStagingContentsPath,
			addContentTitle,
			uniqueName,
			userDataRoot,
			fileFormat,
			ingestedParametersRoot,
			encodingParametersRoot
		);

		{
			string field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value credentialsRoot = internalMMSRoot[field];

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		vector<string> otherHeaders;
		string sResponse = MMSCURL::httpPostString(
			_logger,
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			otherHeaders,
			3 // maxRetryNumber
		).second;

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage =
			string("Ingested URL failed, addContentIngestionJobKey is not valid")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
		int64_t chunkFileSize = fs::file_size(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);

		string mmsBinaryIngestionURL;
		{
			string field = "mmsBinaryIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsBinaryIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		mmsBinaryURL =
			mmsBinaryIngestionURL
			+ "/" + to_string(addContentIngestionJobKey)
		;

		string sResponse = MMSCURL::httpPostFile(
			_logger,
			ingestionJobKey,
			mmsBinaryURL,
			_mmsBinaryTimeoutInSeconds,
			to_string(userKey),
			apiKey,
			chunksTranscoderStagingContentsPath + currentRecordedAssetFileName,
			chunkFileSize,
			3 // maxRetryNumber
		);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

string LiveRecorderDaemons::buildChunkIngestionWorkflow(
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
)
{
	string workflowMetadata;
	try
	{
		/*
		{
        	"label": "<workflow label>",
        	"type": "Workflow",
        	"Task": {
                "label": "<task label 1>",
                "type": "Add-Content"
                "parameters": {
                        "FileFormat": "ts",
                        "Ingester": "Giuliano",
                        "SourceURL": "move:///abc...."
                },
        	}
		}
		*/
		Json::Value mmsDataRoot = userDataRoot["mmsData"];
		int64_t utcPreviousChunkStartTime = JSONUtils::asInt64(mmsDataRoot, "utcPreviousChunkStartTime", -1);
		int64_t utcChunkStartTime = JSONUtils::asInt64(mmsDataRoot, "utcChunkStartTime", -1);
		int64_t utcChunkEndTime = JSONUtils::asInt64(mmsDataRoot, "utcChunkEndTime", -1);

		Json::Value addContentRoot;

		string field = "label";
		addContentRoot[field] = to_string(utcChunkStartTime);

		field = "type";
		addContentRoot[field] = "Add-Content";

		{
			field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value eventsRoot = internalMMSRoot[field];

					field = "OnSuccess";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						addContentRoot[field] = eventsRoot[field];

					field = "OnError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						addContentRoot[field] = eventsRoot[field];

					field = "OnComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						addContentRoot[field] = eventsRoot[field];
				}
			}
		}

		Json::Value addContentParametersRoot = ingestedParametersRoot;
		// if (internalMMSRootPresent)
		{
			Json::Value removed;
			field = "internalMMS";
			addContentParametersRoot.removeMember(field, &removed);
		}

		field = "FileFormat";
		addContentParametersRoot[field] = fileFormat;

		if (!externalEncoder)
		{
			string sourceURL = string("move") + "://" + chunksNFSStagingContentsPath + currentRecordedAssetFileName;
			field = "SourceURL";
			addContentParametersRoot[field] = sourceURL;
		}

		field = "Ingester";
		addContentParametersRoot[field] = "Live Recorder Task";

		field = "Title";
		addContentParametersRoot[field] = addContentTitle;

		field = "UserData";
		addContentParametersRoot[field] = userDataRoot;

		// if (!highAvailability)
		{
			// in case of no high availability, we can set just now the UniqueName for this content
			// in case of high availability, the unique name will be set only of the selected content
			//		choosing between main and bqckup
			field = "UniqueName";
			addContentParametersRoot[field] = uniqueName;
		}

		field = "parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "label";
		workflowRoot[field] = addContentTitle;

		field = "type";
		workflowRoot[field] = "Workflow";

		{
			Json::Value variablesWorkflowRoot;

			{
				Json::Value variableWorkflowRoot;

				field = "type";
				variableWorkflowRoot[field] = "integer";

				field = "Value";
				variableWorkflowRoot[field] = utcChunkStartTime;

				// name of the variable
				field = "CurrentUtcChunkStartTime";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			char	currentUtcChunkStartTime_HHMISS [64];
			{
				tm		tmDateTime;

				// from utc to local time
				localtime_r (&utcChunkStartTime, &tmDateTime);

				sprintf (currentUtcChunkStartTime_HHMISS,
					"%02d:%02d:%02d",
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);

			}
			// field = "CurrentUtcChunkStartTime_HHMISS";
			// variablesWorkflowRoot[field] = string(currentUtcChunkStartTime_HHMISS);
			{
				Json::Value variableWorkflowRoot;

				field = "type";
				variableWorkflowRoot[field] = "string";

				field = "Value";
				variableWorkflowRoot[field] = string(currentUtcChunkStartTime_HHMISS);

				// name of the variable
				field = "CurrentUtcChunkStartTime_HHMISS";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			// field = "PreviousUtcChunkStartTime";
			// variablesWorkflowRoot[field] = utcPreviousChunkStartTime;
			{
				Json::Value variableWorkflowRoot;

				field = "type";
				variableWorkflowRoot[field] = "integer";

				field = "Value";
				variableWorkflowRoot[field] = utcPreviousChunkStartTime;

				// name of the variable
				field = "PreviousUtcChunkStartTime";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);
			{
				Json::Value variableWorkflowRoot;

				field = "type";
				variableWorkflowRoot[field] = "integer";

				field = "Value";
				variableWorkflowRoot[field] = recordingCode;

				// name of the variable
				field = "recordingCode";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			string ingestionJobLabel = JSONUtils::asString(encodingParametersRoot, "ingestionJobLabel", "");
			{
				Json::Value variableWorkflowRoot;

				field = "type";
				variableWorkflowRoot[field] = "string";

				field = "Value";
				variableWorkflowRoot[field] = ingestionJobLabel;

				// name of the variable
				field = "IngestionJobLabel";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			field = "Variables";
			workflowRoot[field] = variablesWorkflowRoot;
		}

		field = "Task";
		workflowRoot[field] = addContentRoot;

   		{
       		workflowMetadata = JSONUtils::toString(workflowRoot);
   		}

		_logger->info(__FILEREF__ + "Recording Workflow metadata generated"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", " + addContentTitle + ", "
				+ currentRecordedAssetFileName
				+ ", prev: " + to_string(utcPreviousChunkStartTime)
				+ ", from: " + to_string(utcChunkStartTime)
				+ ", to: " + to_string(utcChunkEndTime)
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "buildRecordedMediaWorkflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "buildRecordedMediaWorkflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

bool LiveRecorderDaemons::isLastLiveRecorderFile(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	time_t utcCurrentRecordedFileCreationTime, string chunksTranscoderStagingContentsPath,
	string recordedFileNamePrefix, int segmentDurationInSeconds, bool isFirstChunk)
{
	bool isLastLiveRecorderFile = true;

    try
    {
		_logger->info(__FILEREF__ + "isLastLiveRecorderFile"
			+ ", chunksTranscoderStagingContentsPath: " + chunksTranscoderStagingContentsPath
			+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
			+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
		);

		for (fs::directory_entry const& entry: fs::directory_iterator(chunksTranscoderStagingContentsPath))
        {
            try
            {
				_logger->info(__FILEREF__ + "readDirectory"
					+ ", directoryEntry: " + entry.path().string()
				);

				// next statement is endWith and .lck is used during the move of a file
				string suffix(".lck");
				if (entry.path().filename().string().size() >= suffix.size()
					&& 0 == entry.path().filename().string().compare(entry.path().filename().string().size()-suffix.size(),
						suffix.size(), suffix))
					continue;

                if (!entry.is_regular_file())
                    continue;

                if (entry.path().filename().string().size() >= recordedFileNamePrefix.size()
						&& entry.path().filename().string().compare(0, recordedFileNamePrefix.size(),
							recordedFileNamePrefix) == 0)
                {
					time_t utcFileCreationTime = getMediaLiveRecorderStartTime(
							ingestionJobKey, encodingJobKey, entry.path().filename().string(), segmentDurationInSeconds,
							isFirstChunk);

					if (utcFileCreationTime > utcCurrentRecordedFileCreationTime)
					{
						isLastLiveRecorderFile = false;

						break;
					}
				}
            }
            catch(runtime_error e)
            {
                string errorMessage = __FILEREF__ + "listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
            catch(exception e)
            {
                string errorMessage = __FILEREF__ + "listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "isLastLiveRecorderFile failed"
            + ", e.what(): " + e.what()
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "isLastLiveRecorderFile failed");
    }

	return isLastLiveRecorderFile;
}

time_t LiveRecorderDaemons::getMediaLiveRecorderStartTime(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	string mediaLiveRecorderFileName, int segmentDurationInSeconds,
	bool isFirstChunk)
{
	// liveRecorder_6405_48749_2019-02-02_22-11-00_1100374273.ts
	// liveRecorder_<ingestionJobKey>_<FFMPEGEncoderBase::encodingJobKey>_YYYY-MM-DD_HH-MI-SS_<utc>.ts

	_logger->info(__FILEREF__ + "Received getMediaLiveRecorderStartTime"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
		+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
		+ ", isFirstChunk: " + to_string(isFirstChunk)
	);

	size_t endIndex = mediaLiveRecorderFileName.find_last_of(".");
	if (mediaLiveRecorderFileName.length() < 20 ||
		   endIndex == string::npos)
	{
		string errorMessage = __FILEREF__ + "wrong media live recorder format"
			+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
			;
			_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	size_t beginUTCIndex = mediaLiveRecorderFileName.find_last_of("_");
	if (mediaLiveRecorderFileName.length() < 20 ||
		   beginUTCIndex == string::npos)
	{
		string errorMessage = __FILEREF__ + "wrong media live recorder format"
			+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
			;
			_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	time_t utcMediaLiveRecorderStartTime = stol(mediaLiveRecorderFileName.substr(beginUTCIndex + 1,
				endIndex - beginUTCIndex + 1));

	{
		// in case of high bit rate (huge files) and server with high cpu usage, sometime I saw seconds 1 instead of 0
		// For this reason, utcMediaLiveRecorderStartTime is fixed.
		// From the other side the first generated file is the only one where we can have seconds
		// different from 0, anyway here this is not possible because we discard the first chunk
		// 2019-10-16: I saw as well seconds == 59, in this case we would not do utcMediaLiveRecorderStartTime -= seconds
		//	as it is done below in the code but we should do utcMediaLiveRecorderStartTime += 1.
		int seconds = stoi(mediaLiveRecorderFileName.substr(beginUTCIndex - 2, 2));
		if (!isFirstChunk && seconds % segmentDurationInSeconds != 0)
		{
			int halfSegmentDurationInSeconds = segmentDurationInSeconds / 2;

			// scenario: segmentDurationInSeconds is 10 and seconds = 29
			//	Before to compare seconds with halfSegmentDurationInSeconds
			//	(the check compare the seconds between 0 and 10)
			//	we have to redure 29 to 9
			if (seconds > segmentDurationInSeconds)
			{
				int factorToBeReduced = seconds / segmentDurationInSeconds;
				seconds -= (factorToBeReduced * segmentDurationInSeconds);
			}

			if (seconds <= halfSegmentDurationInSeconds)
			{
				_logger->warn(__FILEREF__ + "Wrong seconds (start time), force it to 0"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
					+ ", seconds: " + to_string(seconds)
				);
				utcMediaLiveRecorderStartTime -= seconds;
			}
			else if (seconds > halfSegmentDurationInSeconds && seconds < segmentDurationInSeconds)
			{
				_logger->warn(__FILEREF__ + "Wrong seconds (start time), increase it"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
					+ ", seconds: " + to_string(seconds)
				);
				utcMediaLiveRecorderStartTime += (segmentDurationInSeconds - seconds);
			}
		}
	}

	return utcMediaLiveRecorderStartTime;
	/*
	tm                      tmDateTime;


	// liveRecorder_6405_2019-02-02_22-11-00.ts

	_logger->info(__FILEREF__ + "getMediaLiveRecorderStartTime"
		", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
	);

	size_t index = mediaLiveRecorderFileName.find_last_of(".");
	if (mediaLiveRecorderFileName.length() < 20 ||
		   index == string::npos)
	{
		string errorMessage = __FILEREF__ + "wrong media live recorder format"
			+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
			;
			_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	time_t utcMediaLiveRecorderStartTime;
	time (&utcMediaLiveRecorderStartTime);
	gmtime_r(&utcMediaLiveRecorderStartTime, &tmDateTime);

	tmDateTime.tm_year		= stoi(mediaLiveRecorderFileName.substr(index - 19, 4))
		- 1900;
	tmDateTime.tm_mon		= stoi(mediaLiveRecorderFileName.substr(index - 14, 2))
		- 1;
	tmDateTime.tm_mday		= stoi(mediaLiveRecorderFileName.substr(index - 11, 2));
	tmDateTime.tm_hour		= stoi(mediaLiveRecorderFileName.substr(index - 8, 2));
	tmDateTime.tm_min      = stoi(mediaLiveRecorderFileName.substr(index - 5, 2));

	// in case of high bit rate (huge files) and server with high cpu usage, sometime I saw seconds 1 instead of 0
	// For this reason, 0 is set.
	// From the other side the first generated file is the only one where we can have seconds
	// different from 0, anyway here this is not possible because we discard the first chunk
	int seconds = stoi(mediaLiveRecorderFileName.substr(index - 2, 2));
	if (seconds != 0)
	{
		_logger->warn(__FILEREF__ + "Wrong seconds (start time), force it to 0"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
				+ ", seconds: " + to_string(seconds)
				);
		seconds = 0;
	}
	tmDateTime.tm_sec      = seconds;

	utcMediaLiveRecorderStartTime = timegm (&tmDateTime);

	return utcMediaLiveRecorderStartTime;
	*/
}

time_t LiveRecorderDaemons::getMediaLiveRecorderEndTime(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	string mediaLiveRecorderFileName)
{
	tm                      tmDateTime;

	time_t utcCurrentRecordedFileLastModificationTime;
	{
		chrono::system_clock::time_point fileLastModification =
			chrono::time_point_cast<chrono::system_clock::duration>(
				fs::last_write_time(mediaLiveRecorderFileName) - fs::file_time_type::clock::now() + chrono::system_clock::now());
		utcCurrentRecordedFileLastModificationTime = chrono::system_clock::to_time_t(fileLastModification);
	}

	// FileIO::getFileTime (mediaLiveRecorderFileName.c_str(),
	// 	&utcCurrentRecordedFileLastModificationTime);

	localtime_r(&utcCurrentRecordedFileLastModificationTime, &tmDateTime);

	// in case of high bit rate (huge files) and server with high cpu usage, sometime I saw seconds 1 instead of 0
	// For this reason, 0 is set
	if (tmDateTime.tm_sec != 0)
	{
		_logger->warn(__FILEREF__ + "Wrong seconds (end time), force it to 0"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
				+ ", seconds: " + to_string(tmDateTime.tm_sec)
				);
		tmDateTime.tm_sec = 0;
	}

	utcCurrentRecordedFileLastModificationTime = mktime(&tmDateTime);

	return utcCurrentRecordedFileLastModificationTime;
}

long LiveRecorderDaemons::buildAndIngestVirtualVOD(
	int64_t liveRecorderIngestionJobKey,
	int64_t liveRecorderEncodingJobKey,
	bool externalEncoder,

	string sourceSegmentsDirectoryPathName,
	string sourceManifestFileName,
	// /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/.../content
	string stagingLiveRecorderVirtualVODPathName,

	int64_t recordingCode,
	string liveRecorderIngestionJobLabel,
	string liveRecorderVirtualVODUniqueName,
	string liveRecorderVirtualVODRetention,
	int64_t liveRecorderVirtualVODImageMediaItemKey,
	int64_t liveRecorderUserKey,
	string liveRecorderApiKey,
	string mmsWorkflowIngestionURL,
	string mmsBinaryIngestionURL
)
{

    _logger->info(__FILEREF__ + "Received buildAndIngestVirtualVOD"
		+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
		+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
		+ ", externalEncoder: " + to_string(externalEncoder)

        + ", sourceSegmentsDirectoryPathName: " + sourceSegmentsDirectoryPathName
        + ", sourceManifestFileName: " + sourceManifestFileName
        + ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName

		+ ", recordingCode: " + to_string(recordingCode)
        + ", liveRecorderIngestionJobLabel: " + liveRecorderIngestionJobLabel
        + ", liveRecorderVirtualVODUniqueName: " + liveRecorderVirtualVODUniqueName
        + ", liveRecorderVirtualVODRetention: " + liveRecorderVirtualVODRetention
		+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
		+ ", liveRecorderUserKey: " + to_string(liveRecorderUserKey)
        + ", liveRecorderApiKey: " + liveRecorderApiKey
        + ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
        + ", mmsBinaryIngestionURL: " + mmsBinaryIngestionURL
    );

	long segmentsNumber = 0;

	// let's build the live recorder virtual VOD
	// - copy current manifest and TS files
	// - calculate start end time of the virtual VOD
	// - add end line to manifest
	// - create tar gz
	// - remove directory
	int64_t utcStartTimeInMilliSecs = -1;
	int64_t utcEndTimeInMilliSecs = -1;

	string virtualVODM3u8DirectoryName;
	string tarGzStagingLiveRecorderVirtualVODPathName;
	try
	{
		{
			// virtualVODM3u8DirectoryName = to_string(liveRecorderIngestionJobKey)
			// 	+ "_liveRecorderVirtualVOD"
			// ;
			{
				size_t endOfPathIndex = stagingLiveRecorderVirtualVODPathName.find_last_of("/");
				if (endOfPathIndex == string::npos)
				{
					string errorMessage = string("No stagingLiveRecorderVirtualVODPathName found")
						+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
						+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
						+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName 
					;
					_logger->error(__FILEREF__ + errorMessage);
          
					throw runtime_error(errorMessage);
				}
				// stagingLiveRecorderVirtualVODPathName is initialized in EncoderVideoAudioProxy.cpp
				// and virtualVODM3u8DirectoryName will be the name of the directory of the m3u8 of the Virtual VOD
				// In case of externalEncoder, since PUSH is used, virtualVODM3u8DirectoryName has to be 'content'
				// (see the Add-Content Task documentation). For this reason, in EncoderVideoAudioProxy.cpp,
				// 'content' is used
				virtualVODM3u8DirectoryName =
					stagingLiveRecorderVirtualVODPathName.substr(endOfPathIndex + 1);
			}


			if (stagingLiveRecorderVirtualVODPathName != ""
				&& fs::exists(stagingLiveRecorderVirtualVODPathName))
			{
				_logger->info(__FILEREF__ + "Remove directory"
					+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(stagingLiveRecorderVirtualVODPathName);
			}
		}

		string sourceManifestPathFileName = sourceSegmentsDirectoryPathName + "/" +
			sourceManifestFileName;
		if (!fs::exists(sourceManifestPathFileName.c_str()))
		{
			string errorMessage = string("manifest file not existing")
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceManifestPathFileName: " + sourceManifestPathFileName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2021-05-30: it is not a good idea to copy all the directory (manifest and ts files) because
		//	ffmpeg is not accurate to remove the obsolete ts files, so we will have the manifest files
		//	having for example 300 ts references but the directory contains thousands of ts files.
		//	So we will copy only the manifest file and ONLY the ts files referenced into the manifest file

		/*
		// copy manifest and TS files into the stagingLiveRecorderVirtualVODPathName
		{
			_logger->info(__FILEREF__ + "Coping directory"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceSegmentsDirectoryPathName: " + sourceSegmentsDirectoryPathName
				+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
			);

			chrono::system_clock::time_point startCoping = chrono::system_clock::now();
			fs::copyDirectory(sourceSegmentsDirectoryPathName, stagingLiveRecorderVirtualVODPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |                                                                 
                  S_IRGRP | S_IXGRP |                                                                           
                  S_IROTH | S_IXOTH);
			chrono::system_clock::time_point endCoping = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Copied directory"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", @MMS COPY statistics@ - copingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endCoping - startCoping).count()) + "@"
			);
		}
		*/

		// 2022-05-26: non dovrebbe accadere ma, a volte, capita che il file ts non esiste, perchè eliminato
		//	da ffmpeg, ma risiede ancora nel manifest. Per evitare quindi che la generazione del virtualVOD
		//	si blocchi, consideriamo come se il manifest avesse solamente i segmenti successivi
		//	La copia quindi del manifest originale viene fatta su un file temporaneo e gestiamo
		//	noi il manifest "definitivo"
		// 2022-05-27: Probabilmente era il crontab che rimuoveva i segmenti e causava il problema
		//	descritto sopra. Per cui, fissato il retention del crontab, mantenere la playlist originale
		//	probabilmente va bene. Ormai lasciamo cosi visto che funziona ed è piu robusto nel caso in cui
		//	un segmento venisse eliminato

		string tmpManifestPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
			sourceManifestFileName + ".tmp";
		string destManifestPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
			sourceManifestFileName;

		// create the destination directory and copy the manifest file
		{
			if (!fs::exists(stagingLiveRecorderVirtualVODPathName))
			{
				_logger->info(__FILEREF__ + "Creating directory"
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
				);
				fs::create_directories(stagingLiveRecorderVirtualVODPathName);
				fs::permissions(stagingLiveRecorderVirtualVODPathName,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
					| fs::perms::group_read | fs::perms::group_exec
					| fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace);
			}

			_logger->info(__FILEREF__ + "Coping"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceManifestPathFileName: " + sourceManifestPathFileName
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			);
			fs::copy(sourceManifestPathFileName, tmpManifestPathFileName);
		}

		if (!fs::exists (tmpManifestPathFileName.c_str()))
		{
			string errorMessage = string("manifest file not existing")
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		ofstream ofManifestFile(destManifestPathFileName, ofstream::trunc);

		// read start time of the first segment
		// read start time and duration of the last segment
		// copy ts file into the directory
		double firstSegmentDuration = -1.0;
		int64_t firstSegmentUtcStartTimeInMillisecs = -1;
		double lastSegmentDuration = -1.0;
		int64_t lastSegmentUtcStartTimeInMillisecs = -1;
		{
			_logger->info(__FILEREF__ + "Reading copied manifest file"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			);

			ifstream ifManifestFile(tmpManifestPathFileName);
			if (!ifManifestFile.is_open())
			{
				string errorMessage = string("Not authorized: manifest file not opened")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			string firstPartOfManifest;
			string manifestLine;
			{
				while(getline(ifManifestFile, manifestLine))
				{
					// #EXTM3U
					// #EXT-X-VERSION:3
					// #EXT-X-TARGETDURATION:19
					// #EXT-X-MEDIA-SEQUENCE:0
					// #EXTINF:10.000000,
					// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
					// liveRecorder_760504_1653579715.ts
					// ...

					string extInfPrefix ("#EXTINF:");
					string programDatePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
					if (manifestLine.size() >= extInfPrefix.size()
						&& 0 == manifestLine.compare(0, extInfPrefix.size(), extInfPrefix))
					{
						break;
					}
					else if (manifestLine.size() >= programDatePrefix.size()
						&& 0 == manifestLine.compare(0, programDatePrefix.size(), programDatePrefix))
						break;
					else if (manifestLine[0] != '#')
					{
						break;
					}
					else
					{
						firstPartOfManifest += (manifestLine + "\n");
					}
				}
			}

			ofManifestFile << firstPartOfManifest;

			segmentsNumber = 0;
			do
			{
				// #EXTM3U
				// #EXT-X-VERSION:3
				// #EXT-X-TARGETDURATION:19
				// #EXT-X-MEDIA-SEQUENCE:0
				// #EXTINF:10.000000,
				// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
				// liveRecorder_760504_1653579715.ts
				// ...

				_logger->info(__FILEREF__ + "manifestLine"
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", manifestLine: " + manifestLine
					+ ", segmentsNumber: " + to_string(segmentsNumber)
				);

				string extInfPrefix ("#EXTINF:");
				string programDatePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
				if (manifestLine.size() >= extInfPrefix.size()
					&& 0 == manifestLine.compare(0, extInfPrefix.size(), extInfPrefix))
				{
					size_t endOfSegmentDuration = manifestLine.find(",");
					if (endOfSegmentDuration == string::npos)
					{
						string errorMessage = string("wrong manifest line format")
							+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
							+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
							+ ", manifestLine: " + manifestLine
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}

					lastSegmentDuration = stod(manifestLine.substr(extInfPrefix.size(),
						endOfSegmentDuration - extInfPrefix.size()));
				}
				else if (manifestLine.size() >= programDatePrefix.size()
					&& 0 == manifestLine.compare(0, programDatePrefix.size(), programDatePrefix))
					lastSegmentUtcStartTimeInMillisecs = DateTime::sDateMilliSecondsToUtc(manifestLine.substr(programDatePrefix.size()));
				else if (manifestLine != "" && manifestLine[0] != '#')
				{
					string sourceTSPathFileName = sourceSegmentsDirectoryPathName + "/" +
						manifestLine;
					string copiedTSPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
						manifestLine;

					try
					{
						_logger->info(__FILEREF__ + "Coping"
							+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
							+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
							+ ", sourceTSPathFileName: " + sourceTSPathFileName
							+ ", copiedTSPathFileName: " + copiedTSPathFileName
						);
						fs::copy(sourceTSPathFileName, copiedTSPathFileName);
					}
					catch(runtime_error e)
					{
						string errorMessage =
							string("copyFile failed, previous segments of the manifest will be omitted")
							+ ", sourceTSPathFileName: " + sourceTSPathFileName
							+ ", copiedTSPathFileName: " + copiedTSPathFileName
							+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
							+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
							+ ", segmentsNumber: " + to_string(segmentsNumber)
							+ ", e.what: " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);

						ofManifestFile.close();

						ofManifestFile.open(destManifestPathFileName, ofstream::trunc);
						ofManifestFile << firstPartOfManifest;

						firstSegmentDuration = -1.0;
						firstSegmentUtcStartTimeInMillisecs = -1;
						lastSegmentDuration = -1.0;
						lastSegmentUtcStartTimeInMillisecs = -1;

						segmentsNumber = 0;

						continue;
					}

					segmentsNumber++;
				}

				if (firstSegmentDuration == -1.0 && firstSegmentUtcStartTimeInMillisecs == -1
					&& lastSegmentDuration != -1.0 && lastSegmentUtcStartTimeInMillisecs != -1)
				{
					firstSegmentDuration = lastSegmentDuration;
					firstSegmentUtcStartTimeInMillisecs = lastSegmentUtcStartTimeInMillisecs;
				}

				ofManifestFile << manifestLine << endl;
			}
			while(getline(ifManifestFile, manifestLine));
		}
		utcStartTimeInMilliSecs = firstSegmentUtcStartTimeInMillisecs;
		utcEndTimeInMilliSecs = lastSegmentUtcStartTimeInMillisecs + (lastSegmentDuration * 1000);

		// add end list to manifest file
		{
			_logger->info(__FILEREF__ + "Add end manifest line to copied manifest file"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			);

			// string endLine = "\n";
			ofManifestFile << endl << "#EXT-X-ENDLIST" << endl;
			ofManifestFile.close();
		}

		if (segmentsNumber == 0)
		{
			string errorMessage = string("No segments found")
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceManifestPathFileName: " + sourceManifestPathFileName 
				+ ", destManifestPathFileName: " + destManifestPathFileName 
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			string executeCommand;
			try
			{
				tarGzStagingLiveRecorderVirtualVODPathName = stagingLiveRecorderVirtualVODPathName + ".tar.gz";

				size_t endOfPathIndex = stagingLiveRecorderVirtualVODPathName.find_last_of("/");
				if (endOfPathIndex == string::npos)
				{
					string errorMessage = string("No stagingLiveRecorderVirtualVODDirectory found")
						+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
						+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
						+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName 
					;
					_logger->error(__FILEREF__ + errorMessage);
          
					throw runtime_error(errorMessage);
				}
				string stagingLiveRecorderVirtualVODDirectory =
					stagingLiveRecorderVirtualVODPathName.substr(0, endOfPathIndex);

				executeCommand =
					"tar cfz " + tarGzStagingLiveRecorderVirtualVODPathName
					+ " -C " + stagingLiveRecorderVirtualVODDirectory
					+ " " + virtualVODM3u8DirectoryName;

				// sometimes tar return 1 as status and the command fails because, according the tar man pages,
				// "this exit code means that some files were changed while being archived and
				//	so the resulting archive does not contain the exact copy of the file set"
				// I guess this is due because of the copy of the ts files among different file systems
				// For this reason I added this sleep
				long secondsToSleep = 3;
				_logger->info(__FILEREF__ + "Start tar command "
					+ ", executeCommand: " + executeCommand
					+ ", secondsToSleep: " + to_string(secondsToSleep)
				);
				this_thread::sleep_for(chrono::seconds(secondsToSleep));

				chrono::system_clock::time_point startTar = chrono::system_clock::now();
				int executeCommandStatus = ProcessUtility::execute(executeCommand);
				chrono::system_clock::time_point endTar = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "End tar command "
					+ ", executeCommand: " + executeCommand
					+ ", @MMS statistics@ - tarDuration (millisecs): @"
						+ to_string(chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count()) + "@"
				);
				if (executeCommandStatus != 0)
				{
					string errorMessage = string("ProcessUtility::execute failed")
						+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
						+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
						+ ", executeCommandStatus: " + to_string(executeCommandStatus) 
						+ ", executeCommand: " + executeCommand 
					;
					_logger->error(__FILEREF__ + errorMessage);
          
					throw runtime_error(errorMessage);
				}

				{
					_logger->info(__FILEREF__ + "Remove directory"
						+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
					);
					fs::remove_all(stagingLiveRecorderVirtualVODPathName);
				}
			}
			catch(runtime_error e)
			{
				string errorMessage = string("tar command failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", executeCommand: " + executeCommand 
				;
				_logger->error(__FILEREF__ + errorMessage);
         
				throw runtime_error(errorMessage);
			}
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("build the live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		if (stagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(stagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(stagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch(exception e)
	{
		string errorMessage = string("build the live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		if (stagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(stagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(stagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}


	// build workflow
	string workflowMetadata;
	try
	{
		workflowMetadata = buildVirtualVODIngestionWorkflow(
			liveRecorderIngestionJobKey,
			liveRecorderEncodingJobKey,
			externalEncoder,

			utcStartTimeInMilliSecs,
			utcEndTimeInMilliSecs,
			recordingCode,
			liveRecorderIngestionJobLabel,
			tarGzStagingLiveRecorderVirtualVODPathName,
			liveRecorderVirtualVODUniqueName,
			liveRecorderVirtualVODRetention,
			liveRecorderVirtualVODImageMediaItemKey);
	}
	catch (runtime_error e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}

	// ingest the Live Recorder VOD
	int64_t addContentIngestionJobKey = -1;
	try
	{
		vector<string> otherHeaders;
		string sResponse = MMSCURL::httpPostString(
			_logger,
			liveRecorderIngestionJobKey,
			mmsWorkflowIngestionURL,
			_mmsAPITimeoutInSeconds,
			to_string(liveRecorderUserKey),
			liveRecorderApiKey,
			workflowMetadata,
			"application/json",	// contentType
			otherHeaders,
			3 // maxRetryNumber
		).second;

		if (externalEncoder)
		{
			addContentIngestionJobKey = getAddContentIngestionJobKey(
				liveRecorderIngestionJobKey, sResponse);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = string("ingest live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string("ingest live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}

	if (externalEncoder)
	{
		string mmsBinaryURL;
		// ingest binary
		try
		{
			if (addContentIngestionJobKey == -1)
			{
				string errorMessage =
					string("Ingested URL failed, addContentIngestionJobKey is not valid")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			int64_t chunkFileSize = fs::file_size(tarGzStagingLiveRecorderVirtualVODPathName);

			mmsBinaryURL =
				mmsBinaryIngestionURL
				+ "/" + to_string(addContentIngestionJobKey)
			;

			string sResponse = MMSCURL::httpPostFileSplittingInChunks(
				_logger,
				liveRecorderIngestionJobKey,
				mmsBinaryURL,
				_mmsBinaryTimeoutInSeconds,
				to_string(liveRecorderUserKey),
				liveRecorderApiKey,
				tarGzStagingLiveRecorderVirtualVODPathName,
				chunkFileSize,
				3 // maxRetryNumber
			);

			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
			}
		}
		catch (runtime_error e)
		{
			_logger->error(__FILEREF__ + "Ingestion binary failed"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				+ ", mmsBinaryURL: " + mmsBinaryURL
				+ ", workflowMetadata: " + workflowMetadata
				+ ", exception: " + e.what()
			);

			if (tarGzStagingLiveRecorderVirtualVODPathName != ""
				&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
			}

			throw e;
		}
		catch (exception e)
		{
			_logger->error(__FILEREF__ + "Ingestion binary failed"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				+ ", mmsBinaryURL: " + mmsBinaryURL
				+ ", workflowMetadata: " + workflowMetadata
				+ ", exception: " + e.what()
			);

			if (tarGzStagingLiveRecorderVirtualVODPathName != ""
				&& fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
			}

			throw e;
		}
	}

	return segmentsNumber;
}

string LiveRecorderDaemons::buildVirtualVODIngestionWorkflow(
	int64_t liveRecorderIngestionJobKey,
	int64_t liveRecorderEncodingJobKey,
	bool externalEncoder,

	int64_t utcStartTimeInMilliSecs,
	int64_t utcEndTimeInMilliSecs,
	int64_t recordingCode,
	string liveRecorderIngestionJobLabel,
	string tarGzStagingLiveRecorderVirtualVODPathName,
	string liveRecorderVirtualVODUniqueName,
	string liveRecorderVirtualVODRetention,
	int64_t liveRecorderVirtualVODImageMediaItemKey
)
{
	string workflowMetadata;

	try
	{
		// {
        // 	"label": "<workflow label>",
        // 	"type": "Workflow",
        //	"Task": {
        //        "label": "<task label 1>",
        //        "type": "Add-Content"
        //        "parameters": {
        //                "FileFormat": "m3u8",
        //                "Ingester": "Giuliano",
        //                "SourceURL": "move:///abc...."
        //        },
        //	}
		// }
		Json::Value mmsDataRoot;

		// 2020-04-28: set it to liveRecordingChunk to avoid to be visible into the GUI (view MediaItems).
		//	This is because this MediaItem is not completed yet
		string field = "dataType";
		mmsDataRoot[field] = "liveRecordingVOD";

		field = "utcStartTimeInMilliSecs";
		mmsDataRoot[field] = utcStartTimeInMilliSecs;

		field = "utcEndTimeInMilliSecs";
		mmsDataRoot[field] = utcEndTimeInMilliSecs;

		string sUtcEndTimeForContentTitle;
		{
			char    utcEndTime_str [64];
			tm      tmDateTime;


			time_t utcEndTimeInSeconds = utcEndTimeInMilliSecs / 1000;

			// from utc to local time
			localtime_r (&utcEndTimeInSeconds, &tmDateTime);

			{
				sprintf (utcEndTime_str,
					"%04d-%02d-%02d %02d:%02d:%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1,
					tmDateTime. tm_mday,
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);

				string sUtcEndTime = utcEndTime_str;

				field = "utcEndTime_str";
				mmsDataRoot[field] = sUtcEndTime;
			}

			{
				sprintf (utcEndTime_str,
					"%02d:%02d:%02d",
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);

				sUtcEndTimeForContentTitle = utcEndTime_str;
			}
		}

		field = "recordingCode";
		mmsDataRoot[field] = recordingCode;

		Json::Value userDataRoot;

		field = "mmsData";
		userDataRoot[field] = mmsDataRoot;

		Json::Value addContentRoot;

		string addContentLabel = liveRecorderIngestionJobLabel;
			// + " V-VOD (up to " + sUtcEndTimeForContentTitle + ")";

		field = "label";
		addContentRoot[field] = addContentLabel;

		field = "type";
		addContentRoot[field] = "Add-Content";

		Json::Value addContentParametersRoot;

		field = "FileFormat";
		addContentParametersRoot[field] = "m3u8-tar.gz";

		if (!externalEncoder)
		{
			// 2021-05-30: changed from copy to move with the idea to have better performance
			string sourceURL = string("move") + "://" + tarGzStagingLiveRecorderVirtualVODPathName;
			field = "SourceURL";
			addContentParametersRoot[field] = sourceURL;
		}

		field = "Ingester";
		addContentParametersRoot[field] = "Live Recorder Task";

		field = "Title";
		addContentParametersRoot[field] = "Virtual VOD: " + addContentLabel;

		field = "UniqueName";
		addContentParametersRoot[field] = liveRecorderVirtualVODUniqueName;

		field = "AllowUniqueNameOverride";
		addContentParametersRoot[field] = true;

		field = "Retention";
		addContentParametersRoot[field] = liveRecorderVirtualVODRetention;

		field = "UserData";
		addContentParametersRoot[field] = userDataRoot;

		if (liveRecorderVirtualVODImageMediaItemKey != -1)
		{
			try
			{
				/*
				bool warningIfMissing = true;
				pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemDetails =
					_mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					workspace->_workspaceKey, liveRecorderVirtualVODImageLabel, warningIfMissing);

				int64_t liveRecorderVirtualVODImageMediaItemKey;
				tie(liveRecorderVirtualVODImageMediaItemKey, ignore) = mediaItemDetails;
				*/

				Json::Value crossReferenceRoot;

				field = "type";
				crossReferenceRoot[field] = "VideoOfImage";

				field = "MediaItemKey";
				crossReferenceRoot[field] = liveRecorderVirtualVODImageMediaItemKey;

				field = "CrossReference";
				addContentParametersRoot[field] = crossReferenceRoot;
			}
			catch (MediaItemKeyNotFound e)
			{
				string errorMessage = string("getMediaItemKeyDetailsByUniqueName failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
			catch (runtime_error e)
			{
				string errorMessage = string("getMediaItemKeyDetailsByUniqueName failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string("getMediaItemKeyDetailsByUniqueName failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
		}

		field = "parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "label";
		workflowRoot[field] = addContentLabel + " (virtual VOD)";

		field = "type";
		workflowRoot[field] = "Workflow";

		field = "Task";
		workflowRoot[field] = addContentRoot;

   		{
       		workflowMetadata = JSONUtils::toString(workflowRoot);
   		}

		_logger->info(__FILEREF__ + "Live Recorder VOD Workflow metadata generated"
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", " + addContentLabel + ", "
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

