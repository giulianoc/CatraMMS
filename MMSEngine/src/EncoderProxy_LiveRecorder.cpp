/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   EnodingsManager.cpp
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#include "EncoderProxy.h"
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSDeliveryAuthorization.h"
#include "catralibraries/DateTime.h"
#include "spdlog/spdlog.h"

bool EncoderProxy::liveRecorder()
{
	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	{
		string field = "schedule";
		json recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)[field];

		field = "start";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

		field = "autoRenew";
		autoRenew = JSONUtils::asBool(recordingPeriodRoot, field, false);

		string segmenterType = "hlsSegmenter";
		// string segmenterType = "streamSegmenter";
		if (segmenterType == "streamSegmenter")
		{
			// since the first chunk is discarded, we will start recording
			// before the period of the chunk 2021-07-09: commented because we
			// do not have monitorVirtualVODSegmentDurationInSeconds anymore
			//	(since it is inside outputsRoot)
			// utcRecordingPeriodStart -=
			// monitorVirtualVODSegmentDurationInSeconds;
		}
	}

	time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

	// MMS allocates a thread just 5 minutes before the beginning of the
	// recording
	if (utcNow < utcRecordingPeriodStart)
	{
		if (utcRecordingPeriodStart - utcNow >= _timeBeforeToPrepareResourcesInMinutes * 60)
		{
			SPDLOG_INFO(
				"Too early to allocate a thread for recording"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", utcRecordingPeriodStart - utcNow: {}"
				", _timeBeforeToPrepareResourcesInSeconds: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcRecordingPeriodStart - utcNow,
				_timeBeforeToPrepareResourcesInMinutes * 60
			);

			// it is simulated a MaxConcurrentJobsReached to avoid to increase
			// the error counter
			throw MaxConcurrentJobsReached();
		}
	}

	if (!autoRenew)
	{
		if (utcRecordingPeriodEnd <= utcNow)
		{
			string errorMessage = std::format(
				"Too late to activate the recording"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", utcRecordingPeriodEnd: {}"
				", utcNow: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcRecordingPeriodEnd, utcNow
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	{
		string field = "outputsRoot";
		json outputsRoot = (_encodingItem->_encodingParametersRoot)[field];

		bool killedByUser = false;
		try
		{
			field = "monitorVirtualVODOutputRootIndex";
			int monitorVirtualVODOutputRootIndex = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, -1);

			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "CDN_AWS")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string awsChannelConfigurationLabel = JSONUtils::asString(outputRoot, "awsChannelConfigurationLabel", "");
					// bool awsSignedURL = JSONUtils::asBool(outputRoot, "awsSignedURL", false);
					// int awsExpirationInMinutes = JSONUtils::asInt(outputRoot, "awsExpirationInMinutes", 1440);

					/*
					string awsChannelType;
					if (awsChannelConfigurationLabel == "")
						awsChannelType = "SHARED";
					else
						awsChannelType = "DEDICATED";
					*/

					// reserveAWSChannel ritorna exception se non ci sono piu
					// canali liberi o quello dedicato è già occupato In caso di
					// ripartenza di mmsEngine, nel caso di richiesta già
					// attiva, ritornerebbe le stesse info associate a
					// ingestionJobKey (senza exception)
					auto [awsChannelId, rtmpURL, playURL, channelAlreadyReserved] = _mmsEngineDBFacade->reserveAWSChannel(
						_encodingItem->_workspace->_workspaceKey, awsChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					/*
					string awsChannelId;
					string rtmpURL;
					string playURL;
					bool channelAlreadyReserved;
					tie(awsChannelId, rtmpURL, playURL, channelAlreadyReserved) = awsChannelDetails;
					*/

					/*
					if (awsSignedURL)
					{
						try
						{
							MMSDeliveryAuthorization mmsDeliveryAuthorization(_configuration, _mmsStorage, _mmsEngineDBFacade);
							playURL = mmsDeliveryAuthorization.getAWSSignedURL(playURL, awsExpirationInMinutes * 60);
						}
						catch (exception &ex)
						{
							SPDLOG_ERROR(
								"getAWSSignedURL failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", playURL: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, playURL
							);

							// throw e;
						}
					}
					*/

					// update outputsRoot with the new details
					{
						field = "awsChannelConfigurationLabel";
						outputRoot[field] = awsChannelConfigurationLabel;

						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						// field = "playUrl";
						// outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							// string encodingParameters = JSONUtils::toString(
							// 	_encodingItem->_encodingParametersRoot);

							SPDLOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", awsChannelConfigurationLabel: {}"
								", awsChannelId: {}"
								", rtmpURL: {}"
								", playURL: {}"
								", channelAlreadyReserved: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, awsChannelConfigurationLabel, awsChannelId, rtmpURL, playURL, channelAlreadyReserved
							);

							// update sia IngestionJob che EncodingJob
							_mmsEngineDBFacade->updateOutputRtmp(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL
							);
							// _mmsEngineDBFacade->updateEncodingJobParameters(
							// 	_encodingItem->_encodingJobKey,
							// 	encodingParameters);
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}

					// channelAlreadyReserved true means the channel was already
					// reserved, so it is supposed is already started Maybe just
					// start again is not an issue!!! Let's see
					if (!channelAlreadyReserved)
						awsStartChannel(_encodingItem->_ingestionJobKey, awsChannelId);
				}
				else if (outputType == "CDN_CDN77")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string cdn77ChannelConfigurationLabel = JSONUtils::asString(outputRoot, "cdn77ChannelConfigurationLabel", "");
					// int cdn77ExpirationInMinutes = JSONUtils::asInt(outputRoot, "cdn77ExpirationInMinutes", 1440);

					/*
					string cdn77ChannelType;
					if (cdn77ChannelConfigurationLabel == "")
						cdn77ChannelType = "SHARED";
					else
						cdn77ChannelType = "DEDICATED";
					*/

					// reserveCDN77Channel ritorna exception se non ci sono piu
					// canali liberi o quello dedicato è già occupato In caso di
					// ripartenza di mmsEngine, nel caso di richiesta già
					// attiva, ritornerebbe le stesse info associate a
					// ingestionJobKey (senza exception)
					auto [reservedLabel, rtmpURL, resourceURL, filePath, secureToken, channelAlreadyReserved] =
						_mmsEngineDBFacade->reserveCDN77Channel(
							_encodingItem->_workspace->_workspaceKey, cdn77ChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
						);

					/*
					string reservedLabel;
					string rtmpURL;
					string resourceURL;
					string filePath;
					string secureToken;
					bool channelAlreadyReserved;
					tie(reservedLabel, rtmpURL, resourceURL, filePath, secureToken, channelAlreadyReserved) = cdn77ChannelDetails;
					*/

					/*
					if (filePath.size() > 0 && filePath.front() != '/')
						filePath = "/" + filePath;

					string playURL;
					if (secureToken != "")
					{
						try
						{
							playURL = MMSDeliveryAuthorization::getSignedCDN77URL(resourceURL, filePath, secureToken, cdn77ExpirationInMinutes * 60);
						}
						catch (exception &ex)
						{
							SPDLOG_ERROR(
								"getSignedCDN77URL failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
							);

							// throw e;
						}
					}
					else
					{
						playURL = "https://" + resourceURL + filePath;
					}
					*/

					// update outputsRoot with the new details
					{
						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						// field = "playUrl";
						// outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							SPDLOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", cdn77ChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", rtmpURL: {}"
								", resourceURL: {}"
								", filePath: {}"
								", secureToken: {}"
								", channelAlreadyReserved: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, cdn77ChannelConfigurationLabel, reservedLabel, rtmpURL, resourceURL, filePath,
								secureToken, channelAlreadyReserved
							);

							_mmsEngineDBFacade->updateOutputRtmp(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL
							);
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}
				}
				else if (outputType == "RTMP_Channel")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string rtmpChannelConfigurationLabel = JSONUtils::asString(outputRoot, "rtmpChannelConfigurationLabel", "");

					/*
					string rtmpChannelType;
					if (rtmpChannelConfigurationLabel == "")
						rtmpChannelType = "SHARED";
					else
						rtmpChannelType = "DEDICATED";
					*/

					// reserveRTMPChannel ritorna exception se non ci sono piu
					// canali liberi o quello dedicato è già occupato In caso di
					// ripartenza di mmsEngine, nel caso di richiesta già
					// attiva, ritornerebbe le stesse info associate a
					// ingestionJobKey (senza exception)
					auto [reservedLabel, rtmpURL, streamName, userName, password, playURL, channelAlreadyReserved] =
						_mmsEngineDBFacade->reserveRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, rtmpChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
						);

					/*
					string reservedLabel;
					string rtmpURL;
					string streamName;
					string userName;
					string password;
					string playURL;
					bool channelAlreadyReserved;
					tie(reservedLabel, rtmpURL, streamName, userName, password, playURL, channelAlreadyReserved) = rtmpChannelDetails;
					*/

					if (streamName != "")
					{
						if (rtmpURL.back() == '/')
							rtmpURL += streamName;
						else
							rtmpURL += ("/" + streamName);
					}
					if (userName != "" && password != "")
					{
						// rtmp://.....
						rtmpURL.insert(7, (userName + ":" + password + "@"));
					}

					// update outputsRoot with the new details
					{
						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						// field = "playUrl";
						// outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							SPDLOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", rtmpChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", rtmpURL: {}"
								", channelAlreadyReserved: {}"
								", playURL: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, rtmpChannelConfigurationLabel, reservedLabel, rtmpURL, channelAlreadyReserved, playURL
							);

							_mmsEngineDBFacade->updateOutputRtmp(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL
							);
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}
				}
				else if (outputType == "HLS_Channel")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string hlsChannelConfigurationLabel = JSONUtils::asString(outputRoot, "hlsChannelConfigurationLabel", "");

					/*
					string hlsChannelType;
					if (hlsChannelConfigurationLabel == "")
						hlsChannelType = "SHARED";
					else
						hlsChannelType = "DEDICATED";
					*/

					// reserveHLSChannel ritorna exception se non ci sono piu
					// canali liberi o quello dedicato è già occupato In caso di
					// ripartenza di mmsEngine, nel caso di richiesta già
					// attiva, ritornerebbe le stesse info associate a
					// ingestionJobKey (senza exception)
					auto [reservedLabel, deliveryCode, segmentDurationInSeconds, playlistEntriesNumber, channelAlreadyReserved] =
						_mmsEngineDBFacade->reserveHLSChannel(
							_encodingItem->_workspace->_workspaceKey, hlsChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
						);

					/*
					string reservedLabel;
					int64_t deliveryCode;
					int segmentDurationInSeconds;
					int playlistEntriesNumber;
					bool channelAlreadyReserved;
					tie(reservedLabel, deliveryCode, segmentDurationInSeconds, playlistEntriesNumber, channelAlreadyReserved) = hlsChannelDetails;
					*/

					// update outputsRoot with the new details
					{
						field = "deliveryCode";
						outputRoot[field] = deliveryCode;

						if (segmentDurationInSeconds > 0)
						{
							// if not present, default is decided by the encoder
							field = "segmentDurationInSeconds";
							outputRoot[field] = segmentDurationInSeconds;
						}

						if (playlistEntriesNumber > 0)
						{
							// if not present, default is decided by the encoder
							field = "playlistEntriesNumber";
							outputRoot[field] = playlistEntriesNumber;
						}

						if (outputIndex == monitorVirtualVODOutputRootIndex)
						{
							// in case of virtualVOD, è necessario modificare
							// PlaylistEntriesNumber considerando il parametro
							// VirtualVODMaxDurationInMinutes

							bool liveRecorderVirtualVOD = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, "liveRecorderVirtualVOD", false);

							if (liveRecorderVirtualVOD)
							{
								// 10 is the same default used in FFMpeg.cpp
								int localSegmentDurationInSeconds = segmentDurationInSeconds > 0 ? segmentDurationInSeconds : 10;
								int maxDurationInMinutes =
									JSONUtils::asInt(_encodingItem->_ingestedParametersRoot["liveRecorderVirtualVOD"], "maxDuration", 30);

								playlistEntriesNumber = (maxDurationInMinutes * 60) / localSegmentDurationInSeconds;
								outputRoot["playlistEntriesNumber"] = playlistEntriesNumber;
							}
						}

						string manifestDirectoryPath = _mmsStorage->getLiveDeliveryAssetPath(to_string(deliveryCode), _encodingItem->_workspace);
						string manifestFileName = to_string(deliveryCode) + ".m3u8";

						field = "manifestDirectoryPath";
						outputRoot[field] = manifestDirectoryPath;

						field = "manifestFileName";
						outputRoot[field] = manifestFileName;

						field = "otherOutputOptions";
						string otherOutputOptions = JSONUtils::asString(outputRoot, field, "");
						if (outputIndex == monitorVirtualVODOutputRootIndex)
						{
							// this is the OutputRoot of the monitor or
							// VirtualVOD E' necessario avere opzioni HLS
							// particolari

							string recordedFileNamePrefix = string("liveRecorder_") + to_string(_encodingItem->_ingestionJobKey)
								// + "_" +
								// to_string(_encodingItem->_encodingJobKey)
								;
							string segmentFilePathName = manifestDirectoryPath + "/" + recordedFileNamePrefix;

							string segmenterType = "hlsSegmenter";
							// string segmenterType = "streamSegmenter";
							if (segmenterType == "streamSegmenter")
							{
								// viene letto il timestamp dal nome del file
								segmentFilePathName += "_%s.ts";
								otherOutputOptions = "-hls_flags program_date_time -strftime 1 "
													 "-hls_segment_filename " +
													 segmentFilePathName; // + " -f hls";
																		  // viene già aggiunto
																		  // in ffmpeg.cpp
							}
							else
							{
								// NON viene letto il timestamp dal nome del
								// file 2023-04-12: usando -strftime 1 con _%s_
								// ho visto che il nome del file viene ripetuto
								//	uguale nel senso che %s viene sostituito con
								// lo stesso numero di secondi per due file
								//	differenti. Avere lo stesso nome file crea
								// problemi quando si crea il virtual VOD 	copiano
								// lo stesso file due volte, la seconda copia
								// ritorna un errore 	Per questo motivo viene
								// usato un semplice contatore _%04d
								segmentFilePathName += "_%04d.ts";
								otherOutputOptions = "-hls_flags program_date_time "
													 "-hls_segment_filename " +
													 segmentFilePathName; // + " -f hls";
																		  // viene già aggiunto
																		  // in ffmpeg.cpp
							}

							outputRoot[field] = otherOutputOptions;
						}

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							SPDLOG_INFO(
								"updateOutputHLSDetails"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", hlsChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", outputIndex: {}"
								", monitorVirtualVODOutputRootIndex: {}"
								", deliveryCode: {}"
								", segmentDurationInSeconds: {}"
								", playlistEntriesNumber: {}"
								", manifestDirectoryPath: {}"
								", manifestFileName: {}"
								", otherOutputOptions: {}"
								", channelAlreadyReserved: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, hlsChannelConfigurationLabel, reservedLabel, outputIndex,
								monitorVirtualVODOutputRootIndex, deliveryCode, segmentDurationInSeconds, playlistEntriesNumber,
								manifestDirectoryPath, manifestFileName, otherOutputOptions, channelAlreadyReserved
							);

							_mmsEngineDBFacade->updateOutputHLSDetails(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, deliveryCode, segmentDurationInSeconds,
								playlistEntriesNumber, manifestDirectoryPath, manifestFileName, otherOutputOptions
							);
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}
				}
			}

			killedByUser = liveRecorder_through_ffmpeg();

			if (killedByUser)
			{
				SPDLOG_WARN(
					"Encoding killed by the User"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);

				// the catch will releaseXXXChannel
				throw EncodingKilledByUser();
			}

			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "CDN_AWS")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseAWSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "CDN_CDN77")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseCDN77Channel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseCDN77Channel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseRTMPChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "HLS_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseHLSChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseHLSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
			}
		}
		catch (...)
		{
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "CDN_AWS")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseAWSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "CDN_CDN77")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseCDN77Channel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseCDN77Channel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseRTMPChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "HLS_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseHLSChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseHLSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						SPDLOG_ERROR(errorMessage);
					}
				}
			}

			// throw the same received exception
			throw;
		}

		return killedByUser;
	}
}

