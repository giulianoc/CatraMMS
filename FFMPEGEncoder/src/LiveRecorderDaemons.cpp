
#include "LiveRecorderDaemons.h"

#include "CurlWrapper.h"
#include "Datetime.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "JsonPath.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "ProcessUtility.h"
#include "SafeFileSystem.h"
#include "StringUtils.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"
#include <sstream>

using namespace std;
using json = nlohmann::json;

LiveRecorderDaemons::LiveRecorderDaemons(
	json configurationRoot, mutex *liveRecordingMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability
)
	: FFMPEGEncoderBase(configurationRoot)
{
	try
	{
		_liveRecordingMutex = liveRecordingMutex;
		_liveRecordingsCapability = liveRecordingsCapability;

		_liveRecorderChunksIngestionThreadShutdown = false;
		_liveRecorderVirtualVODIngestionThreadShutdown = false;

		_liveRecorderChunksIngestionCheckInSeconds = JSONUtils::asInt32(configurationRoot["ffmpeg"], "liveRecorderChunksIngestionCheckInSeconds", 5);
		SPDLOG_INFO(
			"Configuration item"
			", ffmpeg->liveRecorderChunksIngestionCheckInSeconds: {}",
			_liveRecorderChunksIngestionCheckInSeconds
		);

		_liveRecorderVirtualVODRetention = JSONUtils::asString(configurationRoot["ffmpeg"], "liveRecorderVirtualVODRetention", "15m");
		SPDLOG_INFO(
			"Configuration item"
			", ffmpeg->liveRecorderVirtualVODRetention: {}",
			_liveRecorderVirtualVODRetention
		);
		_liveRecorderVirtualVODIngestionInSeconds = JSONUtils::asInt32(configurationRoot["ffmpeg"], "liveRecorderVirtualVODIngestionInSeconds", 5);
		SPDLOG_INFO(
			"Configuration item"
			", ffmpeg->liveRecorderVirtualVODIngestionInSeconds: {}",
			_liveRecorderVirtualVODIngestionInSeconds
		);
	}
	catch (runtime_error &e)
	{
		// error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch (exception &e)
	{
		// error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

LiveRecorderDaemons::~LiveRecorderDaemons()
{
	try
	{
	}
	catch (runtime_error &e)
	{
		// error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch (exception &e)
	{
		// error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

void LiveRecorderDaemons::startChunksIngestionThread()
{

	while (!_liveRecorderChunksIngestionThreadShutdown)
	{
		try
		{
			chrono::system_clock::time_point startAllChannelsIngestionChunks = chrono::system_clock::now();

			lock_guard<mutex> locker(*_liveRecordingMutex);

			for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording : *_liveRecordingsCapability)
			{
				if (liveRecording->_childProcessId.isInitialized()) // running
				{
					SPDLOG_INFO(
						"processSegmenterOutput ..."
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", liveRecording->_segmenterType: {}",
						liveRecording->_ingestionJobKey, liveRecording->_encodingJobKey, liveRecording->_segmenterType
					);

					chrono::system_clock::time_point startSingleChannelIngestionChunks = chrono::system_clock::now();

					try
					{
						if (liveRecording->_encodingParametersRoot != nullptr)
						{
							int segmentDurationInSeconds;
							string outputFileFormat;
							{
								string field = "segmentDuration";
								segmentDurationInSeconds = JSONUtils::asInt32(liveRecording->_ingestedParametersRoot, field, -1);

								field = "outputFileFormat";
								outputFileFormat = JSONUtils::asString(liveRecording->_ingestedParametersRoot, field, "ts");
							}

							tuple<string, int, int64_t> lastRecordedAssetInfo;

							if (liveRecording->_segmenterType == "streamSegmenter")
							{
								lastRecordedAssetInfo = processStreamSegmenterOutput(
									liveRecording->_ingestionJobKey, liveRecording->_encodingJobKey, liveRecording->_streamSourceType,
									liveRecording->_externalEncoder, segmentDurationInSeconds, outputFileFormat,
									liveRecording->_encodingParametersRoot, liveRecording->_ingestedParametersRoot,

									liveRecording->_chunksTranscoderStagingContentsPath, liveRecording->_chunksNFSStagingContentsPath,
									liveRecording->_segmentListFileName, liveRecording->_recordedFileNamePrefix,
									liveRecording->_lastRecordedAssetFileName, liveRecording->_lastRecordedAssetDurationInSeconds,
									liveRecording->_lastRecordedSegmentUtcStartTimeInMillisecs
								);
							}
							else // if (liveRecording->_segmenterType ==
								 // "hlsSegmenter")
							{
								lastRecordedAssetInfo = processHLSSegmenterOutput(
									liveRecording->_ingestionJobKey, liveRecording->_encodingJobKey, liveRecording->_streamSourceType,
									liveRecording->_externalEncoder, segmentDurationInSeconds, outputFileFormat,
									liveRecording->_encodingParametersRoot, liveRecording->_ingestedParametersRoot,

									liveRecording->_chunksTranscoderStagingContentsPath, liveRecording->_chunksNFSStagingContentsPath,
									liveRecording->_segmentListFileName, liveRecording->_recordedFileNamePrefix,
									liveRecording->_lastRecordedAssetFileName, liveRecording->_lastRecordedAssetDurationInSeconds,
									liveRecording->_lastRecordedSegmentUtcStartTimeInMillisecs
								);
							}

							tie(liveRecording->_lastRecordedAssetFileName, liveRecording->_lastRecordedAssetDurationInSeconds,
								liveRecording->_lastRecordedSegmentUtcStartTimeInMillisecs) = lastRecordedAssetInfo;
							// liveRecording->_lastRecordedAssetFileName =
							// lastRecordedAssetInfo.first;
							// liveRecording->_lastRecordedAssetDurationInSeconds
							// = lastRecordedAssetInfo.second;
						}
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"processSegmenterOutput failed"
							", liveRecording->_ingestionJobKey: {}"
							", liveRecording->_encodingJobKey: {}"
							", e.what(): {}",
							liveRecording->_ingestionJobKey, liveRecording->_encodingJobKey, e.what()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"processSegmenterOutput failed"
							", liveRecording->_ingestionJobKey: {}"
							", liveRecording->_encodingJobKey: {}"
							", e.what(): {}",
							liveRecording->_ingestionJobKey, liveRecording->_encodingJobKey, e.what()
						);
					}

					SPDLOG_INFO(
						"Single Channel Ingestion Chunks"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", @MMS statistics@ - elapsed time: @{}@",
						liveRecording->_ingestionJobKey, liveRecording->_encodingJobKey,
						chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startSingleChannelIngestionChunks).count()
					);
				}
			}

			SPDLOG_INFO(
				"All Channels Ingestion Chunks"
				", @MMS statistics@ - elapsed time: @{}@",
				chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startAllChannelsIngestionChunks).count()
			);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"liveRecorderChunksIngestion failed"
				", e.what(): {}",
				e.what()
			);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"liveRecorderChunksIngestion failed"
				", e.what(): {}",
				e.what()
			);
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

	while (!_liveRecorderVirtualVODIngestionThreadShutdown)
	{
		int virtualVODsNumber = 0;

		try
		{
			// this is to have a copy of LiveRecording
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> copiedRunningLiveRecordingCapability;

			// this is to have access to running and _proxyStart
			//	to check if it is changed. In case the process is killed, it
			// will
			// access 	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> sourceLiveRecordingCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveRecordingVirtualVODCounter = 0;
			{
				lock_guard<mutex> locker(*_liveRecordingMutex);

				int liveRecordingNotVirtualVODCounter = 0;

				for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording : *_liveRecordingsCapability)
				{
					if (liveRecording->_childProcessId.isInitialized() && liveRecording->_virtualVOD
						&& liveRecording->_encodingStart && startClone > liveRecording->_encodingStart)
					{
						liveRecordingVirtualVODCounter++;

						copiedRunningLiveRecordingCapability.push_back(liveRecording->cloneForMonitorAndVirtualVOD());
						sourceLiveRecordingCapability.push_back(liveRecording);
					}
					else
					{
						liveRecordingNotVirtualVODCounter++;
					}
				}
				SPDLOG_INFO(
					"virtualVOD, numbers"
					", total LiveRecording: {}"
					", liveRecordingVirtualVODCounter: {}"
					", liveRecordingNotVirtualVODCounter: {}",
					liveRecordingVirtualVODCounter + liveRecordingNotVirtualVODCounter, liveRecordingVirtualVODCounter,
					liveRecordingNotVirtualVODCounter
				);
			}
			SPDLOG_INFO(
				"virtualVOD clone"
				", copiedRunningLiveRecordingCapability.size: {}"
				", @MMS statistics@ - elapsed (millisecs): {}",
				copiedRunningLiveRecordingCapability.size(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startClone).count()
			);

			chrono::system_clock::time_point startAllChannelsVirtualVOD = chrono::system_clock::now();

			for (int liveRecordingIndex = 0; liveRecordingIndex < copiedRunningLiveRecordingCapability.size(); liveRecordingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveRecording> copiedLiveRecording = copiedRunningLiveRecordingCapability[liveRecordingIndex];
				shared_ptr<FFMPEGEncoderBase::LiveRecording> sourceLiveRecording = sourceLiveRecordingCapability[liveRecordingIndex];

				SPDLOG_INFO(
					"virtualVOD"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", channelLabel: {}",
					copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
				);

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					SPDLOG_INFO(
						"virtualVOD. LiveRecorder changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
					);

					continue;
				}

				{
					chrono::system_clock::time_point startSingleChannelVirtualVOD = chrono::system_clock::now();

					virtualVODsNumber++;

					SPDLOG_INFO(
						"buildAndIngestVirtualVOD ..."
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", externalEncoder: {}"
						", _childProcessId: {}"
						", virtualVOD: {}"
						", virtualVODsNumber: {}"
						", monitorVirtualVODManifestDirectoryPath: {}"
						", monitorVirtualVODManifestFileName: {}"
						", virtualVODStagingContentsPath: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_externalEncoder,
						copiedLiveRecording->_childProcessId.toString(), copiedLiveRecording->_virtualVOD, virtualVODsNumber,
						copiedLiveRecording->_monitorVirtualVODManifestDirectoryPath, copiedLiveRecording->_monitorVirtualVODManifestFileName,
						copiedLiveRecording->_virtualVODStagingContentsPath
					);

					long segmentsNumber = 0;

					try
					{
						int64_t recordingCode = JSONUtils::asInt64(copiedLiveRecording->_ingestedParametersRoot, "recordingCode", 0);
						string ingestionJobLabel = JSONUtils::asString(copiedLiveRecording->_encodingParametersRoot, "ingestionJobLabel", "");
						string liveRecorderVirtualVODUniqueName =
							ingestionJobLabel + "(" + to_string(recordingCode) + "_" + to_string(copiedLiveRecording->_ingestionJobKey) + ")";

						int64_t userKey;
						string apiKey;
						{
							string field = "internalMMS";
							if (JSONUtils::isPresent(copiedLiveRecording->_ingestedParametersRoot, field))
							{
								json internalMMSRoot = copiedLiveRecording->_ingestedParametersRoot[field];

								field = "credentials";
								if (JSONUtils::isPresent(internalMMSRoot, field))
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

						// string mmsWorkflowIngestionURL;
						// string mmsBinaryIngestionURL;
						{
							/*
							string field = "mmsWorkflowIngestionURL";
							if (!JSONUtils::isPresent(copiedLiveRecording->_encodingParametersRoot, field))
							{
								string errorMessage = std::format(
									"Field is not present or it is null"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", Field: {}",
									copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, field
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
							mmsWorkflowIngestionURL = JSONUtils::asString(copiedLiveRecording->_encodingParametersRoot, field, "");

							field = "mmsBinaryIngestionURL";
							if (!JSONUtils::isPresent(copiedLiveRecording->_encodingParametersRoot, field))
							{
								string errorMessage = std::format(
									"Field is not present or it is null"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", Field: {}",
									copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, field
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
							mmsBinaryIngestionURL = JSONUtils::asString(copiedLiveRecording->_encodingParametersRoot, field, "");
							*/
						}

						segmentsNumber = buildAndIngestVirtualVOD(
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_externalEncoder,

							copiedLiveRecording->_monitorVirtualVODManifestDirectoryPath, copiedLiveRecording->_monitorVirtualVODManifestFileName,
							copiedLiveRecording->_virtualVODStagingContentsPath,

							recordingCode, ingestionJobLabel, liveRecorderVirtualVODUniqueName, _liveRecorderVirtualVODRetention,
							copiedLiveRecording->_liveRecorderVirtualVODImageMediaItemKey, userKey, apiKey, _mmsWorkflowIngestionURL,
							_mmsBinaryIngestionURL
						);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"buildAndIngestVirtualVOD failed"
							", copiedLiveRecording->_ingestionJobKey: {}"
							", copiedLiveRecording->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, e.what()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"buildAndIngestVirtualVOD failed"
							", copiedLiveRecording->_ingestionJobKey: {}"
							", copiedLiveRecording->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, e.what()
						);
					}

					SPDLOG_INFO(
						"Single Channel Virtual VOD"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentsNumber: {}"
						", @MMS statistics@ - elapsed time (secs): @{}@",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, segmentsNumber,
						chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startSingleChannelVirtualVOD).count()
					);
				}
			}

			SPDLOG_INFO(
				"All Channels Virtual VOD"
				", @MMS statistics@ - elapsed time: @{}@",
				chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startAllChannelsVirtualVOD).count()
			);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"liveRecorderVirtualVODIngestion failed"
				", e.what(): {}",
				e.what()
			);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"liveRecorderVirtualVODIngestion failed"
				", e.what(): {}",
				e.what()
			);
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
	int64_t ingestionJobKey, int64_t encodingJobKey, string streamSourceType, bool externalEncoder, int segmentDurationInSeconds,
	string outputFileFormat, json encodingParametersRoot, json ingestedParametersRoot, string chunksTranscoderStagingContentsPath,
	string chunksNFSStagingContentsPath, string segmentListFileName, string recordedFileNamePrefix, string lastRecordedAssetFileName,
	double lastRecordedAssetDurationInSeconds, int64_t lastRecordedSegmentUtcStartTimeInMillisecs
)
{

	// it is assigned to lastRecordedAssetFileName because in case no new files
	// are present, the same lastRecordedAssetFileName has to be returned
	string newLastRecordedAssetFileName = lastRecordedAssetFileName;
	double newLastRecordedAssetDurationInSeconds = lastRecordedAssetDurationInSeconds;
	int64_t newLastRecordedSegmentUtcStartTimeInMillisecs = lastRecordedSegmentUtcStartTimeInMillisecs;
	try
	{
		SPDLOG_INFO(
			"processStreamSegmenterOutput"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", segmentDurationInSeconds: {}"
			", outputFileFormat: {}"
			", chunksTranscoderStagingContentsPath: {}"
			", chunksNFSStagingContentsPath: {}"
			", segmentListFileName: {}"
			", recordedFileNamePrefix: {}"
			", lastRecordedAssetFileName: {}"
			", lastRecordedAssetDurationInSeconds: {}",
			ingestionJobKey, encodingJobKey, segmentDurationInSeconds, outputFileFormat, chunksTranscoderStagingContentsPath,
			chunksNFSStagingContentsPath, segmentListFileName, recordedFileNamePrefix, lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds
		);

		ifstream segmentList(chunksTranscoderStagingContentsPath + segmentListFileName);
		if (!segmentList)
		{
			SPDLOG_WARN(
				"No segment list file found yet"
				", chunksTranscoderStagingContentsPath: {}"
				", segmentListFileName: {}"
				", lastRecordedAssetFileName: {}",
				chunksTranscoderStagingContentsPath, segmentListFileName, lastRecordedAssetFileName
			);

			return make_tuple(lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs);
			// throw runtime_error(errorMessage);
		}

		bool reachedNextFileToProcess = false;
		string currentRecordedAssetFileName;
		while (getline(segmentList, currentRecordedAssetFileName))
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

			SPDLOG_INFO(
				"processing LiveRecorder file"
				", currentRecordedAssetFileName: {}",
				currentRecordedAssetFileName
			);

			if (!fs::exists(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName))
			{
				// it could be the scenario where mmsEngineService is restarted,
				// the segments list file still contains obsolete filenames
				SPDLOG_ERROR(
					"file not existing"
					", currentRecordedAssetPathName: {}{}",
					chunksTranscoderStagingContentsPath, currentRecordedAssetFileName
				);

				continue;
			}

			bool isFirstChunk = (lastRecordedAssetFileName == "");

			time_t utcCurrentRecordedFileCreationTime =
				getMediaLiveRecorderStartTime(ingestionJobKey, encodingJobKey, currentRecordedAssetFileName, segmentDurationInSeconds, isFirstChunk);

			/*
			time_t utcNow =
			chrono::system_clock::to_time_t(chrono::system_clock::now()); if
			(utcNow
			- utcCurrentRecordedFileCreationTime < _secondsToWaitNFSBuffers)
			{
				long secondsToWait = _secondsToWaitNFSBuffers
					- (utcNow - utcCurrentRecordedFileCreationTime);

				info(__FILEREF__ + "processing LiveRecorder file too
			young"
					+ ", secondsToWait: " + to_string(secondsToWait));
				this_thread::sleep_for(chrono::seconds(secondsToWait));
			}
			*/

			bool ingestionRowToBeUpdatedAsSuccess = isLastLiveRecorderFile(
				ingestionJobKey, encodingJobKey, utcCurrentRecordedFileCreationTime, chunksTranscoderStagingContentsPath, recordedFileNamePrefix,
				segmentDurationInSeconds, isFirstChunk
			);
			SPDLOG_INFO(
				"isLastLiveRecorderFile"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", chunksTranscoderStagingContentsPath: {}"
				", recordedFileNamePrefix: {}"
				", ingestionRowToBeUpdatedAsSuccess: {}",
				ingestionJobKey, encodingJobKey, chunksTranscoderStagingContentsPath, recordedFileNamePrefix, ingestionRowToBeUpdatedAsSuccess
			);

			newLastRecordedAssetFileName = currentRecordedAssetFileName;
			newLastRecordedAssetDurationInSeconds = segmentDurationInSeconds;
			newLastRecordedSegmentUtcStartTimeInMillisecs = utcCurrentRecordedFileCreationTime * 1000;

			/*
			 * 2019-10-17: we just saw that, even if the real duration is 59
			seconds,
			 * next utc time inside the chunk file name is still like +60 from
			the previuos chunk utc.
			 * For this reason next code was commented.
			try
			{
				int64_t durationInMilliSeconds;


				info(__FILEREF__ + "Calling ffmpeg.getMediaInfo"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", chunksTranscoderStagingContentsPath +
			currentRecordedAssetFileName: "
						+ (chunksTranscoderStagingContentsPath +
			currentRecordedAssetFileName)
				);
				FFMpeg ffmpeg (_configuration, _logger);
				tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
			mediaInfo = ffmpeg.getMediaInfo(chunksTranscoderStagingContentsPath
			+ currentRecordedAssetFileName);

				tie(durationInMilliSeconds, ignore,
					ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore, ignore) = mediaInfo;

				newLastRecordedAssetDurationInSeconds = durationInMilliSeconds /
			1000;

				if (newLastRecordedAssetDurationInSeconds !=
			segmentDurationInSeconds)
				{
					warn(__FILEREF__ + "segment duration is different
			from file duration"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", durationInMilliSeconds: " + to_string
			(durationInMilliSeconds)
						+ ", newLastRecordedAssetDurationInSeconds: "
							+ to_string (newLastRecordedAssetDurationInSeconds)
						+ ", segmentDurationInSeconds: " + to_string
			(segmentDurationInSeconds)
					);
				}
			}
			catch(exception& e)
			{
				error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", chunksTranscoderStagingContentsPath +
			currentRecordedAssetFileName: "
						+ (chunksTranscoderStagingContentsPath +
			currentRecordedAssetFileName)
				);
			}
			*/

			time_t utcCurrentRecordedFileLastModificationTime = utcCurrentRecordedFileCreationTime + newLastRecordedAssetDurationInSeconds;
			/*
			time_t utcCurrentRecordedFileLastModificationTime =
			getMediaLiveRecorderEndTime( currentRecordedAssetPathName);
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
			json userDataRoot;
			{
				if (JSONUtils::isPresent(ingestedParametersRoot, "userData"))
					userDataRoot = ingestedParametersRoot["userData"];

				json mmsDataRoot;

				json liveRecordingChunkRoot;
				{
					int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);
					// recordingCode is used by DB generated column
					liveRecordingChunkRoot["recordingCode"] = recordingCode;
				}
				liveRecordingChunkRoot["ingestionJobLabel"] = ingestionJobLabel;
				liveRecordingChunkRoot["ingestionJobKey"] = (int64_t)(ingestionJobKey);
				liveRecordingChunkRoot["utcPreviousChunkStartTime"] =
					(time_t)(utcCurrentRecordedFileCreationTime - lastRecordedAssetDurationInSeconds);
				liveRecordingChunkRoot["utcChunkStartTime"] = utcCurrentRecordedFileCreationTime;
				liveRecordingChunkRoot["utcChunkEndTime"] = utcCurrentRecordedFileLastModificationTime;

				liveRecordingChunkRoot["uniqueName"] = uniqueName;

				mmsDataRoot["liveRecordingChunk"] = liveRecordingChunkRoot;

				userDataRoot["mmsData"] = mmsDataRoot;
			}

			// Title
			string addContentTitle;
			{
				/*
				if (streamSourceType == "IP_PUSH")
				{
					int64_t recordingCode =
				JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);
					addContentTitle = to_string(recordingCode);
				}
				else
				{
					// 2021-02-03: in this case, we will use the
				'ConfigurationLabel' that
					// it is much better that a code. Who will see the title of
				the chunks will recognize
					// easily the recording
					addContentTitle =
				ingestedParametersRoot.get("configurationLabel",
				"").asString();
				}
				*/
				// string ingestionJobLabel =
				// encodingParametersRoot.get("ingestionJobLabel",
				// "").asString();
				if (ingestionJobLabel == "")
				{
					int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);
					addContentTitle = to_string(recordingCode);
				}
				else
					addContentTitle = ingestionJobLabel;

				addContentTitle += " - ";

				{
					tm tmDateTime;
					// char strCurrentRecordedFileTime[64];

					// from utc to local time
					localtime_r(&utcCurrentRecordedFileCreationTime, &tmDateTime);

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

					// sprintf(strCurrentRecordedFileTime, "%02d:%02d:%02d", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec);

					// addContentTitle += strCurrentRecordedFileTime; // local time
					addContentTitle += std::format("{:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec);
				}

				addContentTitle += " - ";

				{
					tm tmDateTime;
					// char strCurrentRecordedFileTime[64];

					// from utc to local time
					localtime_r(&utcCurrentRecordedFileLastModificationTime, &tmDateTime);

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
					// sprintf(strCurrentRecordedFileTime, "%02d:%02d:%02d", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec);

					// addContentTitle += strCurrentRecordedFileTime; // local time
					addContentTitle += std::format("{:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec);
				}

				// if (!main)
				// 	addContentTitle += " (BCK)";
			}

			if (isFirstChunk)
			{
				SPDLOG_INFO(
					"The first asset file name is not ingested because it does not contain the entire period and it will be removed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", currentRecordedAssetPathName: {}{}"
					", title: {}",
					ingestionJobKey, encodingJobKey, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName, addContentTitle
				);

				SPDLOG_INFO(
					"Remove"
					", currentRecordedAssetPathName: {}{}",
					chunksTranscoderStagingContentsPath, currentRecordedAssetFileName
				);
				fs::remove_all(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);
			}
			else
			{
				try
				{
					SPDLOG_INFO(
						"ingest Recorded media"
						", ingestionJobKey: {}"
						", encodingJobKey: "
						", chunksTranscoderStagingContentsPath: {}"
						", currentRecordedAssetFileName: {}"
						", chunksNFSStagingContentsPath: {}"
						", addContentTitle: {}",
						ingestionJobKey, encodingJobKey, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName,
						chunksNFSStagingContentsPath, addContentTitle
					);

					if (externalEncoder)
						ingestRecordedMediaInCaseOfExternalTranscoder(
							ingestionJobKey, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName, addContentTitle, uniqueName,
							/* highAvailability, */ userDataRoot, outputFileFormat, ingestedParametersRoot, encodingParametersRoot
						);
					else
						ingestRecordedMediaInCaseOfInternalTranscoder(
							ingestionJobKey, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName, chunksNFSStagingContentsPath,
							addContentTitle, uniqueName,
							/* highAvailability, */ userDataRoot, outputFileFormat, ingestedParametersRoot, encodingParametersRoot, false
						);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"ingestRecordedMedia failed"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", externalEncoder: {}"
						", chunksTranscoderStagingContentsPath: {}"
						", currentRecordedAssetFileName: {}"
						", chunksNFSStagingContentsPath: {}"
						", addContentTitle: {}"
						", outputFileFormat: {}"
						", e.what(): {}",
						encodingJobKey, ingestionJobKey, externalEncoder, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName,
						chunksNFSStagingContentsPath, addContentTitle, outputFileFormat, e.what()
					);

					// throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"ingestRecordedMedia failed"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", externalEncoder: {}"
						", chunksTranscoderStagingContentsPath: {}"
						", currentRecordedAssetFileName: {}"
						", chunksNFSStagingContentsPath: {}"
						", addContentTitle: {}"
						", outputFileFormat: {}"
						", e.what(): {}",
						encodingJobKey, ingestionJobKey, externalEncoder, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName,
						chunksNFSStagingContentsPath, addContentTitle, outputFileFormat, e.what()
					);

					// throw e;
				}
			}
		}

		if (reachedNextFileToProcess == false)
		{
			// this scenario should never happens, we have only one option when
			// mmEngineService is restarted, the new LiveRecorder is not started
			// and the segments list file contains still old files. So
			// newLastRecordedAssetFileName is initialized with the old file
			// that will never be found once LiveRecorder starts and reset the
			// segment list file In this scenario, we will reset
			// newLastRecordedAssetFileName
			newLastRecordedAssetFileName = "";
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processStreamSegmenterOutput failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", chunksTranscoderStagingContentsPath: {}"
			", chunksNFSStagingContentsPath: {}"
			", segmentListFileName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, chunksTranscoderStagingContentsPath, chunksNFSStagingContentsPath, segmentListFileName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processStreamSegmenterOutput failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", chunksTranscoderStagingContentsPath: {}"
			", chunksNFSStagingContentsPath: {}"
			", segmentListFileName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, chunksTranscoderStagingContentsPath, chunksNFSStagingContentsPath, segmentListFileName, e.what()
		);

		throw e;
	}

	return make_tuple(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs);
}

