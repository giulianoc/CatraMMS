
#include "Datetime.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "StringUtils.h"

using namespace std;
using json = nlohmann::json;

void MMSEngineProcessor::manageLiveRecorder(
	int64_t ingestionJobKey, const string& ingestionJobLabel, MMSEngineDBFacade::IngestionStatus ingestionStatus,
	const shared_ptr<Workspace>& workspace, json parametersRoot
)
{
	try
	{
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isPresent(parametersRoot, field))
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		else
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::as<string>(parametersRoot, field, ""));

		string configurationLabel;

		int64_t confKey = -1;
		string streamSourceType;
		string encodersPoolLabel;
		string pullUrl;
		int64_t pushEncoderKey = -1;
		int pushListenTimeout = -1;
		int captureVideoDeviceNumber = -1;
		string captureVideoInputFormat;
		int captureFrameRate = -1;
		int captureWidth = -1;
		int captureHeight = -1;
		int captureAudioDeviceNumber = -1;
		int captureChannelsNumber = -1;
		int64_t tvSourceTVConfKey = -1;

		string recordingPeriodStart;
		string recordingPeriodEnd;

		bool liveRecorderVirtualVOD = false;
		string virtualVODHlsChannelConfigurationLabel;
		int64_t virtualVODEncodingProfileKey = -1;

		bool utcTimeOverlay;

		bool monitorHLS = false;
		string monitorHlsChannelConfigurationLabel;
		int64_t monitorEncodingProfileKey = -1;

		json outputsRoot = nullptr;
		json framesToBeDetectedRoot = nullptr;
		{
			bool pushPublicEncoderName = false;
			{
				field = "configurationLabel";
				if (!JSONUtils::isPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				configurationLabel = JSONUtils::as<string>(parametersRoot, field, "");

				{
					tie(confKey, streamSourceType, encodersPoolLabel, pullUrl, pushEncoderKey, pushPublicEncoderName, pushListenTimeout,
						captureVideoDeviceNumber, captureVideoInputFormat, captureFrameRate, captureWidth, captureHeight, captureAudioDeviceNumber,
						captureChannelsNumber, tvSourceTVConfKey) = _mmsEngineDBFacade->stream_aLot(workspace->_workspaceKey, configurationLabel);

					// default is IP_PULL
					if (streamSourceType.empty())
						streamSourceType = "IP_PULL";
				}
			}

			// EncodersPool override the one included in ChannelConf if present
			field = "encodersPool";
			encodersPoolLabel = JSONUtils::as<string>(parametersRoot, field, encodersPoolLabel);

			utcTimeOverlay = JSONUtils::as<bool>(parametersRoot, "utcTimeOverlay", false);

			// aggiungiomo 'encodersDetails' in ingestion parameters. In questo oggetto json mettiamo
			// l'encodersPool o l'encoderKey in caso di IP_PUSH che viene realmente utilizzato dall'MMS (MMSEngine::EncoderProxy).
			// In questo modo è possibile cambiare tramite API l'encoder per fare uno switch di un ingestionJob da un encoder ad un'altro
			{
				json encodersDetailsRoot;

				if (streamSourceType == "IP_PUSH")
				{
					encodersDetailsRoot["pushEncoderKey"] = pushEncoderKey;
					encodersDetailsRoot["pushPublicEncoderName"] = pushPublicEncoderName;
				}
				else
					encodersDetailsRoot["encodersPoolLabel"] = encodersPoolLabel;

				if (JSONUtils::isPresent(parametersRoot, "internalMMS"))
					parametersRoot["internalMMS"]["encodersDetails"] = encodersDetailsRoot;
				else
				{
					json internalMMSRoot;
					internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
					parametersRoot["internalMMS"] = internalMMSRoot;
				}

				_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(parametersRoot));
			}

			field = "schedule";
			const json& recordingPeriodRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isPresent(recordingPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingPeriodStart = JSONUtils::as<string>(recordingPeriodRoot, field, "");

			field = "end";
			if (!JSONUtils::isPresent(recordingPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingPeriodEnd = JSONUtils::as<string>(recordingPeriodRoot, field, "");

			field = "monitorHLS";
			if (JSONUtils::isPresent(parametersRoot, field))
			{
				const json& monitorHLSRoot = parametersRoot[field];

				monitorHLS = true;

				field = "hlsChannelConfigurationLabel";
				monitorHlsChannelConfigurationLabel = JSONUtils::as<string>(monitorHLSRoot, field, "");

				// field = "playlistEntriesNumber";
				// monitorPlaylistEntriesNumber =
				// JSONUtils::as<int32_t>(monitorHLSRoot, field, 6);

				// field = "segmentDurationInSeconds";
				// monitorSegmentDurationInSeconds =
				// JSONUtils::as<int32_t>(monitorHLSRoot, field, 10);

				string keyField = "encodingProfileKey";
				string labelField = "encodingProfileLabel";
				string contentTypeField = "contentType";
				if (JSONUtils::isPresent(monitorHLSRoot, keyField))
					monitorEncodingProfileKey = JSONUtils::as<int64_t>(monitorHLSRoot, keyField, 0);
				else if (JSONUtils::isPresent(monitorHLSRoot, labelField))
				{
					string encodingProfileLabel = JSONUtils::as<string>(monitorHLSRoot, labelField, "");

					MMSEngineDBFacade::ContentType contentType;
					if (JSONUtils::isPresent(monitorHLSRoot, contentTypeField))
					{
						contentType = MMSEngineDBFacade::toContentType(JSONUtils::as<string>(monitorHLSRoot, contentTypeField, ""));

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
			if (JSONUtils::isPresent(parametersRoot, field))
			{
				const json& virtualVODRoot = parametersRoot[field];

				liveRecorderVirtualVOD = true;

				field = "hlsChannelConfigurationLabel";
				virtualVODHlsChannelConfigurationLabel = JSONUtils::as<string>(virtualVODRoot, field, "");

				// field = "segmentDurationInSeconds";
				// virtualVODSegmentDurationInSeconds =
				// JSONUtils::as<int32_t>(virtualVODRoot, field, 10);

				string keyField = "encodingProfileKey";
				string labelField = "encodingProfileLabel";
				string contentTypeField = "contentType";
				if (JSONUtils::isPresent(virtualVODRoot, keyField))
					virtualVODEncodingProfileKey = JSONUtils::as<int64_t>(virtualVODRoot, keyField, 0);
				else if (JSONUtils::isPresent(virtualVODRoot, labelField))
				{
					string encodingProfileLabel = JSONUtils::as<string>(virtualVODRoot, labelField, "");

					MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::ContentType::Video;
					if (JSONUtils::isPresent(virtualVODRoot, contentTypeField))
					{
						contentType = MMSEngineDBFacade::toContentType(JSONUtils::as<string>(virtualVODRoot, contentTypeField, ""));

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

			outputsRoot = JSONUtils::as<json>(parametersRoot, "outputs", json::array());

			if (JSONUtils::isPresent(parametersRoot, "framesToBeDetected"))
			{
				framesToBeDetectedRoot = parametersRoot["framesToBeDetected"];

				for (int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size(); pictureIndex++)
				{
					json frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

					if (JSONUtils::isPresent(frameToBeDetectedRoot, "picturePhysicalPathKey"))
					{
						int64_t physicalPathKey = JSONUtils::as<int64_t>(frameToBeDetectedRoot, "picturePhysicalPathKey", -1);
						string picturePathName;

						tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
							physicalPathKey,
							// 2022-12-18: MIK potrebbe essere stato
							// appena aggiunto
							true
						);
						tie(picturePathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

						bool videoFrameToBeCropped = JSONUtils::as<bool>(frameToBeDetectedRoot, "videoFrameToBeCropped", false);
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

		time_t utcRecordingPeriodStart = Datetime::parseUtcStringToUtcInSecs(recordingPeriodStart);
		// LOG_ERROR(string() + "ctime recordingPeriodStart: "
		//		+ ctime(utcRecordingPeriodStart));

		// next code is the same in the Validator class
		time_t utcRecordingPeriodEnd = Datetime::parseUtcStringToUtcInSecs(recordingPeriodEnd);

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
			if (liveURL.starts_with("https://www.youtube.com/") || liveURL.starts_with("https://youtu.be/"))
			{
				liveURL = _mmsEngineDBFacade->getStreamingYouTubeLiveURL(workspace, ingestionJobKey, confKey, liveURL);
			}
		}
		else if (streamSourceType == "IP_PUSH")
		{
			tie(pushEncoderKey, liveURL) =
				_mmsEngineDBFacade->getStreamInputPushDetails(workspace->_workspaceKey, ingestionJobKey, configurationLabel);
			// liveURL = pushProtocol + "://" + pushEncoderName + ":" + to_string(pushServerPort) + pushUri;
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
				LOG_ERROR(errorMessage);

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
				if (!virtualVODHlsChannelConfigurationLabel.empty())
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

				encodingProfileDetailsRoot = JSONUtils::toJson<json>(jsonEncodingProfile);
			}

			json localOutputRoot;

			field = "outputType";
			localOutputRoot[field] = string("HLS_Channel");

			field = "hlsChannelConfigurationLabel";
			localOutputRoot[field] = monitorVirtualVODHlsChannelConfigurationLabel;

			localOutputRoot["filters"] = nullptr;

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
		}

		if (utcTimeOverlay)
		{
			for (auto& outputRoot: outputsRoot)
			{
				json filtersRoot = JSONUtils::as<json>(outputRoot, "filters", json::object());
				json videoFiltersRoot = JSONUtils::as<json>(filtersRoot, "video", json::array());

				videoFiltersRoot.push_back(FFMpegFilters::createTimecodeDrawTextFilter());
				filtersRoot["video"] = videoFiltersRoot;
				outputRoot["filters"] = filtersRoot;
			}
		}

		if (monitorHLS || liveRecorderVirtualVOD || utcTimeOverlay)
		{
			// in questo scenario outputs è cambiato, per cui aggiorniamo il DB
			parametersRoot["outputs"] = outputsRoot;
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
			size_t directoryEndIndex = stagingLiveRecordingAssetPathName.find_last_of('/');
			if (directoryEndIndex == string::npos)
			{
				string errorMessage = string() + "No directory found in the staging asset path name" +
									  ", _ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", stagingLiveRecordingAssetPathName: " + stagingLiveRecordingAssetPathName;
				LOG_ERROR(errorMessage);

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
			size_t directoryEndIndex = stagingLiveRecordingAssetPathName.find_last_of('/');
			if (directoryEndIndex == string::npos)
			{
				string errorMessage = string() + "No directory found in the staging asset path name" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", stagingLiveRecordingAssetPathName: " + stagingLiveRecordingAssetPathName;
				LOG_ERROR(errorMessage);

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
		if (liveRecorderVirtualVOD && !_liveRecorderVirtualVODImageLabel.empty())
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
			catch (MediaItemKeyNotFound& e)
			{
				_logger->warn(
					string() + "No associated VirtualVODImage to the Workspace" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _liveRecorderVirtualVODImageLabel: " + _liveRecorderVirtualVODImageLabel + ", exception: " + e.what()
				);

				liveRecorderVirtualVODImageMediaItemKey = -1;
			}
			catch (exception& e)
			{
				LOG_ERROR(
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

			pushListenTimeout, pushEncoderKey, captureRoot,

			tvRoot,

			monitorHLS, liveRecorderVirtualVOD, monitorVirtualVODOutputRootIndex,

			localOutputsRoot, framesToBeDetectedRoot,

			chunksTranscoderStagingContentsPath, chunksNFSStagingContentsPath, segmentListFileName, recordedFileNamePrefix,
			virtualVODStagingContentsPath, virtualVODTranscoderStagingContentsPath, liveRecorderVirtualVODImageMediaItemKey
		);
	}
	catch (DBRecordNotFound &e)
	{
		LOG_ERROR(
			"manageLiveRecorder failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (runtime_error &e)
	{
		LOG_ERROR(
			"manageLiveRecorder failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"manageLiveRecorder failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}",
			_processorIdentifier, ingestionJobKey
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
