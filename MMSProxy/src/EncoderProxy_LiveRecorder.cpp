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

#include "CurlWrapper.h"
#include "Datetime.h"
#include "EncoderProxy.h"
#include "FFMpegWrapper.h"
#include "JSONUtils.h"
#include "MMSDeliveryAuthorization.h"
#include "spdlog/spdlog.h"
#include <FFMpegWrapper.h>

using namespace std;
using json = nlohmann::json;

bool EncoderProxy::liveRecorder()
{
	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	{
		string field = "schedule";
		json recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)[field];

		field = "start";
		if (!JSONUtils::isPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::as<string>(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = Datetime::parseUtcStringToUtcInSecs(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodEnd = JSONUtils::as<string>(recordingPeriodRoot, field, "");
		utcRecordingPeriodEnd = Datetime::parseUtcStringToUtcInSecs(recordingPeriodEnd);

		field = "autoRenew";
		autoRenew = JSONUtils::as<bool>(recordingPeriodRoot, field, false);

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
			LOG_INFO(
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
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	{
		string field = "outputsRoot";
		json outputsRoot = (_encodingItem->_encodingParametersRoot)[field];

		bool killed = false;
		try
		{
			field = "monitorVirtualVODOutputRootIndex";
			int monitorVirtualVODOutputRootIndex = JSONUtils::as<int32_t>(_encodingItem->_encodingParametersRoot, field, -1);

			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::as<string>(outputRoot, "outputType", "");

				if (outputType == "RTMP_Channel")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string rtmpChannelConfigurationLabel = JSONUtils::as<string>(outputRoot, "rtmpChannelConfigurationLabel", "");

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
					auto [reservedLabel, rtmpURL, streamName, userName, password, channelAlreadyReserved,
						playURLDetailsRoot] = _mmsEngineDBFacade->reserveRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, rtmpChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
						);

					if (streamName != "")
					{
						if (rtmpURL.back() == '/')
							rtmpURL += streamName;
						else
							rtmpURL += ("/" + streamName);
					}
					if (userName != "" && password != "")
					{
						// nel caso di rtmp nativo (basato su libavformat/protocols/rtmp.c) non serve l'escape.
						// Ad esempio avevo una password con il '.' e, se fosse codificato, non funziona
						// rtmpURL.insert(7, (CurlWrapper::escape(userName) + ":" + CurlWrapper::escape(password) + "@"));
						rtmpURL.insert(7, (userName + ":" + password + "@"));
					}

					// update outputsRoot with the new details
					{
						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							LOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", rtmpChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", rtmpURL: {}"
								", channelAlreadyReserved: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, rtmpChannelConfigurationLabel, reservedLabel, rtmpURL, channelAlreadyReserved
							);

							_mmsEngineDBFacade->updateOutputURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, false, rtmpURL
							);
						}
						catch (exception &e)
						{
							LOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}

					// channelAlreadyReserved true means the channel was already reserved, so it is supposed
					// is already started Maybe just start again is not an issue!!! Let's see
					if (!channelAlreadyReserved)
					{
						string cdnName = JSONUtils::as<string>(playURLDetailsRoot, "cdnName", "");
						if (cdnName == "aws")
						{
							json awsRoot = JSONUtils::as<json>(playURLDetailsRoot, "aws", json(nullptr));
							string awsChannelId = JSONUtils::as<string>(awsRoot, "channelId", "");
							if (!awsChannelId.empty())
								awsStartChannel(_encodingItem->_ingestionJobKey, awsChannelId);
						}
					}
				}
				else if (outputType == "SRT_Channel")
				{
					// SrtUrl and PlayUrl fields have to be initialized

					string srtChannelConfigurationLabel = JSONUtils::as<string>(outputRoot, "srtChannelConfigurationLabel", "");

					/*
					string srtChannelType;
					if (srtChannelConfigurationLabel == "")
						srtChannelType = "SHARED";
					else
						srtChannelType = "DEDICATED";
					*/

					// reserveSRTChannel ritorna exception se non ci sono piu
					// canali liberi o quello dedicato è già occupato In caso di
					// ripartenza di mmsEngine, nel caso di richiesta già
					// attiva, ritornerebbe le stesse info associate a
					// ingestionJobKey (senza exception)
					auto [reservedLabel, srtURL, mode, streamId, passphrase, playURL, channelAlreadyReserved] = _mmsEngineDBFacade->reserveSRTChannel(
						_encodingItem->_workspace->_workspaceKey, srtChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					if (mode != "")
					{
						if (srtURL.find('?') == string::npos)
							srtURL += "?";
						else if (srtURL.back() != '?' && srtURL.back() != '&')
							srtURL += "&";

						srtURL += std::format("mode={}", mode);
					}
					if (streamId != "")
					{
						if (srtURL.find('?') == string::npos)
							srtURL += "?";
						else if (srtURL.back() != '?' && srtURL.back() != '&')
							srtURL += "&";

						srtURL += std::format("streamid={}", streamId);
					}
					if (passphrase != "")
					{
						if (srtURL.find('?') == string::npos)
							srtURL += "?";
						else if (srtURL.back() != '?' && srtURL.back() != '&')
							srtURL += "&";

						srtURL += std::format("passphrase={}", passphrase);
					}

					// update outputsRoot with the new details
					{
						field = "srtUrl";
						outputRoot[field] = srtURL;

						// field = "playUrl";
						// outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							LOG_INFO(
								"updateOutputSrtAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", srtChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", srtURL: {}"
								", channelAlreadyReserved: {}"
								", playURL: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, srtChannelConfigurationLabel, reservedLabel, srtURL, channelAlreadyReserved, playURL
							);

							_mmsEngineDBFacade->updateOutputURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, true, srtURL
							);
						}
						catch (exception &e)
						{
							LOG_ERROR(
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

					string hlsChannelConfigurationLabel = JSONUtils::as<string>(outputRoot, "hlsChannelConfigurationLabel", "");

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

							bool liveRecorderVirtualVOD = JSONUtils::as<bool>(_encodingItem->_encodingParametersRoot, "liveRecorderVirtualVOD", false);

							if (liveRecorderVirtualVOD)
							{
								// 10 is the same default used in FFMpeg.cpp
								int localSegmentDurationInSeconds = segmentDurationInSeconds > 0 ? segmentDurationInSeconds : 10;
								int maxDurationInMinutes =
									JSONUtils::as<int32_t>(_encodingItem->_ingestedParametersRoot["liveRecorderVirtualVOD"], "maxDuration", 30);

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
						string otherOutputOptions = JSONUtils::as<string>(outputRoot, field, "");
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
							LOG_INFO(
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
							LOG_ERROR(
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

			killed = liveRecorder_through_ffmpeg();

			if (killed)
			{
				LOG_WARN(
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

				string outputType = JSONUtils::as<string>(outputRoot, "outputType", "");

				if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						json playURLDetailsRoot = _mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
						string cdnName = JSONUtils::as<string>(playURLDetailsRoot, "cdnName", "");
						if (cdnName == "aws")
						{
							json awsRoot = JSONUtils::as<json>(playURLDetailsRoot, "aws", json(nullptr));
							string awsChannelId = JSONUtils::as<string>(awsRoot, "channelId", "");
							if (!awsChannelId.empty())
								awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
						}
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
						LOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "SRT_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseSRTChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseSRTChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						LOG_ERROR(errorMessage);
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
						LOG_ERROR(errorMessage);
					}
				}
			}
		}
		catch (...)
		{
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::as<string>(outputRoot, "outputType", "");

				if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						json playURLDetailsRoot = _mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
						string cdnName = JSONUtils::as<string>(playURLDetailsRoot, "cdnName", "");
						if (cdnName == "aws")
						{
							json awsRoot = JSONUtils::as<json>(playURLDetailsRoot, "aws", json(nullptr));
							string awsChannelId = JSONUtils::as<string>(awsRoot, "channelId", "");
							if (!awsChannelId.empty())
								awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
						}
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
						LOG_ERROR(errorMessage);
					}
				}
				else if (outputType == "SRT_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseSRTChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						string errorMessage = std::format(
							"releaseSRTChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
						LOG_ERROR(errorMessage);
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
						LOG_ERROR(errorMessage);
					}
				}
			}

			// throw the same received exception
			throw;
		}

		return killed;
	}
}

