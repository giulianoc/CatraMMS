
#include "FFMpegWrapper.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "ProcessUtility.h"

using namespace std;
using json = nlohmann::json;

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
		LOG_INFO("generateAndIngestFrameThread"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", _processorsThreadsNumber.use_count(): {}", _processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
		);

		string field;

		if (dependencies.empty())
		{
			string errorMessage = std::format("No dependencies found"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", dependencies.size: {}", _processorIdentifier, ingestionJobKey, dependencies.size());
			_logger->warn(errorMessage);

			LOG_INFO("Update IngestionJob"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", IngestionStatus: End_TaskSuccess"
				", errorMessage: ", _processorIdentifier, ingestionJobKey
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
					"" // errorMessage
				);
			}
			catch (exception &e)
			{
				LOG_ERROR("Update IngestionJob failed"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", errorMessage: {}", _processorIdentifier, ingestionJobKey, e.what()
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
					string errorMessage = std::format("ContentTpe is not a Video"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}", _processorIdentifier, ingestionJobKey);
					LOG_ERROR(errorMessage);

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

				ProcessUtility::ProcessId childProcessId;
				FFMpegWrapper ffmpeg(_configurationRoot);
				ffmpeg.generateFrameToIngest(
					ingestionJobKey, sourcePhysicalPath, durationInMilliSeconds, startTimeInSeconds, frameAssetPathName, imageWidth, imageHeight,
					childProcessId, nullptr
				);

				{
					LOG_INFO("Generated Frame to ingest"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", frameAssetPathName: {}"
						", fileFormat: {}", _processorIdentifier, ingestionJobKey, frameAssetPathName, fileFormat
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
						auto localAssetIngestionEvent = make_shared<LocalAssetIngestionEvent>();

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
					catch (exception &e)
					{
						LOG_ERROR("handleLocalAssetIngestionEvent failed"
							", _processorIdentifier: {}"
							", exception: {}", _processorIdentifier, e.what()
						);

						{
							LOG_INFO("Remove file"
								", _processorIdentifier: {}"
								", ingestionJobKey: {}"
								", frameAssetPathName: {}", _processorIdentifier, ingestionJobKey, frameAssetPathName
							);
							fs::remove_all(frameAssetPathName);
						}

						throw;
					}
				}
			}
			catch (exception& e)
			{
				string errorMessage = std::format(
					"generate and ingest frame failed"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencyIndex: {}"
					", dependencies.size(): {}"
					", e.what(): {}",
					_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size(), e.what()
				);
				LOG_ERROR(errorMessage);

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
	catch (exception &e)
	{
		LOG_ERROR("generateAndIngestFrame failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}", _processorIdentifier, ingestionJobKey, e.what()
		);

		LOG_INFO("Update IngestionJob"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", IngestionStatus: End_IngestionFailure"
			", errorMessage: {}", _processorIdentifier, ingestionJobKey, e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
				e.what());
		}
		catch (exception &e)
		{
			LOG_INFO("Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", errorMessage: {}", _processorIdentifier, ingestionJobKey, e.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
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
			string errorMessage = std::format("Wrong number of dependencies"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", dependencies.size: {}", _processorIdentifier, ingestionJobKey, dependencies.size());
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::as<string>(parametersRoot, field, ""));
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
			string errorMessage = std::format("ContentTpe is not a Video"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}", _processorIdentifier, ingestionJobKey);
			LOG_ERROR(errorMessage);

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
			maxFramesNumber, videoFilter, periodInSeconds, mjpeg, imageWidth, imageHeight
		);
	}
	catch (exception &e)
	{
		LOG_ERROR("manageGenerateFramesTask failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}", _processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw;
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
				if (!JSONUtils::isPresent(parametersRoot, field))
				{
					string errorMessage = std::format("Field is not present or it is null"
						", _processorIdentifier: {}"
						", Field: {}", _processorIdentifier, field);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				periodInSeconds = JSONUtils::as<int32_t>(parametersRoot, field, 0);
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
				startTimeInSeconds = JSONUtils::as<double>(parametersRoot, field, 0);
			}
			else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::IFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
					 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
			{
				field = "startTimeInSeconds";
				startTimeInSeconds = JSONUtils::as<double>(parametersRoot, field, 0);
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
				maxFramesNumber = JSONUtils::as<int32_t>(parametersRoot, field, -1);
			}
		}

		// 2021-09-14: default is set to true because often we have the error
		//	endTimeInSeconds is bigger of few milliseconds of the duration of
		// the media 	For this reason this field is set to true by default
		bool fixStartTimeIfOvercomeDuration = true;
		if (JSONUtils::isPresent(parametersRoot, "fixInstantInSecondsIfOvercomeDuration"))
			fixStartTimeIfOvercomeDuration = JSONUtils::as<bool>(parametersRoot, "fixInstantInSecondsIfOvercomeDuration", true);
		else if (JSONUtils::isPresent(parametersRoot, "FixStartTimeIfOvercomeDuration"))
			fixStartTimeIfOvercomeDuration = JSONUtils::as<bool>(parametersRoot, "FixStartTimeIfOvercomeDuration", true);

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
			width = JSONUtils::as<int32_t>(parametersRoot, field, -1);
		}

		int height = -1;
		{
			field = "height";
			height = JSONUtils::as<int32_t>(parametersRoot, field, -1);
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
			if (videoTracks.empty())
			{
				string errorMessage = std::format("No video track are present"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}", _processorIdentifier, ingestionJobKey);

				LOG_ERROR(errorMessage);

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
		catch (exception &e)
		{
			string errorMessage = std::format("_mmsEngineDBFacade->getVideoDetails failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", e.what(): {}", _processorIdentifier, ingestionJobKey, e.what());
			LOG_ERROR(errorMessage);

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

				LOG_INFO("startTimeInSeconds was changed to durationInMilliSeconds"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", fixStartTimeIfOvercomeDuration: {}"
					", previousStartTimeInSeconds: {}"
					", new startTimeInSeconds: {}", _processorIdentifier, ingestionJobKey, fixStartTimeIfOvercomeDuration,
					previousStartTimeInSeconds, startTimeInSeconds
				);
			}
			else
			{
				string errorMessage = std::format("Frame was not generated because instantInSeconds is "
					"bigger than the video duration"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", video sourceMediaItemKey: {}"
					", startTimeInSeconds: {}"
					", durationInMilliSeconds: {}", _processorIdentifier, ingestionJobKey, sourceMediaItemKey,
					startTimeInSeconds, durationInMilliSeconds);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception &e)
	{
		LOG_ERROR("fillGenerateFramesParameters failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}", _processorIdentifier, ingestionJobKey, e.what()
		);

		throw;
	}
}