tuple<string, double, int64_t> LiveRecorderDaemons::processHLSSegmenterOutput(
	int64_t ingestionJobKey, int64_t encodingJobKey, string streamSourceType, bool externalEncoder, int segmentDurationInSeconds,
	string outputFileFormat, json encodingParametersRoot, json ingestedParametersRoot, string chunksTranscoderStagingContentsPath,
	string chunksNFSStagingContentsPath, string segmentListFileName, string recordedFileNamePrefix, string lastRecordedAssetFileName,
	double lastRecordedAssetDurationInSeconds, int64_t lastRecordedSegmentUtcStartTimeInMillisecs
)
{

	string newLastRecordedAssetFileName = lastRecordedAssetFileName;
	double newLastRecordedAssetDurationInSeconds = lastRecordedAssetDurationInSeconds;
	int64_t newLastRecordedSegmentUtcStartTimeInMillisecs = lastRecordedSegmentUtcStartTimeInMillisecs;

	try
	{
		SPDLOG_INFO(
			"processHLSSegmenterOutput"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", segmentDurationInSeconds: {}"
			", outputFileFormat: {}"
			", chunksTranscoderStagingContentsPath: {}"
			", chunksNFSStagingContentsPath: {}"
			", segmentListFileName: {}"
			", recordedFileNamePrefix: {}"
			", lastRecordedAssetFileName: {}"
			", lastRecordedAssetDurationInSeconds: {}",
			ingestionJobKey, encodingJobKey, segmentDurationInSeconds, outputFileFormat, chunksTranscoderStagingContentsPath,
			chunksNFSStagingContentsPath, segmentListFileName, recordedFileNamePrefix, lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds
		);

		double toBeIngestedSegmentDuration = -1.0;
		int64_t toBeIngestedSegmentUtcStartTimeInMillisecs = -1;
		string toBeIngestedSegmentFileName;
		{
			double currentSegmentDuration = -1.0;
			int64_t currentSegmentUtcStartTimeInMillisecs = -1;
			string currentSegmentFileName;

			bool toBeIngested = false;

			SPDLOG_INFO(
				"Reading manifest"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", chunksTranscoderStagingContentsPath + segmentListFileName: {}{}",
				ingestionJobKey, encodingJobKey, chunksTranscoderStagingContentsPath, segmentListFileName
			);

			ifstream segmentList;
			segmentList.open(chunksTranscoderStagingContentsPath + segmentListFileName, ifstream::in);
			if (!segmentList)
			{
				SPDLOG_WARN(
					"No segment list file found yet"
					", chunksTranscoderStagingContentsPath: {}"
					", segmentListFileName: {}"
					", lastRecordedAssetFileName: {}",
					chunksTranscoderStagingContentsPath, segmentListFileName, lastRecordedAssetFileName
				);

				return make_tuple(lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs);
				// throw runtime_error(errorMessage);
			}

			int ingestionNumber = 0;
			string manifestLine;
			while (getline(segmentList, manifestLine))
			{
				SPDLOG_INFO(
					"Reading manifest line"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", manifestLine: {}"
					", toBeIngested: {}"
					", toBeIngestedSegmentDuration: {}"
					", toBeIngestedSegmentUtcStartTimeInMillisecs: {}"
					", toBeIngestedSegmentFileName: {}"
					", currentSegmentDuration: {}"
					", currentSegmentUtcStartTimeInMillisecs: {}"
					", currentSegmentFileName: {}"
					", lastRecordedAssetFileName: {}"
					", newLastRecordedAssetFileName: {}"
					", newLastRecordedSegmentUtcStartTimeInMillisecs: {}",
					ingestionJobKey, encodingJobKey, manifestLine, toBeIngested, toBeIngestedSegmentDuration,
					toBeIngestedSegmentUtcStartTimeInMillisecs, toBeIngestedSegmentFileName, currentSegmentDuration,
					currentSegmentUtcStartTimeInMillisecs, currentSegmentFileName, lastRecordedAssetFileName, newLastRecordedAssetFileName,
					newLastRecordedSegmentUtcStartTimeInMillisecs
				);

				// #EXTINF:14.640000,
				// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
				// <segment file name>

				if (manifestLine.size() == 0)
					continue;

				string durationPrefix("#EXTINF:");
				string dateTimePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
				if (manifestLine.size() >= durationPrefix.size() && 0 == manifestLine.compare(0, durationPrefix.size(), durationPrefix))
				{
					size_t endOfSegmentDuration = manifestLine.find(",");
					if (endOfSegmentDuration == string::npos)
					{
						string errorMessage = std::format(
							"wrong manifest line format"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", manifestLine: {}",
							ingestionJobKey, encodingJobKey, manifestLine
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					if (toBeIngested)
						toBeIngestedSegmentDuration = stod(manifestLine.substr(durationPrefix.size(), endOfSegmentDuration - durationPrefix.size()));
					else
						currentSegmentDuration = stod(manifestLine.substr(durationPrefix.size(), endOfSegmentDuration - durationPrefix.size()));
				}
				else if (manifestLine.size() >= dateTimePrefix.size() && 0 == manifestLine.compare(0, dateTimePrefix.size(), dateTimePrefix))
				{
					if (toBeIngested)
						toBeIngestedSegmentUtcStartTimeInMillisecs = Datetime::sDateMilliSecondsToUtc(manifestLine.substr(dateTimePrefix.size()));
					else
						currentSegmentUtcStartTimeInMillisecs = Datetime::sDateMilliSecondsToUtc(manifestLine.substr(dateTimePrefix.size()));
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
					SPDLOG_INFO(
						"manifest line not used by our algorithm"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", manifestLine: {}",
						ingestionJobKey, encodingJobKey, manifestLine
					);

					continue;
				}

				if (
                    // case 1: we are in the toBeIngested status (part of the
                    // playlist after the last ingested segment)
                    //	and we are all the details of the new ingested segment
                    (toBeIngested && toBeIngestedSegmentDuration != -1.0 &&
                     toBeIngestedSegmentUtcStartTimeInMillisecs != -1 &&
                     toBeIngestedSegmentFileName != "") ||
                    // case 2: we are NOT in the toBeIngested status
                    //	but we just started to ingest (lastRecordedAssetFileName
                    //== "") 	and we have all the details of the ingested
                    // segment
                    (!toBeIngested && currentSegmentDuration != -1.0 &&
                     currentSegmentUtcStartTimeInMillisecs != -1 &&
                     currentSegmentFileName != "" &&
                     lastRecordedAssetFileName == ""))
				{
					// if we are in case 2, let's initialize variables like we
					// are in case 1
					if (!toBeIngested)
					{
						toBeIngestedSegmentDuration = currentSegmentDuration;
						toBeIngestedSegmentUtcStartTimeInMillisecs = currentSegmentUtcStartTimeInMillisecs;
						toBeIngestedSegmentFileName = currentSegmentFileName;

						toBeIngested = true;
					}

					// ingest the asset and initilize
					// newLastRecordedAssetFileName,
					// newLastRecordedAssetDurationInSeconds and
					// newLastRecordedSegmentUtcStartTimeInMillisecs
					{
						int64_t toBeIngestedSegmentUtcEndTimeInMillisecs =
							toBeIngestedSegmentUtcStartTimeInMillisecs + (toBeIngestedSegmentDuration * 1000);

						SPDLOG_INFO(
							"processing LiveRecorder file"
							", toBeIngestedSegmentDuration: {}"
							", toBeIngestedSegmentUtcStartTimeInMillisecs: {}"
							", toBeIngestedSegmentUtcEndTimeInMillisecs: {}"
							", toBeIngestedSegmentFileName: {}",
							toBeIngestedSegmentDuration, toBeIngestedSegmentUtcStartTimeInMillisecs, toBeIngestedSegmentUtcEndTimeInMillisecs,
							toBeIngestedSegmentFileName
						);

						if (!fs::exists(chunksTranscoderStagingContentsPath + toBeIngestedSegmentFileName))
						{
							// it could be the scenario where mmsEngineService
							// is restarted, the segments list file still
							// contains obsolete filenames
							SPDLOG_ERROR(
								"file not existing"
								", currentRecordedAssetPathName: {}{}",
								chunksTranscoderStagingContentsPath, toBeIngestedSegmentFileName
							);

							return make_tuple(
								newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs
							);
						}
						else if (toBeIngestedSegmentUtcStartTimeInMillisecs <= newLastRecordedSegmentUtcStartTimeInMillisecs)
						{
							// toBeIngestedSegmentUtcStartTimeInMillisecs:
							// indica il nuovo RecordedSegmentUtcStartTime
							// newLastRecordedSegmentUtcStartTimeInMillisecs:
							// indica il precedente RecordedSegmentUtcStartTime
							// 2023-03-28: nella registrazione delle partite ho
							// notato che una volta che la partita è terminata,
							//		poichè per sicurezza avevo messo un
							// orario di
							// fine
							// registrazione parecchio piu 		avanti,
							// sono ancora stati ingestati 200 media items con
							// orari simili, l'ultimo media item di soli 30
							// minuti piu avanti anzicchè 200 minuti piu avanti.
							// Questo controllo garantisce anche che i tempi
							// 'start time' siano consistenti (sempre crescenti)

							SPDLOG_WARN(
								"media item not ingested because his start time <= last ingested start time"
								", toBeIngestedSegmentFileName: {}"
								", toBeIngestedSegmentUtcStartTimeInMillisecs (new start time): {}"
								", newLastRecordedSegmentUtcStartTimeInMillisecs (previous start time): {}",
								toBeIngestedSegmentFileName, toBeIngestedSegmentUtcStartTimeInMillisecs, newLastRecordedSegmentUtcStartTimeInMillisecs
							);

							return make_tuple(
								newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs
							);
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
							json userDataRoot;
							{
								if (JSONUtils::isPresent(ingestedParametersRoot, "userData"))
									userDataRoot = ingestedParametersRoot["userData"];

								json mmsDataRoot;

								json liveRecordingChunkRoot;

								{
									int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);
									// recordingCode is used by DB generated
									// column
									liveRecordingChunkRoot["recordingCode"] = recordingCode;
								}
								liveRecordingChunkRoot["ingestionJobLabel"] = ingestionJobLabel;
								liveRecordingChunkRoot["ingestionJobKey"] = (int64_t)(ingestionJobKey);
								liveRecordingChunkRoot["utcChunkStartTime"] = (int64_t)(toBeIngestedSegmentUtcStartTimeInMillisecs / 1000);
								// utcStartTimeInMilliSecs is used by DB
								// generated column
								mmsDataRoot["utcStartTimeInMilliSecs"] = toBeIngestedSegmentUtcStartTimeInMillisecs;

								liveRecordingChunkRoot["utcChunkEndTime"] = (int64_t)(toBeIngestedSegmentUtcEndTimeInMillisecs / 1000);
								// utcStartTimeInMilliSecs is used by DB
								// generated column
								mmsDataRoot["utcEndTimeInMilliSecs"] = toBeIngestedSegmentUtcEndTimeInMillisecs;

								liveRecordingChunkRoot["uniqueName"] = uniqueName;

								mmsDataRoot["liveRecordingChunk"] = liveRecordingChunkRoot;

								userDataRoot["mmsData"] = mmsDataRoot;
							}

							// Title
							string addContentTitle;
							{
								if (ingestionJobLabel == "")
								{
									int64_t recordingCode = JSONUtils::asInt64(ingestedParametersRoot, "recordingCode", 0);
									addContentTitle = to_string(recordingCode);
								}
								else
									addContentTitle = ingestionJobLabel;

								addContentTitle += " - ";

								{
									tm tmDateTime;
									// char strCurrentRecordedFileTime[64];

									time_t toBeIngestedSegmentUtcStartTimeInSeconds = toBeIngestedSegmentUtcStartTimeInMillisecs / 1000;
									int toBeIngestedSegmentMilliSecs = toBeIngestedSegmentUtcStartTimeInMillisecs % 1000;

									// from utc to local time
									localtime_r(&toBeIngestedSegmentUtcStartTimeInSeconds, &tmDateTime);

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

									/*
									sprintf(
										strCurrentRecordedFileTime, "%02d:%02d:%02d.%03d", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec,
										toBeIngestedSegmentMilliSecs
									);
									*/

									// addContentTitle += strCurrentRecordedFileTime; // local
									addContentTitle += std::format(
										"{:0>2}:{:0>2}:{:0>2}.{:0>3}", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec,
										toBeIngestedSegmentMilliSecs
									);
									// time
								}

								addContentTitle += " - ";

								{
									tm tmDateTime;
									// char strCurrentRecordedFileTime[64];

									time_t toBeIngestedSegmentUtcEndTimeInSeconds = toBeIngestedSegmentUtcEndTimeInMillisecs / 1000;
									int toBeIngestedSegmentMilliSecs = toBeIngestedSegmentUtcEndTimeInMillisecs % 1000;

									// from utc to local time
									localtime_r(&toBeIngestedSegmentUtcEndTimeInSeconds, &tmDateTime);

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
									/*
									sprintf(
										strCurrentRecordedFileTime, "%02d:%02d:%02d.%03d", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec,
										toBeIngestedSegmentMilliSecs
									);

									addContentTitle += strCurrentRecordedFileTime; // local
									*/
									addContentTitle += std::format(
										"{:0>2}:{:0>2}:{:0>2}.{:0>3}", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec,
										toBeIngestedSegmentMilliSecs
									);
									// time
								}

								// if (!main)
								// 	addContentTitle += " (BCK)";
							}

							{
								try
								{
									SPDLOG_INFO(
										"ingest Recorded media"
										", ingestionJobKey: {}"
										", encodingJobKey: {}"
										", chunksTranscoderStagingContentsPath: {}"
										", toBeIngestedSegmentFileName: {}"
										", chunksNFSStagingContentsPath: {}"
										", addContentTitle: {}",
										ingestionJobKey, encodingJobKey, chunksTranscoderStagingContentsPath, toBeIngestedSegmentFileName,
										chunksNFSStagingContentsPath, addContentTitle
									);

									if (externalEncoder)
										ingestRecordedMediaInCaseOfExternalTranscoder(
											ingestionJobKey, chunksTranscoderStagingContentsPath, toBeIngestedSegmentFileName, addContentTitle,
											uniqueName, userDataRoot, outputFileFormat, ingestedParametersRoot, encodingParametersRoot
										);
									else
										ingestRecordedMediaInCaseOfInternalTranscoder(
											ingestionJobKey, chunksTranscoderStagingContentsPath, toBeIngestedSegmentFileName,
											chunksNFSStagingContentsPath, addContentTitle, uniqueName, userDataRoot, outputFileFormat,
											ingestedParametersRoot, encodingParametersRoot, true
										);
								}
								catch (runtime_error &e)
								{
									SPDLOG_ERROR(
										"ingestRecordedMedia failed"
										", encodingJobKey: {}"
										", ingestionJobKey: {}"
										", externalEncoder: {}"
										", chunksTranscoderStagingContentsPath: {}"
										", toBeIngestedSegmentFileName: {}"
										", chunksNFSStagingContentsPath: {}"
										", addContentTitle: {}"
										", outputFileFormat: {}"
										", e.what(): {}",
										encodingJobKey, ingestionJobKey, externalEncoder, chunksTranscoderStagingContentsPath,
										toBeIngestedSegmentFileName, chunksNFSStagingContentsPath, addContentTitle, outputFileFormat, e.what()
									);

									// throw e;
								}
								catch (exception &e)
								{
									SPDLOG_ERROR(
										"ingestRecordedMedia failed"
										", encodingJobKey: {}"
										", ingestionJobKey: {}"
										", externalEncoder: {}"
										", chunksTranscoderStagingContentsPath: {}"
										", toBeIngestedSegmentFileName: {}"
										", chunksNFSStagingContentsPath: {}"
										", addContentTitle: {}"
										", outputFileFormat: {}"
										", e.what(): {}",
										encodingJobKey, ingestionJobKey, externalEncoder, chunksTranscoderStagingContentsPath,
										toBeIngestedSegmentFileName, chunksNFSStagingContentsPath, addContentTitle, outputFileFormat, e.what()
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
			//	we have lastRecordedAssetFileName with a filename that does not
			// exist into the playlist
			// This is a scenario that should never happen but, in case it
			// happens, we have to manage otherwise no chunks will be ingested
			// lastRecordedAssetFileName has got from playlist in the previous
			// processHLSSegmenterOutput call
			if (lastRecordedAssetFileName != "" && !toBeIngested // file name does not exist into the playlist
			)
			{
				// 2022-08-12: this scenario happens when the 'monitor process'
				// kills the recording process,
				//	so the playlist is reset and start from scratch.

				SPDLOG_WARN(
					"Filename not found: probable the playlist was reset (may be because of a kill of the monitor process)"
					", encodingJobKey: {}"
					", ingestionJobKey: {}"
					", toBeIngested: {}"
					", toBeIngestedSegmentDuration: {}"
					", toBeIngestedSegmentUtcStartTimeInMillisecs: {}"
					", toBeIngestedSegmentFileName: {}"
					", currentSegmentDuration: {}"
					", currentSegmentUtcStartTimeInMillisecs: {}"
					", currentSegmentFileName: {}"
					", lastRecordedAssetFileName: {}"
					", newLastRecordedAssetFileName: {}"
					", newLastRecordedSegmentUtcStartTimeInMillisecs: {}",
					encodingJobKey, ingestionJobKey, toBeIngested, toBeIngestedSegmentDuration, toBeIngestedSegmentUtcStartTimeInMillisecs,
					toBeIngestedSegmentFileName, currentSegmentDuration, currentSegmentUtcStartTimeInMillisecs, currentSegmentFileName,
					lastRecordedAssetFileName, newLastRecordedAssetFileName, newLastRecordedSegmentUtcStartTimeInMillisecs
				);

				newLastRecordedAssetFileName = "";
				newLastRecordedAssetDurationInSeconds = 0.0;
				newLastRecordedSegmentUtcStartTimeInMillisecs = -1;
			}
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processHLSSegmenterOutput failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", chunksTranscoderStagingContentsPath: {}"
			", chunksNFSStagingContentsPath: {}"
			", segmentListFileName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, chunksTranscoderStagingContentsPath, chunksNFSStagingContentsPath, segmentListFileName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processHLSSegmenterOutput failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", chunksTranscoderStagingContentsPath: {}"
			", chunksNFSStagingContentsPath: {}"
			", segmentListFileName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, chunksTranscoderStagingContentsPath, chunksNFSStagingContentsPath, segmentListFileName, e.what()
		);

		throw e;
	}

	return make_tuple(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds, newLastRecordedSegmentUtcStartTimeInMillisecs);
}

void LiveRecorderDaemons::ingestRecordedMediaInCaseOfInternalTranscoder(
	int64_t ingestionJobKey, string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName, string chunksNFSStagingContentsPath,
	string addContentTitle, string uniqueName,
	// bool highAvailability,
	json userDataRoot, string fileFormat, json ingestedParametersRoot, json encodingParametersRoot, bool copy
)
{
	try
	{
		// moving chunk from transcoder staging path to shared staging path.
		// This is done because the AddContent task has a move://... url
		if (copy)
		{
			SPDLOG_INFO(
				"Chunk copying"
				", ingestionJobKey: {}"
				", currentRecordedAssetFileName: {}"
				", source: {}{}"
				", dest: {}",
				ingestionJobKey, currentRecordedAssetFileName, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName,
				chunksNFSStagingContentsPath
			);

			chrono::system_clock::time_point startCopying = chrono::system_clock::now();
			fs::copy(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName, chunksNFSStagingContentsPath);
			chrono::system_clock::time_point endCopying = chrono::system_clock::now();

			SPDLOG_INFO(
				"Chunk copied"
				", ingestionJobKey: {}"
				", source: {}{}"
				", dest: {}"
				", @MMS COPY statistics@ - copyingDuration (secs): @{}@",
				ingestionJobKey, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName, chunksNFSStagingContentsPath,
				chrono::duration_cast<chrono::seconds>(endCopying - startCopying).count()
			);
		}
		else
		{
			SPDLOG_INFO(
				"Chunk moving"
				", ingestionJobKey: {}"
				", currentRecordedAssetFileName: {}"
				", source: {}{}"
				", dest: {}",
				ingestionJobKey, currentRecordedAssetFileName, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName,
				chunksNFSStagingContentsPath
			);

			chrono::system_clock::time_point startMoving = chrono::system_clock::now();
			MMSStorage::move(ingestionJobKey, chunksTranscoderStagingContentsPath + currentRecordedAssetFileName, chunksNFSStagingContentsPath);
			chrono::system_clock::time_point endMoving = chrono::system_clock::now();

			SPDLOG_INFO(
				"Chunk moved"
				", ingestionJobKey: {}"
				", source: {}{}"
				", dest: {}"
				", @MMS MOVE statistics@ - movingDuration (secs): @{}@",
				ingestionJobKey, chunksTranscoderStagingContentsPath, currentRecordedAssetFileName, chunksNFSStagingContentsPath,
				chrono::duration_cast<chrono::seconds>(endMoving - startMoving).count()
			);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = e.what();
		SPDLOG_ERROR(
			"Coping/Moving of the chunk failed"
			", ingestionJobKey: {}"
			", exception: {}",
			ingestionJobKey, errorMessage
		);
		if (errorMessage.find(string("errno: 28")) != string::npos)
			SPDLOG_ERROR(
				"No space left on storage"
				", ingestionJobKey: {}"
				", exception: {}",
				ingestionJobKey, errorMessage
			);

		SPDLOG_INFO(
			"remove"
			", generated chunk: {}{}",
			chunksTranscoderStagingContentsPath, currentRecordedAssetFileName
		);
		fs::remove_all(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);

		throw e;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"Ingested URL failed"
			", ingestionJobKey: {}"
			", exception: {}",
			ingestionJobKey, e.what()
		);

		SPDLOG_INFO(
			"remove"
			", generated chunk: {}{}",
			chunksTranscoderStagingContentsPath, currentRecordedAssetFileName
		);
		fs::remove_all(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);

		throw e;
	}

	// string mmsWorkflowIngestionURL;
	string workflowMetadata;
	try
	{
		workflowMetadata = buildChunkIngestionWorkflow(
			ingestionJobKey,
			false, // externalEncoder,
			currentRecordedAssetFileName, chunksNFSStagingContentsPath, addContentTitle, uniqueName, userDataRoot, fileFormat, ingestedParametersRoot,
			encodingParametersRoot
		);

		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isPresent(ingestedParametersRoot, field))
			{
				json internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isPresent(internalMMSRoot, field))
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
			/*
			string field = "mmsWorkflowIngestionURL";
			if (!JSONUtils::isPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", ingestionJobKey: {}"
					", Field: {}",
					ingestionJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
			*/
		}

		vector<string> otherHeaders;
		string sResponse =
			CurlWrapper::httpPostString(
				_mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), workflowMetadata,
				"application/json", // contentType
				otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey),
				3 // maxRetryNumber
			)
				.second;
	}
	catch (exception& e)
	{
		SPDLOG_ERROR(
			"Ingested URL failed"
			", ingestionJobKey: {}"
			", mmsWorkflowIngestionURL: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, _mmsWorkflowIngestionURL, workflowMetadata, e.what()
		);

		throw;
	}
}

void LiveRecorderDaemons::ingestRecordedMediaInCaseOfExternalTranscoder(
	int64_t ingestionJobKey, string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName, string addContentTitle,
	string uniqueName, json userDataRoot, string fileFormat, json ingestedParametersRoot, json encodingParametersRoot
)
{
	string workflowMetadata;
	int64_t userKey;
	string apiKey;
	int64_t addContentIngestionJobKey = -1;
	// string mmsWorkflowIngestionURL;
	// create the workflow and ingest it
	try
	{
		workflowMetadata = buildChunkIngestionWorkflow(
			ingestionJobKey,
			true, // externalEncoder,
			"",	  // currentRecordedAssetFileName,
			"",	  // chunksNFSStagingContentsPath,
			addContentTitle, uniqueName, userDataRoot, fileFormat, ingestedParametersRoot, encodingParametersRoot
		);

		if (JSONUtils::isPresent(ingestedParametersRoot, "internalMMS"))
		{
			json internalMMSRoot = ingestedParametersRoot["internalMMS"];

			if (JSONUtils::isPresent(internalMMSRoot, "credentials"))
			{
				json credentialsRoot = internalMMSRoot["credentials"];

				userKey = JSONUtils::asInt64(credentialsRoot, "userKey", -1);

				string apiKeyEncrypted = JSONUtils::asString(credentialsRoot, "apiKey", "");
				apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
			}
		}

		{
			/*
			if (!JSONUtils::isPresent(encodingParametersRoot, "mmsWorkflowIngestionURL"))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", ingestionJobKey: {}"
					", Field: mmsWorkflowIngestionURL",
					ingestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, "mmsWorkflowIngestionURL", "");
			*/
		}

		vector<string> otherHeaders;
		string sResponse =
			CurlWrapper::httpPostString(
				_mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), workflowMetadata,
				"application/json", // contentType
				otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey),
				3 // maxRetryNumber
			)
				.second;

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (exception& e)
	{
		SPDLOG_ERROR(
			"Ingestion workflow failed"
			", ingestionJobKey: {}"
			", mmsWorkflowIngestionURL: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, _mmsWorkflowIngestionURL, workflowMetadata, e.what()
		);

		throw;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage = std::format(
			"Ingested URL failed, addContentIngestionJobKey is not valid"
			", ingestionJobKey: {}",
			ingestionJobKey
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
#ifdef SAFEFILESYSTEMTHREAD
		int64_t chunkFileSize = SafeFileSystem::fileSizeThread(
			chunksTranscoderStagingContentsPath + currentRecordedAssetFileName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey)
		);
#elif SAFEFILESYSTEMPROCESS
		int64_t chunkFileSize = SafeFileSystem::fileSizeProcess(
			chunksTranscoderStagingContentsPath + currentRecordedAssetFileName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey)
		);
#else
		int64_t chunkFileSize = fs::file_size(chunksTranscoderStagingContentsPath + currentRecordedAssetFileName);
#endif

		/*
		string mmsBinaryIngestionURL;
		{
			if (!JSONUtils::isPresent(encodingParametersRoot, "mmsBinaryIngestionURL"))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", ingestionJobKey: {}"
					", Field: mmsBinaryIngestionURL",
					ingestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsBinaryIngestionURL = JSONUtils::asString(encodingParametersRoot, "mmsBinaryIngestionURL", "");
		}
		*/

		mmsBinaryURL = std::format("{}/{}", _mmsBinaryIngestionURL, addContentIngestionJobKey);

		string sResponse = CurlWrapper::httpPostFile(
			mmsBinaryURL, _mmsBinaryTimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey),
			chunksTranscoderStagingContentsPath + currentRecordedAssetFileName, chunkFileSize, "",
			std::format(", ingestionJobKey: {}", ingestionJobKey),
			3 // maxRetryNumber
		);
	}
	catch (runtime_error e)
	{
		SPDLOG_ERROR(
			"Ingestion binary failed"
			", ingestionJobKey: {}"
			", mmsBinaryURL: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, mmsBinaryURL, workflowMetadata, e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"Ingestion binary failed (exception)"
			", ingestionJobKey: {}"
			", mmsBinaryURL: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, mmsBinaryURL, workflowMetadata, e.what()
		);

		throw e;
	}
}

string LiveRecorderDaemons::buildChunkIngestionWorkflow(
	int64_t ingestionJobKey, bool externalEncoder, const string& currentRecordedAssetFileName, const string& chunksNFSStagingContentsPath,
	const string& addContentTitle, const string& uniqueName, const json& userDataRoot, string fileFormat, const json& ingestedParametersRoot,
	const json& encodingParametersRoot
)
{
	string workflowMetadata;
	try
	{
		/*
		{
			"label": "<workflow label>",
			"type": "Workflow",
			"task": {
				"label": "<task label 1>",
				"type": "Add-Content"
				"parameters": {
						"fileFormat": "ts",
						"ingester": "Giuliano",
						"sourceURL": "move:///abc...."
				},
			}
		}
		*/
		json mmsDataRoot = JsonPath(&userDataRoot)["mmsData"].as<json>();
		json liveRecordingChunkRoot = JsonPath(&mmsDataRoot)["liveRecordingChunk"].as<json>();
		auto utcPreviousChunkStartTime = JsonPath(&liveRecordingChunkRoot)["utcPreviousChunkStartTime"].as<int64_t>(-1);
		time_t utcChunkStartTime = JsonPath(&liveRecordingChunkRoot)["utcChunkStartTime"].as<int64_t>(-1);
		auto utcChunkEndTime = JsonPath(&liveRecordingChunkRoot)["utcChunkEndTime"].as<int64_t>(-1);

		json addContentRoot;

		addContentRoot["label"] = to_string(utcChunkStartTime);
		addContentRoot["type"] = "Add-Content";

		{
			json internalMMSRoot = JsonPath(&ingestedParametersRoot)["internalMMS"].as<json>(nullptr);
			if (internalMMSRoot != nullptr)
			{
				json eventsRoot = JsonPath(&internalMMSRoot)["events"].as<json>(nullptr);
				if (eventsRoot != nullptr)
				{
					if (json onEventRoot = JsonPath(&eventsRoot)["onSuccess"].as<json>(nullptr); onEventRoot != nullptr)
						addContentRoot["onSuccess"] = onEventRoot;
					if (json onEventRoot = JsonPath(&eventsRoot)["onError"].as<json>(nullptr); onEventRoot != nullptr)
						addContentRoot["onError"] = onEventRoot;
					if (json onEventRoot = JsonPath(&eventsRoot)["onComplete"].as<json>(nullptr); onEventRoot != nullptr)
						addContentRoot["onComplete"] = onEventRoot;
				}
			}
		}

		json addContentParametersRoot = ingestedParametersRoot;
		addContentParametersRoot.erase("internalMMS");
		addContentParametersRoot["fileFormat"] = fileFormat;
		if (!externalEncoder)
			addContentParametersRoot["sourceURL"] = std::format("move://{}{}",
				chunksNFSStagingContentsPath, currentRecordedAssetFileName);
		addContentParametersRoot["ingester"] = "Live Recorder Task";
		addContentParametersRoot["title"] = addContentTitle;
		addContentParametersRoot["userData"] = userDataRoot;
		addContentParametersRoot["uniqueName"] = uniqueName;
		addContentRoot["parameters"] = addContentParametersRoot;

		json workflowRoot;
		workflowRoot["label"] = addContentTitle;
		workflowRoot["type"] = "Workflow";
		{
			json variablesWorkflowRoot;

			{
				json variableWorkflowRoot;
				variableWorkflowRoot["type"] = "integer";
				variableWorkflowRoot["value"] = utcChunkStartTime;
				variablesWorkflowRoot["currentUtcChunkStartTime"] = variableWorkflowRoot;
			}

			// char currentUtcChunkStartTime_HHMISS[64];
			string currentUtcChunkStartTime_HHMISS;
			{
				tm tmDateTime;

				// from utc to local time
				localtime_r(&utcChunkStartTime, &tmDateTime);

				// sprintf(currentUtcChunkStartTime_HHMISS, "%02d:%02d:%02d", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec);
				currentUtcChunkStartTime_HHMISS = std::format("{:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec);
			}
			{
				json variableWorkflowRoot;
				variableWorkflowRoot["type"] = "string";
				variableWorkflowRoot["value"] = Datetime::dateTimeFormat(utcChunkStartTime * 1000, "%H:%M:%S");
				variablesWorkflowRoot["currentUtcChunkStartTime_HHMISS"] = variableWorkflowRoot;
				SPDLOG_INFO("AAAAAAAA"
					", currentUtcChunkStartTime_HHMISS: {}"
					", variableWorkflowRoot.value", currentUtcChunkStartTime_HHMISS, JsonPath(&variableWorkflowRoot)["value"].as<string>()
					);
			}

			{
				json variableWorkflowRoot;
				variableWorkflowRoot["type"] = "integer";
				variableWorkflowRoot["value"] = utcPreviousChunkStartTime;
				variablesWorkflowRoot["previousUtcChunkStartTime"] = variableWorkflowRoot;
			}

			{
				json variableWorkflowRoot;
				variableWorkflowRoot["type"] = "integer";
				variableWorkflowRoot["value"] = JsonPath(&ingestedParametersRoot)["recordingCode"].as<int64_t>(0);
				variablesWorkflowRoot["recordingCode"] = variableWorkflowRoot;
			}

			{
				json variableWorkflowRoot;
				variableWorkflowRoot["type"] = "string";
				variableWorkflowRoot["value"] = JsonPath(&encodingParametersRoot)["ingestionJobLabel"].as<string>("");
				variablesWorkflowRoot["ingestionJobLabel"] = variableWorkflowRoot;
			}

			workflowRoot["variables"] = variablesWorkflowRoot;
		}

		workflowRoot["task"] = addContentRoot;

		workflowMetadata = JSONUtils::toString(workflowRoot);

		SPDLOG_INFO(
			"Recording Workflow metadata generated"
			", ingestionJobKey: {}"
			", {}, {}, prev: {}, from: {}, to: {}",
			ingestionJobKey, addContentTitle, currentRecordedAssetFileName, utcPreviousChunkStartTime, utcChunkStartTime, utcChunkEndTime
		);

		return workflowMetadata;
	}
	catch (exception& e)
	{
		SPDLOG_ERROR(
			"buildRecordedMediaWorkflow failed"
			", ingestionJobKey: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, workflowMetadata, e.what()
		);

		throw;
	}
}

bool LiveRecorderDaemons::isLastLiveRecorderFile(
	int64_t ingestionJobKey, int64_t encodingJobKey, time_t utcCurrentRecordedFileCreationTime, string chunksTranscoderStagingContentsPath,
	string recordedFileNamePrefix, int segmentDurationInSeconds, bool isFirstChunk
)
{
	bool isLastLiveRecorderFile = true;

	try
	{
		SPDLOG_INFO(
			"isLastLiveRecorderFile"
			", chunksTranscoderStagingContentsPath: {}"
			", recordedFileNamePrefix: {}"
			", segmentDurationInSeconds: {}",
			chunksTranscoderStagingContentsPath, recordedFileNamePrefix, segmentDurationInSeconds
		);

		for (fs::directory_entry const &entry : fs::directory_iterator(chunksTranscoderStagingContentsPath))
		{
			try
			{
				SPDLOG_INFO(
					"readDirectory"
					", directoryEntry: {}",
					entry.path().string()
				);

				// next statement is endWith and .lck is used during the move of
				// a file
				if (entry.path().filename().string().ends_with(".lck"))
					continue;
				/*
				string suffix(".lck");
				if (entry.path().filename().string().size() >= suffix.size() &&
					0 == entry.path().filename().string().compare(
							 entry.path().filename().string().size() -
								 suffix.size(),
							 suffix.size(), suffix
						 ))
					continue;
					*/

				if (!entry.is_regular_file())
					continue;

				if (entry.path().filename().string().size() >= recordedFileNamePrefix.size() &&
					entry.path().filename().string().compare(0, recordedFileNamePrefix.size(), recordedFileNamePrefix) == 0)
				{
					time_t utcFileCreationTime = getMediaLiveRecorderStartTime(
						ingestionJobKey, encodingJobKey, entry.path().filename().string(), segmentDurationInSeconds, isFirstChunk
					);

					if (utcFileCreationTime > utcCurrentRecordedFileCreationTime)
					{
						isLastLiveRecorderFile = false;

						break;
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = std::format(
					"listing directory failed"
					", e.what(): {}",
					e.what()
				);
				SPDLOG_ERROR(errorMessage);

				throw e;
			}
			catch (exception &e)
			{
				string errorMessage = std::format(
					"listing directory failed"
					", e.what(): {}",
					e.what()
				);
				SPDLOG_ERROR(errorMessage);

				throw e;
			}
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"isLastLiveRecorderFile failed"
			", e.what(): {}",
			e.what()
		);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"isLastLiveRecorderFile failed"
			", e.what(): {}",
			e.what()
		);
	}

	return isLastLiveRecorderFile;
}

time_t LiveRecorderDaemons::getMediaLiveRecorderStartTime(
	int64_t ingestionJobKey, int64_t encodingJobKey, string mediaLiveRecorderFileName, int segmentDurationInSeconds, bool isFirstChunk
)
{
	// liveRecorder_6405_48749_2019-02-02_22-11-00_1100374273.ts
	// liveRecorder_<ingestionJobKey>_<FFMPEGEncoderBase::encodingJobKey>_YYYY-MM-DD_HH-MI-SS_<utc>.ts

	SPDLOG_INFO(
		"Received getMediaLiveRecorderStartTime"
		", ingestionJobKey: {}"
		", encodingJobKey: {}"
		", mediaLiveRecorderFileName: {}"
		", segmentDurationInSeconds: {}"
		", isFirstChunk: {}",
		ingestionJobKey, encodingJobKey, mediaLiveRecorderFileName, segmentDurationInSeconds, isFirstChunk
	);

	size_t endIndex = mediaLiveRecorderFileName.find_last_of(".");
	if (mediaLiveRecorderFileName.length() < 20 || endIndex == string::npos)
	{
		string errorMessage = std::format(
			"wrong media live recorder format"
			", mediaLiveRecorderFileName: {}",
			mediaLiveRecorderFileName
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	size_t beginUTCIndex = mediaLiveRecorderFileName.find_last_of("_");
	if (mediaLiveRecorderFileName.length() < 20 || beginUTCIndex == string::npos)
	{
		string errorMessage = std::format(
			"wrong media live recorder format"
			", mediaLiveRecorderFileName: {}",
			mediaLiveRecorderFileName
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	time_t utcMediaLiveRecorderStartTime = stol(mediaLiveRecorderFileName.substr(beginUTCIndex + 1, endIndex - beginUTCIndex + 1));

	{
		// in case of high bit rate (huge files) and server with high cpu usage,
		// sometime I saw seconds 1 instead of 0 For this reason,
		// utcMediaLiveRecorderStartTime is fixed. From the other side the first
		// generated file is the only one where we can have seconds different
		// from 0, anyway here this is not possible because we discard the first
		// chunk 2019-10-16: I saw as well seconds == 59, in this case we would
		// not do utcMediaLiveRecorderStartTime -= seconds
		//	as it is done below in the code but we should do
		// utcMediaLiveRecorderStartTime += 1.
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
				SPDLOG_WARN(
					"Wrong seconds (start time), force it to 0"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", mediaLiveRecorderFileName: {}"
					", seconds: {}",
					ingestionJobKey, encodingJobKey, mediaLiveRecorderFileName, seconds
				);
				utcMediaLiveRecorderStartTime -= seconds;
			}
			else if (seconds > halfSegmentDurationInSeconds && seconds < segmentDurationInSeconds)
			{
				SPDLOG_WARN(
					"Wrong seconds (start time), increase it"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", mediaLiveRecorderFileName: {}"
					", seconds: {}",
					ingestionJobKey, encodingJobKey, mediaLiveRecorderFileName, seconds
				);
				utcMediaLiveRecorderStartTime += (segmentDurationInSeconds - seconds);
			}
		}
	}

	return utcMediaLiveRecorderStartTime;
	/*
	tm                      tmDateTime;


	// liveRecorder_6405_2019-02-02_22-11-00.ts

	info(__FILEREF__ + "getMediaLiveRecorderStartTime"
		", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
	);

	size_t index = mediaLiveRecorderFileName.find_last_of(".");
	if (mediaLiveRecorderFileName.length() < 20 ||
		   index == string::npos)
	{
		string errorMessage = __FILEREF__ + "wrong media live recorder format"
			+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
			;
			SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	time_t utcMediaLiveRecorderStartTime;
	time (&utcMediaLiveRecorderStartTime);
	gmtime_r(&utcMediaLiveRecorderStartTime, &tmDateTime);

	tmDateTime.tm_year		= stoi(mediaLiveRecorderFileName.substr(index -
	19, 4))
		- 1900;
	tmDateTime.tm_mon		= stoi(mediaLiveRecorderFileName.substr(index -
	14, 2))
		- 1;
	tmDateTime.tm_mday		= stoi(mediaLiveRecorderFileName.substr(index -
	11, 2)); tmDateTime.tm_hour		=
	stoi(mediaLiveRecorderFileName.substr(index - 8, 2)); tmDateTime.tm_min =
	stoi(mediaLiveRecorderFileName.substr(index
	- 5, 2));

	// in case of high bit rate (huge files) and server with high cpu usage,
	sometime I saw seconds 1 instead of 0
	// For this reason, 0 is set.
	// From the other side the first generated file is the only one where we can
	have seconds
	// different from 0, anyway here this is not possible because we discard the
	first chunk int seconds = stoi(mediaLiveRecorderFileName.substr(index - 2,
	2)); if (seconds != 0)
	{
		warn(__FILEREF__ + "Wrong seconds (start time), force it to 0"
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

time_t LiveRecorderDaemons::getMediaLiveRecorderEndTime(int64_t ingestionJobKey, int64_t encodingJobKey, string mediaLiveRecorderFileName)
{
	tm tmDateTime;

	time_t utcCurrentRecordedFileLastModificationTime;
	{
		chrono::system_clock::time_point fileLastModification = chrono::time_point_cast<chrono::system_clock::duration>(
			fs::last_write_time(mediaLiveRecorderFileName) - fs::file_time_type::clock::now() + chrono::system_clock::now()
		);
		utcCurrentRecordedFileLastModificationTime = chrono::system_clock::to_time_t(fileLastModification);
	}

	// FileIO::getFileTime (mediaLiveRecorderFileName.c_str(),
	// 	&utcCurrentRecordedFileLastModificationTime);

	localtime_r(&utcCurrentRecordedFileLastModificationTime, &tmDateTime);

	// in case of high bit rate (huge files) and server with high cpu usage,
	// sometime I saw seconds 1 instead of 0 For this reason, 0 is set
	if (tmDateTime.tm_sec != 0)
	{
		SPDLOG_WARN(
			"Wrong seconds (end time), force it to 0"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", mediaLiveRecorderFileName: {}"
			", seconds: {}",
			ingestionJobKey, encodingJobKey, mediaLiveRecorderFileName, tmDateTime.tm_sec
		);
		tmDateTime.tm_sec = 0;
	}

	utcCurrentRecordedFileLastModificationTime = mktime(&tmDateTime);

	return utcCurrentRecordedFileLastModificationTime;
}

long LiveRecorderDaemons::buildAndIngestVirtualVOD(
	int64_t liveRecorderIngestionJobKey, int64_t liveRecorderEncodingJobKey, bool externalEncoder,

	string sourceSegmentsDirectoryPathName, string sourceManifestFileName,
	// /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/.../content
	string stagingLiveRecorderVirtualVODPathName,

	int64_t recordingCode, string liveRecorderIngestionJobLabel, string liveRecorderVirtualVODUniqueName, string liveRecorderVirtualVODRetention,
	int64_t liveRecorderVirtualVODImageMediaItemKey, int64_t liveRecorderUserKey, string liveRecorderApiKey, string mmsWorkflowIngestionURL,
	string mmsBinaryIngestionURL
)
{

	SPDLOG_INFO(
		"Received buildAndIngestVirtualVOD"
		", liveRecorderIngestionJobKey: {}"
		", liveRecorderEncodingJobKey: {}"
		", externalEncoder: {}"
		", sourceSegmentsDirectoryPathName: {}"
		", sourceManifestFileName: {}"
		", stagingLiveRecorderVirtualVODPathName: {}"
		", recordingCode: {}"
		", liveRecorderIngestionJobLabel: {}"
		", liveRecorderVirtualVODUniqueName: {}"
		", liveRecorderVirtualVODRetention: {}"
		", liveRecorderVirtualVODImageMediaItemKey: {}"
		", liveRecorderUserKey: {}"
		", liveRecorderApiKey: {}"
		", mmsWorkflowIngestionURL: {}"
		", mmsBinaryIngestionURL: {}",
		liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, externalEncoder, sourceSegmentsDirectoryPathName, sourceManifestFileName,
		stagingLiveRecorderVirtualVODPathName, recordingCode, liveRecorderIngestionJobLabel, liveRecorderVirtualVODUniqueName,
		liveRecorderVirtualVODRetention, liveRecorderVirtualVODImageMediaItemKey, liveRecorderUserKey, liveRecorderApiKey, mmsWorkflowIngestionURL,
		mmsBinaryIngestionURL
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
			// virtualVODM3u8DirectoryName =
			// to_string(liveRecorderIngestionJobKey)
			// 	+ "_liveRecorderVirtualVOD"
			// ;
			{
				size_t endOfPathIndex = stagingLiveRecorderVirtualVODPathName.find_last_of("/");
				if (endOfPathIndex == string::npos)
				{
					string errorMessage = std::format(
						"buildAndIngestVirtualVOD. No stagingLiveRecorderVirtualVODPathName found"
						", liveRecorderIngestionJobKey: {}"
						", liveRecorderEncodingJobKey: {}"
						", stagingLiveRecorderVirtualVODPathName: {}",
						liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, stagingLiveRecorderVirtualVODPathName
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				// stagingLiveRecorderVirtualVODPathName is initialized in
				// EncoderVideoAudioProxy.cpp and virtualVODM3u8DirectoryName
				// will be the name of the directory of the m3u8 of the Virtual
				// VOD In case of externalEncoder, since PUSH is used,
				// virtualVODM3u8DirectoryName has to be 'content' (see the
				// Add-Content Task documentation). For this reason, in
				// EncoderVideoAudioProxy.cpp, 'content' is used
				virtualVODM3u8DirectoryName = stagingLiveRecorderVirtualVODPathName.substr(endOfPathIndex + 1);
			}

			if (stagingLiveRecorderVirtualVODPathName != "" && fs::exists(stagingLiveRecorderVirtualVODPathName))
			{
				SPDLOG_INFO(
					"buildAndIngestVirtualVOD. Remove directory "
					", stagingLiveRecorderVirtualVODPathName: {}",
					stagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(stagingLiveRecorderVirtualVODPathName);
			}
		}

		string sourceManifestPathFileName = sourceSegmentsDirectoryPathName + "/" + sourceManifestFileName;
		if (!fs::exists(sourceManifestPathFileName.c_str()))
		{
			string errorMessage = std::format(
				"buildAndIngestVirtualVOD. manifest file not existing"
				", liveRecorderIngestionJobKey: {}"
				", liveRecorderEncodingJobKey: {}"
				", sourceManifestPathFileName: {}",
				liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, sourceManifestPathFileName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2021-05-30: it is not a good idea to copy all the directory (manifest
		// and ts files) because
		//	ffmpeg is not accurate to remove the obsolete ts files, so we
		// will have
		// the manifest files 	having for example 300 ts references but the
		// directory contains thousands of ts files. 	So we will copy only the
		// manifest file and ONLY the ts files referenced into the manifest file

		/*
		// copy manifest and TS files into the
		stagingLiveRecorderVirtualVODPathName
		{
			info(__FILEREF__ + "Coping directory"
				+ ", liveRecorderIngestionJobKey: " +
		to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " +
		to_string(liveRecorderEncodingJobKey)
				+ ", sourceSegmentsDirectoryPathName: " +
		sourceSegmentsDirectoryPathName
				+ ", stagingLiveRecorderVirtualVODPathName: " +
		stagingLiveRecorderVirtualVODPathName
			);

			chrono::system_clock::time_point startCoping =
		chrono::system_clock::now();
			fs::copyDirectory(sourceSegmentsDirectoryPathName,
		stagingLiveRecorderVirtualVODPathName, S_IRUSR | S_IWUSR | S_IXUSR |
				  S_IRGRP | S_IXGRP |
				  S_IROTH | S_IXOTH);
			chrono::system_clock::time_point endCoping =
		chrono::system_clock::now();

			info(__FILEREF__ + "Copied directory"
				+ ", liveRecorderIngestionJobKey: " +
		to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " +
		to_string(liveRecorderEncodingJobKey)
				+ ", @MMS COPY statistics@ - copingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endCoping
		- startCoping).count()) + "@"
			);
		}
		*/

		// 2022-05-26: non dovrebbe accadere ma, a volte, capita che il file ts
		// non esiste, perchè eliminato
		//	da ffmpeg, ma risiede ancora nel manifest. Per evitare quindi
		// che la
		// generazione del virtualVOD 	si blocchi, consideriamo come se il
		// manifest avesse solamente i segmenti successivi 	La copia quindi
		// del manifest originale viene fatta su un file temporaneo e gestiamo
		// noi il manifest
		//"definitivo"
		// 2022-05-27: Probabilmente era il crontab che rimuoveva i segmenti e
		// causava il problema
		//	descritto sopra. Per cui, fissato il retention del crontab,
		// mantenere
		// la playlist originale 	probabilmente va bene. Ormai lasciamo
		// cosi visto che funziona ed è piu robusto nel caso in cui 	un
		// segmento venisse eliminato

		string tmpManifestPathFileName = stagingLiveRecorderVirtualVODPathName + "/" + sourceManifestFileName + ".tmp";
		string destManifestPathFileName = stagingLiveRecorderVirtualVODPathName + "/" + sourceManifestFileName;

		// create the destination directory and copy the manifest file
		{
			if (!fs::exists(stagingLiveRecorderVirtualVODPathName))
			{
				SPDLOG_INFO(
					"buildAndIngestVirtualVOD. Creating directory"
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", stagingLiveRecorderVirtualVODPathName: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, stagingLiveRecorderVirtualVODPathName
				);
				fs::create_directories(stagingLiveRecorderVirtualVODPathName);
				fs::permissions(
					stagingLiveRecorderVirtualVODPathName,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
						fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace
				);
			}

			SPDLOG_INFO(
				"buildAndIngestVirtualVOD. Coping"
				", liveRecorderIngestionJobKey: {}"
				", liveRecorderEncodingJobKey: {}"
				", sourceManifestPathFileName: {}"
				", tmpManifestPathFileName: {}",
				liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, sourceManifestPathFileName, tmpManifestPathFileName
			);
			fs::copy(sourceManifestPathFileName, tmpManifestPathFileName);
		}

		if (!fs::exists(tmpManifestPathFileName.c_str()))
		{
			string errorMessage = std::format(
				"manifest file not existing"
				", liveRecorderIngestionJobKey: {}"
				", liveRecorderEncodingJobKey: {}"
				", tmpManifestPathFileName: {}",
				liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, tmpManifestPathFileName
			);
			SPDLOG_ERROR(errorMessage);

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
			SPDLOG_INFO(
				"buildAndIngestVirtualVOD. Reading copied manifest file"
				", liveRecorderIngestionJobKey: {}"
				", liveRecorderEncodingJobKey: {}"
				", tmpManifestPathFileName: {}",
				liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, tmpManifestPathFileName
			);

			ifstream ifManifestFile(tmpManifestPathFileName);
			if (!ifManifestFile.is_open())
			{
				string errorMessage = std::format(
					"Not authorized: manifest file not opened"
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", tmpManifestPathFileName: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, tmpManifestPathFileName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string firstPartOfManifest;
			string manifestLine;
			{
				while (getline(ifManifestFile, manifestLine))
				{
					// #EXTM3U
					// #EXT-X-VERSION:3
					// #EXT-X-TARGETDURATION:19
					// #EXT-X-MEDIA-SEQUENCE:0
					// #EXTINF:10.000000,
					// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
					// liveRecorder_760504_1653579715.ts
					// ...

					string extInfPrefix("#EXTINF:");
					string programDatePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
					if (manifestLine.size() >= extInfPrefix.size() && 0 == manifestLine.compare(0, extInfPrefix.size(), extInfPrefix))
					{
						break;
					}
					else if (manifestLine.size() >= programDatePrefix.size() &&
							 0 == manifestLine.compare(0, programDatePrefix.size(), programDatePrefix))
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

				SPDLOG_INFO(
					"buildAndIngestVirtualVOD. manifestLine"
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", manifestLine: {}"
					", segmentsNumber: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, manifestLine, segmentsNumber
				);

				string extInfPrefix("#EXTINF:");
				string programDatePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
				if (manifestLine.size() >= extInfPrefix.size() && 0 == manifestLine.compare(0, extInfPrefix.size(), extInfPrefix))
				{
					size_t endOfSegmentDuration = manifestLine.find(",");
					if (endOfSegmentDuration == string::npos)
					{
						string errorMessage = std::format(
							"wrong manifest line format"
							", liveRecorderIngestionJobKey: {}"
							", liveRecorderEncodingJobKey: {}"
							", manifestLine: {}",
							liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, manifestLine
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					lastSegmentDuration = stod(manifestLine.substr(extInfPrefix.size(), endOfSegmentDuration - extInfPrefix.size()));
				}
				else if (manifestLine.size() >= programDatePrefix.size() && 0 == manifestLine.compare(0, programDatePrefix.size(), programDatePrefix))
					lastSegmentUtcStartTimeInMillisecs = Datetime::sDateMilliSecondsToUtc(manifestLine.substr(programDatePrefix.size()));
				else if (manifestLine != "" && manifestLine[0] != '#')
				{
					string sourceTSPathFileName = sourceSegmentsDirectoryPathName + "/" + manifestLine;
					string copiedTSPathFileName = stagingLiveRecorderVirtualVODPathName + "/" + manifestLine;

					try
					{
						SPDLOG_INFO(
							"buildAndIngestVirtualVOD. Coping"
							", liveRecorderIngestionJobKey: {}"
							", liveRecorderEncodingJobKey: {}"
							", sourceTSPathFileName: {}"
							", copiedTSPathFileName: {}",
							liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, sourceTSPathFileName, copiedTSPathFileName
						);
						fs::copy(sourceTSPathFileName, copiedTSPathFileName);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"copyFile failed, previous segments of the manifest will be omitted"
							", sourceTSPathFileName: {}"
							", copiedTSPathFileName: {}"
							", liveRecorderIngestionJobKey: {}"
							", liveRecorderEncodingJobKey: {}"
							", segmentsNumber: {}"
							", e.what: {}",
							sourceTSPathFileName, copiedTSPathFileName, liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, segmentsNumber,
							e.what()
						);

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

				if (firstSegmentDuration == -1.0 && firstSegmentUtcStartTimeInMillisecs == -1 && lastSegmentDuration != -1.0 &&
					lastSegmentUtcStartTimeInMillisecs != -1)
				{
					firstSegmentDuration = lastSegmentDuration;
					firstSegmentUtcStartTimeInMillisecs = lastSegmentUtcStartTimeInMillisecs;
				}

				ofManifestFile << manifestLine << endl;
			} while (getline(ifManifestFile, manifestLine));
		}
		utcStartTimeInMilliSecs = firstSegmentUtcStartTimeInMillisecs;
		utcEndTimeInMilliSecs = lastSegmentUtcStartTimeInMillisecs + (lastSegmentDuration * 1000);

		// add end list to manifest file
		{
			SPDLOG_INFO(
				"buildAndIngestVirtualVOD. Add end manifest line to copied manifest file"
				", liveRecorderIngestionJobKey: {}"
				", liveRecorderEncodingJobKey: {}"
				", tmpManifestPathFileName: {}",
				liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, tmpManifestPathFileName
			);

			// string endLine = "\n";
			ofManifestFile << endl << "#EXT-X-ENDLIST" << endl;
			ofManifestFile.close();
		}

		if (segmentsNumber == 0)
		{
			string errorMessage = std::format(
				"No segments found"
				", liveRecorderIngestionJobKey: {}"
				", liveRecorderEncodingJobKey: {}"
				", sourceManifestPathFileName: {}"
				", destManifestPathFileName: {}",
				liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, sourceManifestPathFileName, destManifestPathFileName
			);
			SPDLOG_ERROR(errorMessage);

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
					string errorMessage = std::format(
						"No stagingLiveRecorderVirtualVODDirectory found"
						", liveRecorderIngestionJobKey: {}"
						", liveRecorderEncodingJobKey: {}"
						", stagingLiveRecorderVirtualVODPathName: {}",
						liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, stagingLiveRecorderVirtualVODPathName
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string stagingLiveRecorderVirtualVODDirectory = stagingLiveRecorderVirtualVODPathName.substr(0, endOfPathIndex);

				executeCommand = "tar cfz " + tarGzStagingLiveRecorderVirtualVODPathName + " -C " + stagingLiveRecorderVirtualVODDirectory + " " +
								 virtualVODM3u8DirectoryName;

				// sometimes tar return 1 as status and the command fails
				// because, according the tar man pages, "this exit code means
				// that some files were changed while being archived and
				//	so the resulting archive does not contain the exact copy
				// of the
				// file set"
				// I guess this is due because of the copy of the ts files among
				// different file systems For this reason I added this sleep
				long secondsToSleep = 3;
				SPDLOG_INFO(
					"buildAndIngestVirtualVOD. Start tar command "
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", executeCommand: {}"
					", secondsToSleep: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, executeCommand, secondsToSleep
				);
				this_thread::sleep_for(chrono::seconds(secondsToSleep));

				chrono::system_clock::time_point startTar = chrono::system_clock::now();
				int executeCommandStatus = ProcessUtility::execute(executeCommand);
				chrono::system_clock::time_point endTar = chrono::system_clock::now();
				SPDLOG_INFO(
					"buildAndIngestVirtualVOD. End tar command "
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", executeCommand: {}"
					", @MMS statistics@ - tarDuration (millisecs): @{}@",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, executeCommand,
					to_string(chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count())
				);
				if (executeCommandStatus != 0)
				{
					string errorMessage = std::format(
						"ProcessUtility::execute failed"
						", liveRecorderIngestionJobKey: {}"
						", liveRecorderEncodingJobKey: {}"
						", executeCommandStatus: {}"
						", executeCommand: {}",
						liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, executeCommandStatus, executeCommand
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				{
					SPDLOG_INFO(
						"buildAndIngestVirtualVOD. Remove directory"
						", liveRecorderIngestionJobKey: {}"
						", liveRecorderEncodingJobKey: {}"
						", stagingLiveRecorderVirtualVODPathName: {}",
						liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, stagingLiveRecorderVirtualVODPathName
					);
					fs::remove_all(stagingLiveRecorderVirtualVODPathName);
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = std::format(
					"tar command failed"
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", executeCommand: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, executeCommand
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (runtime_error &e)
	{
		string errorMessage = std::format(
			"build the live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", tarGzStagingLiveRecorderVirtualVODPathName: {}",
				tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		if (stagingLiveRecorderVirtualVODPathName != "" && fs::exists(stagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove directory"
				", stagingLiveRecorderVirtualVODPathName: {}",
				stagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(stagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"build the live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", tarGzStagingLiveRecorderVirtualVODPathName: {}",
				tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		if (stagingLiveRecorderVirtualVODPathName != "" && fs::exists(stagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove directory"
				", stagingLiveRecorderVirtualVODPathName: {}",
				stagingLiveRecorderVirtualVODPathName
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
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, externalEncoder,

			utcStartTimeInMilliSecs, utcEndTimeInMilliSecs, recordingCode, liveRecorderIngestionJobLabel, tarGzStagingLiveRecorderVirtualVODPathName,
			liveRecorderVirtualVODUniqueName, liveRecorderVirtualVODRetention, liveRecorderVirtualVODImageMediaItemKey
		);
	}
	catch (runtime_error e)
	{
		string errorMessage = std::format(
			"build workflowMetadata live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", tarGzStagingLiveRecorderVirtualVODPathName: {}",
				tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = std::format(
			"build workflowMetadata live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", tarGzStagingLiveRecorderVirtualVODPathName: {}",
				tarGzStagingLiveRecorderVirtualVODPathName
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
		string sResponse = CurlWrapper::httpPostString(
							   mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds,
							   CurlWrapper::basicAuthorization(to_string(liveRecorderUserKey), liveRecorderApiKey), workflowMetadata,
							   "application/json", // contentType
							   otherHeaders, std::format(", ingestionJobKey: {}", liveRecorderIngestionJobKey),
							   3 // maxRetryNumber
		)
							   .second;

		if (externalEncoder)
		{
			addContentIngestionJobKey = getAddContentIngestionJobKey(liveRecorderIngestionJobKey, sResponse);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = std::format(
			"ingest live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", mmsWorkflowIngestionURL: {}"
			", workflowMetadata: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, mmsWorkflowIngestionURL, workflowMetadata, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", tarGzStagingLiveRecorderVirtualVODPathName: {}",
				tarGzStagingLiveRecorderVirtualVODPathName
			);
			fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = std::format(
			"ingest live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", mmsWorkflowIngestionURL: {}"
			", workflowMetadata: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, mmsWorkflowIngestionURL, workflowMetadata, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", tarGzStagingLiveRecorderVirtualVODPathName: {}",
				tarGzStagingLiveRecorderVirtualVODPathName
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
				string errorMessage = std::format(
					"Ingested URL failed, addContentIngestionJobKey is not valid"
					", liveRecorderIngestionJobKey: {}",
					liveRecorderIngestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

#ifdef SAFEFILESYSTEMTHREAD
			int64_t chunkFileSize = SafeFileSystem::fileSizeThread(
				tarGzStagingLiveRecorderVirtualVODPathName, 10, std::format(", ingestionJobKey: {}", liveRecorderIngestionJobKey)
			);
#elif SAFEFILESYSTEMPROCESS
			int64_t chunkFileSize = SafeFileSystem::fileSizeProcess(
				tarGzStagingLiveRecorderVirtualVODPathName, 10, std::format(", ingestionJobKey: {}", liveRecorderIngestionJobKey)
			);
#else
			int64_t chunkFileSize = fs::file_size(tarGzStagingLiveRecorderVirtualVODPathName);
#endif

			mmsBinaryURL = mmsBinaryIngestionURL + "/" + to_string(addContentIngestionJobKey);

			string sResponse = CurlWrapper::httpPostFileSplittingInChunks(
				mmsBinaryURL, _mmsBinaryTimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(liveRecorderUserKey), liveRecorderApiKey),
				tarGzStagingLiveRecorderVirtualVODPathName, [](int, int) { return true; },
				std::format(", ingestionJobKey: {}", liveRecorderIngestionJobKey),
				3 // maxRetryNumber
			);

			{
				SPDLOG_INFO(
					"Remove"
					", tarGzStagingLiveRecorderVirtualVODPathName: {}",
					tarGzStagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
			}
		}
		catch (runtime_error e)
		{
			SPDLOG_ERROR(
				"Ingestion binary failed"
				", liveRecorderIngestionJobKey: {}"
				", mmsBinaryURL: {}"
				", workflowMetadata: {}"
				", exception: {}",
				liveRecorderIngestionJobKey, mmsBinaryURL, workflowMetadata, e.what()
			);

			if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
			{
				SPDLOG_INFO(
					"Remove"
					", tarGzStagingLiveRecorderVirtualVODPathName: {}",
					tarGzStagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
			}

			throw e;
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"Ingestion binary failed"
				", liveRecorderIngestionJobKey: {}"
				", mmsBinaryURL: {}"
				", workflowMetadata: {}"
				", exception: {}",
				liveRecorderIngestionJobKey, mmsBinaryURL, workflowMetadata, e.what()
			);

			if (tarGzStagingLiveRecorderVirtualVODPathName != "" && fs::exists(tarGzStagingLiveRecorderVirtualVODPathName))
			{
				SPDLOG_INFO(
					"Remove"
					", tarGzStagingLiveRecorderVirtualVODPathName: {}",
					tarGzStagingLiveRecorderVirtualVODPathName
				);
				fs::remove_all(tarGzStagingLiveRecorderVirtualVODPathName);
			}

			throw e;
		}
	}

	return segmentsNumber;
}

string LiveRecorderDaemons::buildVirtualVODIngestionWorkflow(
	int64_t liveRecorderIngestionJobKey, int64_t liveRecorderEncodingJobKey, bool externalEncoder,

	int64_t utcStartTimeInMilliSecs, int64_t utcEndTimeInMilliSecs, int64_t recordingCode, string liveRecorderIngestionJobLabel,
	string tarGzStagingLiveRecorderVirtualVODPathName, string liveRecorderVirtualVODUniqueName, string liveRecorderVirtualVODRetention,
	int64_t liveRecorderVirtualVODImageMediaItemKey
)
{
	string workflowMetadata;

	try
	{
		// {
		// 	"label": "<workflow label>",
		// 	"type": "Workflow",
		//	"task": {
		//        "label": "<task label 1>",
		//        "type": "Add-Content"
		//        "parameters": {
		//                "fileFormat": "m3u8",
		//                "ingester": "Giuliano",
		//                "sourceURL": "move:///abc...."
		//        },
		//	}
		// }
		json mmsDataRoot;

		// 2020-04-28: set it to liveRecordingChunk to avoid to be visible into
		// the GUI (view MediaItems).
		//	This is because this MediaItem is not completed yet
		json liveRecordingVODRoot;

		// utcStartTimeInMilliSecs is used by DB generated column
		string field = "utcStartTimeInMilliSecs";
		mmsDataRoot[field] = utcStartTimeInMilliSecs;

		// utcEndTimeInMilliSecs is used by DB generated column
		field = "utcEndTimeInMilliSecs";
		mmsDataRoot[field] = utcEndTimeInMilliSecs;

		{
			time_t utcEndTimeInSeconds = utcEndTimeInMilliSecs / 1000;
			// i.e.: 2021-02-26T15:41:15Z
			string utcToUtcString = Datetime::utcToUtcString(utcEndTimeInSeconds);
			utcToUtcString.insert(utcToUtcString.size() - 1, "." + to_string(utcEndTimeInMilliSecs % 1000));

			field = "utcEndTimeInMilliSecs_str";
			mmsDataRoot[field] = utcToUtcString;
		}

		// recordingCode is used by DB generated column
		field = "recordingCode";
		liveRecordingVODRoot[field] = recordingCode;

		field = "liveRecordingVOD";
		mmsDataRoot[field] = liveRecordingVODRoot;

		json userDataRoot;

		field = "mmsData";
		userDataRoot[field] = mmsDataRoot;

		json addContentRoot;

		string addContentLabel = liveRecorderIngestionJobLabel;

		field = "label";
		addContentRoot[field] = addContentLabel;

		field = "type";
		addContentRoot[field] = "Add-Content";

		json addContentParametersRoot;

		field = "fileFormat";
		addContentParametersRoot[field] = "m3u8-tar.gz";

		if (!externalEncoder)
		{
			// 2021-05-30: changed from copy to move with the idea to have
			// better performance
			string sourceURL = string("move") + "://" + tarGzStagingLiveRecorderVirtualVODPathName;
			field = "sourceURL";
			addContentParametersRoot[field] = sourceURL;
		}

		field = "ingester";
		addContentParametersRoot[field] = "Live Recorder Task";

		field = "title";
		addContentParametersRoot[field] = "Virtual VOD: " + addContentLabel;

		field = "uniqueName";
		addContentParametersRoot[field] = liveRecorderVirtualVODUniqueName;

		field = "allowUniqueNameOverride";
		addContentParametersRoot[field] = true;

		field = "retention";
		addContentParametersRoot[field] = liveRecorderVirtualVODRetention;

		field = "userData";
		addContentParametersRoot[field] = userDataRoot;

		if (liveRecorderVirtualVODImageMediaItemKey != -1)
		{
			try
			{
				/*
				bool warningIfMissing = true;
				pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemDetails =
					_mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					workspace->_workspaceKey, liveRecorderVirtualVODImageLabel,
				warningIfMissing);

				int64_t liveRecorderVirtualVODImageMediaItemKey;
				tie(liveRecorderVirtualVODImageMediaItemKey, ignore) =
				mediaItemDetails;
				*/

				json crossReferencesRoot = json::array();
				{
					json crossReferenceRoot;

					field = "type";
					crossReferenceRoot[field] = "VideoOfImage";

					field = "mediaItemKey";
					crossReferenceRoot[field] = liveRecorderVirtualVODImageMediaItemKey;

					crossReferencesRoot.push_back(crossReferenceRoot);
				}

				field = "crossReferences";
				addContentParametersRoot[field] = crossReferencesRoot;
			}
			catch (MediaItemKeyNotFound e)
			{
				SPDLOG_ERROR(
					"getMediaItemKeyDetailsByUniqueName failed"
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", liveRecorderVirtualVODImageMediaItemKey: {}"
					", e.what: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, liveRecorderVirtualVODImageMediaItemKey, e.what()
				);
			}
			catch (runtime_error e)
			{
				SPDLOG_ERROR(
					"getMediaItemKeyDetailsByUniqueName failed"
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", liveRecorderVirtualVODImageMediaItemKey: {}"
					", e.what: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, liveRecorderVirtualVODImageMediaItemKey, e.what()
				);
			}
			catch (exception e)
			{
				SPDLOG_ERROR(
					"getMediaItemKeyDetailsByUniqueName failed"
					", liveRecorderIngestionJobKey: {}"
					", liveRecorderEncodingJobKey: {}"
					", liveRecorderVirtualVODImageMediaItemKey: {}"
					", e.what: {}",
					liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, liveRecorderVirtualVODImageMediaItemKey, e.what()
				);
			}
		}

		field = "parameters";
		addContentRoot[field] = addContentParametersRoot;

		json workflowRoot;

		field = "label";
		workflowRoot[field] = addContentLabel + " (virtual VOD)";

		field = "type";
		workflowRoot[field] = "Workflow";

		field = "task";
		workflowRoot[field] = addContentRoot;

		{
			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		SPDLOG_INFO(
			"Live Recorder VOD Workflow metadata generated"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, addContentLabel
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		string errorMessage = std::format(
			"build workflowMetadata live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = std::format(
			"build workflowMetadata live recorder VOD failed"
			", liveRecorderIngestionJobKey: {}"
			", liveRecorderEncodingJobKey: {}"
			", e.what: {}",
			liveRecorderIngestionJobKey, liveRecorderEncodingJobKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}