bool EncoderProxy::liveRecorder_through_ffmpeg()
{
	string streamSourceType;
	string encodersPool;
	{
		streamSourceType = JSONUtils::as<string>(_encodingItem->_encodingParametersRoot, "streamSourceType", "");
		encodersPool = JSONUtils::as<string>(_encodingItem->_encodingParametersRoot, "encodersPoolLabel", "");
	}

	bool timePeriod = true;
	string ipPushStreamConfigurationLabel;
	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	{
		if (streamSourceType == "IP_PUSH")
			ipPushStreamConfigurationLabel = JSONUtils::as<string>(_encodingItem->_ingestedParametersRoot, "configurationLabel", "");

		string field = "schedule";
		json recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)[field];

		field = "start";
		if (!JSONUtils::isPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::as<string>(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = Datetime::parseUtcStringToUtcInSecs(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isPresent(recordingPeriodRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, field
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodEnd = JSONUtils::as<string>(recordingPeriodRoot, field, "");
		utcRecordingPeriodEnd = Datetime::parseUtcStringToUtcInSecs(recordingPeriodEnd);

		field = "autoRenew";
		autoRenew = JSONUtils::as<bool>(recordingPeriodRoot, field, false);
	}

	long maxAttemptsNumberInCaseOfErrors = 5;

	return waitingLiveProxyOrLiveRecorder(
		MMSEngineDBFacade::EncodingType::LiveRecorder, _ffmpegLiveRecorderURI, timePeriod, utcRecordingPeriodStart, utcRecordingPeriodEnd,
		maxAttemptsNumberInCaseOfErrors, ipPushStreamConfigurationLabel
	);
}

void EncoderProxy::processLiveRecorder(bool killed)
{
	try
	{
		// Status will be success if at least one Chunk was generated, otherwise
		// it will be failed
		{
			string errorMessage;
			string processorMMS;
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

			LOG_INFO(
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
		LOG_ERROR(
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
		LOG_ERROR(
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