bool EncoderProxy::liveRecorder_through_ffmpeg()
{
	string streamSourceType;
	string encodersPool;
	{
		streamSourceType = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "streamSourceType", "");
		encodersPool = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodersPoolLabel", "");
	}

	bool timePeriod = true;
	string ipPushStreamConfigurationLabel;
	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	{
		if (streamSourceType == "IP_PUSH")
			ipPushStreamConfigurationLabel = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, "configurationLabel", "");

		string field = "schedule";
		json recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)[field];

		field = "start";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

		field = "autoRenew";
		autoRenew = JSONUtils::asBool(recordingPeriodRoot, field, false);
	}

	long maxAttemptsNumberInCaseOfErrors = 5;

	return waitingLiveProxyOrLiveRecorder(
		MMSEngineDBFacade::EncodingType::LiveRecorder, _ffmpegLiveRecorderURI, timePeriod, utcRecordingPeriodStart, utcRecordingPeriodEnd,
		maxAttemptsNumberInCaseOfErrors, ipPushStreamConfigurationLabel
	);
}

void EncoderProxy::processLiveRecorder(bool killedByUser)
{
	try
	{
		// Status will be success if at least one Chunk was generated, otherwise
		// it will be failed
		{
			string errorMessage;
			string processorMMS;
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

			SPDLOG_INFO(
				"Update IngestionJob"
				", ingestionJobKey: {}"
				", IngestionStatus: {}"
				", errorMessage: {}"
				", processorMMS: {}",
				_encodingItem->_ingestionJobKey, MMSEngineDBFacade::toString(newIngestionStatus), errorMessage, processorMMS
			);
			_mmsEngineDBFacade->updateIngestionJob(_encodingItem->_ingestionJobKey, newIngestionStatus, errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processLiveRecorder failed"
			", _proxyIdentifier: {}"
			", _encodingJobKey: {}"
			", _ingestionJobKey: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey, _encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processLiveRecorder failed"
			", _proxyIdentifier: {}"
			", _encodingJobKey: {}"
			", _ingestionJobKey: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey, _encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}
