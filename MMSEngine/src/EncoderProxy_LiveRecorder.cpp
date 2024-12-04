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
#include "MMSCURL.h"
#include "MMSDeliveryAuthorization.h"
#include "catralibraries/DateTime.h"

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
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

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

	time_t utcNow;
	{
		chrono::system_clock::time_point now = chrono::system_clock::now();
		utcNow = chrono::system_clock::to_time_t(now);
	}

	// MMS allocates a thread just 5 minutes before the beginning of the
	// recording
	if (utcNow < utcRecordingPeriodStart)
	{
		if (utcRecordingPeriodStart - utcNow >= _timeBeforeToPrepareResourcesInMinutes * 60)
		{
			_logger->info(
				__FILEREF__ + "Too early to allocate a thread for recording" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", utcRecordingPeriodStart - utcNow: " + to_string(utcRecordingPeriodStart - utcNow) +
				", _timeBeforeToPrepareResourcesInSeconds: " + to_string(_timeBeforeToPrepareResourcesInMinutes * 60)
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
			string errorMessage = __FILEREF__ + "Too late to activate the recording" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd) + ", utcNow: " + to_string(utcNow);
			_logger->error(errorMessage);

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
					bool awsSignedURL = JSONUtils::asBool(outputRoot, "awsSignedURL", false);
					int awsExpirationInMinutes = JSONUtils::asInt(outputRoot, "awsExpirationInMinutes", 1440);

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
					tuple<string, string, string, bool> awsChannelDetails = _mmsEngineDBFacade->reserveAWSChannel(
						_encodingItem->_workspace->_workspaceKey, awsChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					string awsChannelId;
					string rtmpURL;
					string playURL;
					bool channelAlreadyReserved;
					tie(awsChannelId, rtmpURL, playURL, channelAlreadyReserved) = awsChannelDetails;

					if (awsSignedURL)
					{
						try
						{
							playURL = getAWSSignedURL(playURL, awsExpirationInMinutes);
						}
						catch (exception &ex)
						{
							_logger->error(
								__FILEREF__ + "getAWSSignedURL failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", playURL: " + playURL
							);

							// throw e;
						}
					}

					// update outputsRoot with the new details
					{
						field = "awsChannelConfigurationLabel";
						outputRoot[field] = awsChannelConfigurationLabel;

						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						field = "playUrl";
						outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							// string encodingParameters = JSONUtils::toString(
							// 	_encodingItem->_encodingParametersRoot);

							_logger->info(
								__FILEREF__ + "updateOutputRtmpAndPlaURL" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								", workspaceKey: " + to_string(_encodingItem->_workspace->_workspaceKey) + ", ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								", awsChannelConfigurationLabel: " + awsChannelConfigurationLabel + ", awsChannelId: " + awsChannelId +
								", rtmpURL: " + rtmpURL + ", playURL: " + playURL + ", channelAlreadyReserved: " + to_string(channelAlreadyReserved)
								// + ", encodingParameters: " +
								// encodingParameters
							);

							// update sia IngestionJob che EncodingJob
							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL, playURL
							);
							// _mmsEngineDBFacade->updateEncodingJobParameters(
							// 	_encodingItem->_encodingJobKey,
							// 	encodingParameters);
						}
						catch (runtime_error &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" +
								", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					int cdn77ExpirationInMinutes = JSONUtils::asInt(outputRoot, "cdn77ExpirationInMinutes", 1440);

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
					tuple<string, string, string, string, string, bool> cdn77ChannelDetails = _mmsEngineDBFacade->reserveCDN77Channel(
						_encodingItem->_workspace->_workspaceKey, cdn77ChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					string reservedLabel;
					string rtmpURL;
					string resourceURL;
					string filePath;
					string secureToken;
					bool channelAlreadyReserved;
					tie(reservedLabel, rtmpURL, resourceURL, filePath, secureToken, channelAlreadyReserved) = cdn77ChannelDetails;

					if (filePath.size() > 0 && filePath.front() != '/')
						filePath = "/" + filePath;

					string playURL;
					if (secureToken != "")
					{
						try
						{
							playURL =
								MMSDeliveryAuthorization::getSignedCDN77URL(resourceURL, filePath, secureToken, cdn77ExpirationInMinutes, _logger);
						}
						catch (exception &ex)
						{
							_logger->error(
								__FILEREF__ + "getSignedCDN77URL failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							);

							// throw e;
						}
					}
					else
					{
						playURL = "https://" + resourceURL + filePath;
					}

					// update outputsRoot with the new details
					{
						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						field = "playUrl";
						outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							_logger->info(
								__FILEREF__ + "updateOutputRtmpAndPlaURL" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								", workspaceKey: " + to_string(_encodingItem->_workspace->_workspaceKey) + ", ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								", cdn77ChannelConfigurationLabel: " + cdn77ChannelConfigurationLabel + ", reservedLabel: " + reservedLabel +
								", rtmpURL: " + rtmpURL + ", resourceURL: " + resourceURL + ", filePath: " + filePath + ", secureToken: " +
								secureToken + ", channelAlreadyReserved: " + to_string(channelAlreadyReserved) + ", playURL: " + playURL
							);

							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL, playURL
							);
						}
						catch (runtime_error &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" +
								", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					tuple<string, string, string, string, string, string, bool> rtmpChannelDetails = _mmsEngineDBFacade->reserveRTMPChannel(
						_encodingItem->_workspace->_workspaceKey, rtmpChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					string reservedLabel;
					string rtmpURL;
					string streamName;
					string userName;
					string password;
					string playURL;
					bool channelAlreadyReserved;
					tie(reservedLabel, rtmpURL, streamName, userName, password, playURL, channelAlreadyReserved) = rtmpChannelDetails;

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

						field = "playUrl";
						outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							_logger->info(
								__FILEREF__ + "updateOutputRtmpAndPlaURL" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								", workspaceKey: " + to_string(_encodingItem->_workspace->_workspaceKey) + ", ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								", rtmpChannelConfigurationLabel: " + rtmpChannelConfigurationLabel + ", reservedLabel: " + reservedLabel +
								", rtmpURL: " + rtmpURL + ", channelAlreadyReserved: " + to_string(channelAlreadyReserved) + ", playURL: " + playURL
							);

							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL, playURL
							);
						}
						catch (runtime_error &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" +
								", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					tuple<string, int64_t, int, int, bool> hlsChannelDetails = _mmsEngineDBFacade->reserveHLSChannel(
						_encodingItem->_workspace->_workspaceKey, hlsChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					string reservedLabel;
					int64_t deliveryCode;
					int segmentDurationInSeconds;
					int playlistEntriesNumber;
					bool channelAlreadyReserved;
					tie(reservedLabel, deliveryCode, segmentDurationInSeconds, playlistEntriesNumber, channelAlreadyReserved) = hlsChannelDetails;

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
							_logger->info(
								__FILEREF__ + "updateOutputHLSDetails" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								", workspaceKey: " + to_string(_encodingItem->_workspace->_workspaceKey) + ", ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								", hlsChannelConfigurationLabel: " + hlsChannelConfigurationLabel + ", reservedLabel: " + reservedLabel +
								", outputIndex: " + to_string(outputIndex) +
								", monitorVirtualVODOutputRootIndex: " + to_string(monitorVirtualVODOutputRootIndex) +
								", deliveryCode: " + to_string(deliveryCode) + ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds) +
								", playlistEntriesNumber: " + to_string(playlistEntriesNumber) + ", manifestDirectoryPath: " + manifestDirectoryPath +
								", manifestFileName: " + manifestFileName + ", otherOutputOptions: " + otherOutputOptions +
								", channelAlreadyReserved: " + to_string(channelAlreadyReserved)
							);

							_mmsEngineDBFacade->updateOutputHLSDetails(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, deliveryCode, segmentDurationInSeconds,
								playlistEntriesNumber, manifestDirectoryPath, manifestFileName, otherOutputOptions
							);
						}
						catch (runtime_error &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" +
								", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobParameters failed" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							);

							// throw e;
						}
					}
				}
			}

			killedByUser = liveRecorder_through_ffmpeg();
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
						string errorMessage = __FILEREF__ + "releaseAWSChannel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
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
						string errorMessage = __FILEREF__ + "releaseCDN77Channel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
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
						string errorMessage = __FILEREF__ + "releaseRTMPChannel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
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
						string errorMessage = __FILEREF__ + "releaseHLSChannel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
					}
				}
			}

			if (killedByUser) // KilledByUser
			{
				string errorMessage = __FILEREF__ + "Encoding killed by the User" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
									  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
				_logger->warn(errorMessage);

				throw EncodingKilledByUser();
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
						string errorMessage = __FILEREF__ + "releaseAWSChannel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
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
						string errorMessage = __FILEREF__ + "releaseCDN77Channel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
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
						string errorMessage = __FILEREF__ + "releaseRTMPChannel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
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
						string errorMessage = __FILEREF__ + "releaseHLSChannel failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);
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
	bool exitInCaseOfUrlNotFoundOrForbidden;
	string streamSourceType;
	string encodersPool;
	{
		string field = "exitInCaseOfUrlNotFoundOrForbidden";
		exitInCaseOfUrlNotFoundOrForbidden = JSONUtils::asBool(_encodingItem->_ingestedParametersRoot, field, false);

		field = "streamSourceType";
		streamSourceType = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

		field = "encodersPoolLabel";
		encodersPool = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");
	}

	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	{
		string field = "schedule";
		json recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)[field];

		field = "start";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

		field = "autoRenew";
		autoRenew = JSONUtils::asBool(recordingPeriodRoot, field, false);
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	time_t utcNowToCheckExit = 0;
	while (!killedByUser // && !urlForbidden && !urlNotFound
		   && utcNowToCheckExit < utcRecordingPeriodEnd)
	{
		if (urlForbidden || urlNotFound)
		{
			if (exitInCaseOfUrlNotFoundOrForbidden)
			{
				_logger->warn(
					__FILEREF__ +
					"url not found or forbidden, terminate the LiveRecorder "
					"task" +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", exitInCaseOfUrlNotFoundOrForbidden: " + to_string(exitInCaseOfUrlNotFoundOrForbidden) +
					", urlForbidden: " + to_string(urlForbidden) + ", urlNotFound: " + to_string(urlNotFound)
				);

				break;
			}
			else
			{
				int waitingInSeconsBeforeTryingAgain = 30;

				_logger->warn(
					__FILEREF__ + "url not found or forbidden, wait a bit and try again" + ", _ingestionJobKey: " +
					to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", exitInCaseOfUrlNotFoundOrForbidden: " + to_string(exitInCaseOfUrlNotFoundOrForbidden) +
					", urlForbidden: " + to_string(urlForbidden) + ", urlNotFound: " + to_string(urlNotFound) +
					", waitingInSeconsBeforeTryingAgain: " + to_string(waitingInSeconsBeforeTryingAgain)
				);

				this_thread::sleep_for(chrono::seconds(waitingInSeconsBeforeTryingAgain));
			}
		}

		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveRecorderURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			_currentUsedFFMpegExternalEncoder = false;

			if (_encodingItem->_encoderKey == -1)
			{
				if (streamSourceType == "IP_PUSH")
				{
					// scenario:
					// 	- viene configurato uno Stream per un IP_PUSH su un encoder specifico
					// 	- questo encoder ha un fault e va giu
					// 	- finche questo encode non viene ripristinato (dipende da Hetzner) abbiamo un outage
					// 	- Per evitare l'outage, posso io cambiare encoder nella configurazione dello Stream
					// 	- La getStreamInputPushDetails sotto mi serve in questo loop per recuperare avere l'encoder aggiornato configurato nello
					// Stream 		altrimenti rimarremmo con l'encoder e l'url calcolata all'inizio e non potremmo evitare l'outage

					int64_t updatedPushEncoderKey = -1;
					string updatedUrl;
					{
						string streamConfigurationLabel = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, "configurationLabel", "");

						json internalMMSRoot = JSONUtils::asJson(_encodingItem->_ingestedParametersRoot, "internalMMS", nullptr);
						json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails", nullptr);
						if (encodersDetailsRoot == nullptr)
						{
							// quando elimino questo if, verifica se anche la funzione getStreamInputPushDetails possa essere eliminata
							// per essere sostituita da getStreamPushServerUrl
							tie(updatedPushEncoderKey, updatedUrl) = _mmsEngineDBFacade->getStreamInputPushDetails(
								_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, streamConfigurationLabel
							);
						}
						else
						{
							// questo quello corretto, l'if sopra dovrebbe essere eliminato

							updatedPushEncoderKey = JSONUtils::asInt64(encodersDetailsRoot, "pushEncoderKey", static_cast<int64_t>(-1));
							if (updatedPushEncoderKey == -1)
							{
								string errorMessage = fmt::format(
									"Wrong pushEncoderKey"
									", _ingestionJobKey: {}"
									", _encodingJobKey: {}"
									", encodersDetailsRoot: {}",
									_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, JSONUtils::toString(encodersDetailsRoot)
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							bool pushPublicEncoderName = JSONUtils::asBool(encodersDetailsRoot, "pushPublicEncoderName", false);

							updatedUrl = _mmsEngineDBFacade->getStreamPushServerUrl(
								_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, streamConfigurationLabel,
								updatedPushEncoderKey, pushPublicEncoderName, true
							);
						}
					}

					_encodingItem->_encodingParametersRoot["pushEncoderKey"] = updatedPushEncoderKey;
					_encodingItem->_encodingParametersRoot["liveURL"] = updatedUrl;

					_currentUsedFFMpegEncoderKey = updatedPushEncoderKey;
					// 2023-12-18: pushEncoderName è importante che sia usato
					// nella url rtmp
					//	dove il transcoder ascolta per il flusso di streaming
					//	ma non deve essere usato per decidere l'url con cui
					// l'engine deve comunicare 	con il transcoder. Questa url
					// dipende solamente dal fatto che il transcoder 	sia interno
					// o esterno
					tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) =
						_mmsEngineDBFacade->getEncoderURL(updatedPushEncoderKey); // , pushEncoderName);

					SPDLOG_INFO(
						"LiveRecorder. Retrieved updated Stream info"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", updatedPushEncoderKey: {}"
						", updatedUrl: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, updatedPushEncoderKey, updatedUrl
					);
				}
				else
				{
					SPDLOG_INFO(
						"LiveRecorder. Selection of the transcoder..."
						", _proxyIdentifier: {}"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}",
						_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
					);

					int64_t encoderKeyToBeSkipped = -1;
					bool externalEncoderAllowed = true;
					tuple<int64_t, string, bool> encoderDetails = _encodersLoadBalancer->getEncoderURL(
						_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace, encoderKeyToBeSkipped, externalEncoderAllowed
					);
					tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
				}

				SPDLOG_INFO(
					"LiveRecorder. Selection of the transcoder"
					", _proxyIdentifier: {}"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", streamSourceType: {}"
					", _currentUsedFFMpegEncoderHost: {}"
					", _currentUsedFFMpegEncoderKey: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, streamSourceType,
					_currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
				);

				ffmpegEncoderURL = _currentUsedFFMpegEncoderHost + ffmpegURI + "/" + to_string(_encodingItem->_ingestionJobKey) + "/" +
								   to_string(_encodingItem->_encodingJobKey);

				string body;
				{
					json liveRecorderMedatada;

					// 2023-03-21: rimuovere il parametro ingestionJobKey se il
					// trascoder deployed è > 1.0.5315
					liveRecorderMedatada["ingestionJobKey"] = _encodingItem->_ingestionJobKey;
					liveRecorderMedatada["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
					liveRecorderMedatada["encodingParametersRoot"] = _encodingItem->_encodingParametersRoot;
					liveRecorderMedatada["ingestedParametersRoot"] = _encodingItem->_ingestedParametersRoot;

					body = JSONUtils::toString(liveRecorderMedatada);
				}

				vector<string> otherHeaders;
				json liveRecorderContentResponse;
				try
				{
					liveRecorderContentResponse = MMSCURL::httpPostStringAndGetJson(
						_logger, _encodingItem->_ingestionJobKey, ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, _ffmpegEncoderUser,
						_ffmpegEncoderPassword, body,
						"application/json", // contentType
						otherHeaders
					);
				}
				catch (runtime_error &e)
				{
					string error = e.what();
					if (error.find(EncodingIsAlreadyRunning().what()) != string::npos)
					{
						// 2023-03-26:
						// Questo scenario indica che per il DB "l'encoding è da
						// eseguire" mentre abbiamo un Encoder che lo sta già
						// eseguendo Si tratta di una inconsistenza che non
						// dovrebbe mai accadere. Oggi pero' ho visto questo
						// scenario e l'ho risolto facendo ripartire sia
						// l'encoder che gli engines Gestire questo scenario
						// rende il sistema piu' robusto e recupera facilmente
						// una situazione che altrimenti richiederebbe una
						// gestione manuale Inoltre senza guardare nel log, non
						// si riuscirebbe a capire che siamo in questo scenario.

						// La gestione di questo scenario consiste nell'ignorare
						// questa eccezione facendo andare avanti la procedura,
						// come se non avesse generato alcun errore
						_logger->error(
							__FILEREF__ +
							"inconsistency: DB says the encoding has to be "
							"executed but the Encoder is already executing it. "
							"We will manage it" +
							", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", body: " + body + ", e.what: " + e.what()
						);
					}
					else
						throw e;
				}

				/* 2023-03-26; non si verifica mai, se FFMPEGEncoder genera un
				errore, ritorna un HTTP status diverso da 200 e quindi MMSCURL
				genera un eccezione
				{
					string field = "error";
					if
				(JSONUtils::isMetadataPresent(liveRecorderContentResponse,
				field))
					{
						string error =
				JSONUtils::asString(liveRecorderContentResponse, field, "");

						string errorMessage = string("FFMPEGEncoder error")
							+ ", _proxyIdentifier: " +
				to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " +
				to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey)
							+ ", error: " + error
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				*/
			}
			else
			{
				_logger->info(
					__FILEREF__ +
					"LiveRecorder. Selection of the transcoder. The transcoder "
					"is already saved (DB), the encoding should be already "
					"running" +
					", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				);

				pair<string, bool> encoderDetails = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

				// we have to reset _encodingItem->_encoderKey because in case
				// we will come back in the above 'while' loop, we have to
				// select another encoder
				_encodingItem->_encoderKey = -1;

				ffmpegEncoderURL = _currentUsedFFMpegEncoderHost + ffmpegURI + "/" + to_string(_encodingItem->_encodingJobKey);
			}

			chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

			{
				lock_guard<mutex> locker(*_mtEncodingJobs);

				*_status = EncodingJobStatus::Running;
			}

			_logger->info(
				__FILEREF__ + "Update EncodingJob" + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", transcoder: " + _currentUsedFFMpegEncoderHost + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			// loop waiting the end of the encoding
			bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
			int maxEncodingStatusFailures = 5; // consecutive errors
			int encodingStatusFailures = 0;
			int lastEncodingPid = 0;
			long lastRealTimeFrameRate = 0;
			double lastRealTimeBitRate = 0;
			int encodingPid;
			long realTimeFrameRate;
			double realTimeBitRate;
			long lastNumberOfRestartBecauseOfFailure = 0;
			long numberOfRestartBecauseOfFailure;
			// string lastRecordedAssetFileName;

			// see the comment few lines below (2019-05-03)
			// while(!(encodingFinished || encodingStatusFailures >=
			// maxEncodingStatusFailures))
			while (!encodingFinished)
			{
				this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

				try
				{
					// tuple<bool, bool, bool, string, bool, bool, double, int>
					// encodingStatus = getEncodingStatus(
					/* _encodingItem->_encodingJobKey */
					// );
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage, urlForbidden, urlNotFound, ignore, encodingPid,
						realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure) = getEncodingStatus();

					if (encodingErrorMessage != "")
					{
						try
						{
							string firstLineOfEncodingErrorMessage;
							{
								string firstLine;
								stringstream ss(encodingErrorMessage);
								if (getline(ss, firstLine))
									firstLineOfEncodingErrorMessage = firstLine;
								else
									firstLineOfEncodingErrorMessage = encodingErrorMessage;
							}

							_mmsEngineDBFacade->appendIngestionJobErrorMessage(_encodingItem->_ingestionJobKey, firstLineOfEncodingErrorMessage);
						}
						catch (runtime_error &e)
						{
							_logger->error(
								__FILEREF__ + "appendIngestionJobErrorMessage failed" +
								", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
							);
						}
						catch (exception &e)
						{
							_logger->error(
								__FILEREF__ + "appendIngestionJobErrorMessage failed" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							);
						}
					}

					if (completedWithError)
					{
						string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)" +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
											  ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost +
											  ", encodingErrorMessage: " + encodingErrorMessage;
						_logger->error(errorMessage);

						encodingStatusFailures++;

						// in this scenario encodingFinished is true

						// update EncodingJob failures number to notify the GUI
						// EncodingJob is failing
						try
						{
							_logger->info(
								__FILEREF__ + "check and update encodingJob FailuresNumber" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								", encodingStatusFailures: " + to_string(encodingStatusFailures)
							);

							bool isKilled =
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, encodingStatusFailures);
							if (isKilled)
							{
								_logger->info(
									__FILEREF__ +
									"LiveRecorder Killed by user during "
									"waiting loop" +
									", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
									to_string(_encodingItem->_encodingJobKey) + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								// when previousEncodingStatusFailures is < 0
								// means:
								// 1. the live recording is not starting (ffmpeg
								// is generating
								//		continuously an error)
								// 2. User killed the encoding through MMS GUI
								// or API
								// 3. the kill procedure (in API module) was not
								// able to kill the ffmpeg process,
								//		because it does not exist the process
								// and set the failuresNumber DB field 		to a
								// negative value in order to communicate with
								// this thread
								// 4. This thread, when it finds a negative
								// failuresNumber, knows the encoding
								//		was killed and exit from the loop
								encodingFinished = true;
								killedByUser = true;
							}
						}
						catch (...)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobFailuresNumber FAILED" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								", encodingStatusFailures: " + to_string(encodingStatusFailures)
							);
						}

						throw runtime_error(errorMessage);
					}

					// encodingProgress/encodingPid
					{
						try
						{
							time_t utcNow;
							{
								chrono::system_clock::time_point now = chrono::system_clock::now();
								utcNow = chrono::system_clock::to_time_t(now);
							}

							double encodingProgress;

							if (utcNow < utcRecordingPeriodStart)
								encodingProgress = 0.0;
							else if (utcRecordingPeriodStart < utcNow && utcNow < utcRecordingPeriodEnd)
							{
								double elapsed = utcNow - utcRecordingPeriodStart;
								double recordingPeriod = utcRecordingPeriodEnd - utcRecordingPeriodStart;
								encodingProgress = (elapsed * 100) / recordingPeriod;
							}
							else
								encodingProgress = 100.0;

							_logger->info(
								__FILEREF__ + "updateEncodingJobProgress" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								", encodingProgress: " + to_string(encodingProgress) + ", utcNow: " + to_string(utcNow) +
								", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart) +
								", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
							);
							_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
						}
						catch (runtime_error &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobProgress failed" +
								", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
							);
						}
						catch (exception &e)
						{
							_logger->error(
								__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							);
						}

						if (lastEncodingPid != encodingPid || lastRealTimeFrameRate != realTimeFrameRate || lastRealTimeBitRate != realTimeBitRate ||
							lastNumberOfRestartBecauseOfFailure != numberOfRestartBecauseOfFailure)
						{
							try
							{
								SPDLOG_INFO(
									"updateEncodingRealTimeInfo"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", encodingPid: {}"
									", realTimeFrameRate: {}"
									", realTimeBitRate: {}"
									", numberOfRestartBecauseOfFailure: {}",
									_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
									numberOfRestartBecauseOfFailure
								);
								_mmsEngineDBFacade->updateEncodingRealTimeInfo(
									_encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure
								);

								lastEncodingPid = encodingPid;
								lastRealTimeFrameRate = realTimeFrameRate;
								lastRealTimeBitRate = realTimeBitRate;
								lastNumberOfRestartBecauseOfFailure = numberOfRestartBecauseOfFailure;
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									"updateEncodingRealTimeInfo failed"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", encodingPid: {}"
									", realTimeFrameRate: {}"
									", realTimeBitRate: {}"
									", numberOfRestartBecauseOfFailure: {}"
									", e.what: {}",
									_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
									numberOfRestartBecauseOfFailure, e.what()
								);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									"updateEncodingRealTimeInfo failed"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", encodingPid: {}"
									", realTimeFrameRate: {}"
									", realTimeBitRate: {}"
									", numberOfRestartBecauseOfFailure: {}",
									_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
									numberOfRestartBecauseOfFailure
								);
							}
						}
					}

					// 2020-06-10: encodingStatusFailures is reset since
					// getEncodingStatus was successful.
					//	Scenario:
					//		1. only sometimes (about once every two hours) an
					// encoder (deployed on centos) running a LiveRecorder
					// continuously, 			returns 'timeout'. 			Really the encoder was
					// working fine, ffmpeg was also running fine, 			just
					// FastCGIAccept was not getting the request
					//		2. these errors was increasing
					// encodingStatusFailures and at the end, it reached the max
					// failures 			and this thread terminates, even if the encoder
					// and ffmpeg was working fine. 		This scenario creates
					// problems and non-consistency between engine and encoder.
					//		For this reason, if the getEncodingStatus is
					// successful, encodingStatusFailures is reset.
					encodingStatusFailures = 0;

					/*
					lastRecordedAssetFileName =
					processLastGeneratedLiveRecorderFiles( highAvailability,
					main, segmentDurationInSeconds,
						transcoderStagingContentsPath + segmentListFileName,
					recordedFileNamePrefix, liveRecordingContentsPath,
					lastRecordedAssetFileName);
					*/
				}
				catch (...)
				{
					/* 2022-09-03: Since we introdiced the
					exitInCaseOfUrlNotFoundOrForbidden flag, I guess the below
					urlNotFound management is not needed anymore

					// 2020-05-20: The initial loop will make the liveRecording
					to exit in case of urlNotFound.
					//	This is because in case the URL is not found, does not
					have sense to try again the liveRecording.
					//	This is true in case the URL not found error happens at
					the beginning of the liveRecording.
					//	This is not always the case. Sometimes the URLNotFound
					error is returned by ffmpeg after a lot of time
					//	the liveRecoridng is started and because just one ts
					file was not found (this is in case of m3u8 URL).
					//	In this case we do not have to exit from the loop and we
					have just to try again long urlNotFoundFakeAfterMinutes =
					10; long encodingDurationInMinutes =
					chrono::duration_cast<chrono::minutes>(
						chrono::system_clock::now() - startEncoding).count();
					if (urlNotFound && encodingDurationInMinutes >
					urlNotFoundFakeAfterMinutes)
					{
						// 2020-06-06. Scenario:
						//	- MMS was sending RAI 1 to CDN77
						//	- here we were recording the streaming poiting to
					the CDN77 URL
						//	- MMS has an error (restarted because of
					'Non-monotonous DTS in output stream/incorrect timestamps')
						//	- here we had the URL not found error
						// Asking again the URL raises again 'URL not found'
					error. For this reason we added
						// a waiting, let's see if 60 seconds is enough
						int waitingInSeconsBeforeTryingAgain = 60;

						_logger->error(__FILEREF__ + "fake urlNotFound, wait a
					bit and try again"
							+ ", _ingestionJobKey: " +
					to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey)
							+ ", encodingStatusFailures: " +
					to_string(encodingStatusFailures)
							+ ", maxEncodingStatusFailures: " +
					to_string(maxEncodingStatusFailures)
							+ ", waitingInSeconsBeforeTryingAgain: " +
					to_string(waitingInSeconsBeforeTryingAgain)
						);

						urlNotFound = false;

						// in case URL not found is because of a segment not
					found
						// or in case of a temporary failures
						//		let's wait a bit before to try again
						this_thread::sleep_for(chrono::seconds(waitingInSeconsBeforeTryingAgain));
					}
					else
					*/
					{
						_logger->info(
							__FILEREF__ + "it is not a fake urlNotFound" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", urlNotFound: " +
							to_string(urlNotFound)
							// + ", @MMS statistics@ -
							// encodingDurationInMinutes: @" +
							// to_string(encodingDurationInMinutes) + "@"
							// + ", urlNotFoundFakeAfterMinutes: " +
							// to_string(urlNotFoundFakeAfterMinutes)
							+ ", encodingStatusFailures: " + to_string(encodingStatusFailures) +
							", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
						);
					}

					// already incremented in above in if (completedWithError)
					// encodingStatusFailures++;

					_logger->error(
						__FILEREF__ + "getEcodingStatus failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encodingStatusFailures: " +
						to_string(encodingStatusFailures) + ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					);

					/*
					 2019-05-03: commented because we saw the following
					 scenario:
						1. getEncodingStatus fails because of HTTP call failure
					 (502 Bad Gateway) Really the live recording is still
					 working into the encoder process
						2. EncoderProxy reached
					 maxEncodingStatusFailures
						3. since the recording is not finished yet, this
					 code/method activate a new live recording session Result:
					 we have 2 live recording process into the encoder creating
					 problems To avoid that we will exit from this loop ONLY
					 when we are sure the recoridng is finished 2019-07-02: in
					 case the encoder was shutdown or crashed, the Engine has to
					 activate another Encoder, so we increased
					 maxEncodingStatusFailures to be sure the encoder is not
					 working anymore and, in this case we do a break in order to
					 activate another encoder.
					*/
					if (encodingStatusFailures >= maxEncodingStatusFailures)
					{
						string errorMessage = string("getEncodingStatus too many failures") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
											  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
											  ", encodingFinished: " + to_string(encodingFinished) +
											  ", encodingStatusFailures: " + to_string(encodingStatusFailures) +
											  ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures);
						_logger->error(__FILEREF__ + errorMessage);

						break;
						// throw runtime_error(errorMessage);
					}
				}
			}

			chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

			utcNowToCheckExit = chrono::system_clock::to_time_t(endEncoding);

			if (utcNowToCheckExit < utcRecordingPeriodEnd)
			{
				_logger->error(
					__FILEREF__ + "LiveRecorder media file completed unexpected" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", still remaining seconds (utcRecordingPeriodEnd - "
					"utcNow): " +
					to_string(utcRecordingPeriodEnd - utcNowToCheckExit) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
					", encodingFinished: " + to_string(encodingFinished) + ", encodingStatusFailures: " + to_string(encodingStatusFailures) +
					", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures) + ", killedByUser: " + to_string(killedByUser) +
					", @MMS statistics@ - encodingDuration (secs): @" +
					to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@" +
					", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);
			}
			else
			{
				_logger->info(
					__FILEREF__ + "LiveRecorder media file completed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", autoRenew: " + to_string(autoRenew) +
					", encodingFinished: " + to_string(encodingFinished) + ", killedByUser: " + to_string(killedByUser) +
					", ffmpegEncoderURL: " + ffmpegEncoderURL + ", @MMS statistics@ - encodingDuration (secs): @" +
					to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@" +
					", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);

				if (autoRenew)
				{
					_logger->info(__FILEREF__ + "Renew Live Recording" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey));

					time_t recordingPeriodInSeconds = utcRecordingPeriodEnd - utcRecordingPeriodStart;

					utcRecordingPeriodStart = utcRecordingPeriodEnd;
					utcRecordingPeriodEnd += recordingPeriodInSeconds;

					utcNowToCheckExit = 0;

					// let's select again the encoder
					_encodingItem->_encoderKey = -1;

					_logger->info(
						__FILEREF__ + "Update Encoding LiveRecording Period" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", utcRecordingPeriodStart: " +
						to_string(utcRecordingPeriodStart) + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
					);
					_mmsEngineDBFacade->updateIngestionAndEncodingLiveRecordingPeriod(
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcRecordingPeriodStart, utcRecordingPeriodEnd
					);

					// next update is important because the JSON is used in the
					// getEncodingProgress method 2022-11-09: I do not call
					// anymore getEncodingProgress 2022-11-20: next update is
					// mandatory otherwise we will have the folloging error:
					//		FFMpeg.cpp:8679: LiveRecorder timing. Too late to
					// start the LiveRecorder
					{
						string field = "utcScheduleStart";
						_encodingItem->_encodingParametersRoot[field] = utcRecordingPeriodStart;

						field = "utcScheduleEnd";
						_encodingItem->_encodingParametersRoot[field] = utcRecordingPeriodEnd;
					}
				}
			}
		}
		catch (EncoderNotFound &e)
		{
			string errorMessage = string("EncoderNotFound") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			// in this case we will through the exception independently if the
			// live streaming time (utcRecordingPeriodEnd) is finished or not.
			// This encodingJob will be set as failed
			throw runtime_error(e.what());
		}
		catch (MaxConcurrentJobsReached &e)
		{
			string errorMessage = string("MaxConcurrentJobsReached") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", response.str(): " + (responseInitialized ? response.str() : "") + ", e.what(): " + e.what();
			_logger->warn(__FILEREF__ + errorMessage);

			// in this case we will through the exception independently if the
			// live streaming time (utcRecordingPeriodEnd) is finished or not.
			// This task will come back by the MMS system
			throw e;
		}
		catch (runtime_error e)
		{
			string error = e.what();
			if (error.find(NoEncodingAvailable().what()) != string::npos)
			{
				string errorMessage = string("No Encodings available / MaxConcurrentJobsReached") +
									  ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
									  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", error: " + error;
				_logger->warn(__FILEREF__ + errorMessage);

				throw MaxConcurrentJobsReached();
			}
			else
			{
				_logger->error(
					__FILEREF__ + "Encoding URL failed (runtime_error)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
					", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				// sleep a bit and try again
				int sleepTime = 30;
				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNowToCheckExit = chrono::system_clock::to_time_t(now);
				}

				// throw e;
			}
		}
		catch (exception e)
		{
			_logger->error(
				__FILEREF__ + "Encoding URL failed (exception)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowToCheckExit = chrono::system_clock::to_time_t(now);
			}

			// throw e;
		}
	}

	// Ingestion/Encoding Status will be success if at least one Chunk was
	// generated otherwise it will be set as failed if (main)
	{
		if (urlForbidden)
		{
			string errorMessage = __FILEREF__ + "LiveRecorder: URL forbidden" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName;
			_logger->error(errorMessage);

			throw FFMpegURLForbidden();
		}
		else if (urlNotFound)
		{
			string errorMessage = __FILEREF__ + "LiveRecorder: URL Not Found" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName;
			_logger->error(errorMessage);

			throw FFMpegURLNotFound();
		}
		else
		{
			long ingestionJobOutputsCount = _mmsEngineDBFacade->getIngestionJobOutputsCount(
				_encodingItem->_ingestionJobKey,
				// 2022-12-18: true because IngestionJobOutputs was updated
				// recently
				true
			);
			if (ingestionJobOutputsCount <= 0)
			{
				string errorMessage = __FILEREF__ + "LiveRecorder: no chunks were generated" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
									  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									  ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName +
									  ", ingestionJobOutputsCount: " + to_string(ingestionJobOutputsCount);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}

	// return make_tuple(killedByUser, main);
	return killedByUser;
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

			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", IngestionStatus: " +
				MMSEngineDBFacade::toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			_mmsEngineDBFacade->updateIngestionJob(_encodingItem->_ingestionJobKey, newIngestionStatus, errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "processLiveRecorder failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName + ", e.what(): " + e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "processLiveRecorder failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
		);

		throw e;
	}
}
