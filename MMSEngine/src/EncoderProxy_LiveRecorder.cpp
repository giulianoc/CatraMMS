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

	time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

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

			if (killedByUser)
			{
				string errorMessage = __FILEREF__ + "Encoding killed by the User" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
									  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
				_logger->warn(errorMessage);

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
