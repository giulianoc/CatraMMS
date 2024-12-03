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
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <cstdint>
#include <regex>

bool EncoderProxy::liveProxy(string proxyType)
{

	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	{
		string field = "inputsRoot";
		json inputsRoot = (_encodingItem->_encodingParametersRoot)[field];

		if (inputsRoot == nullptr || inputsRoot.size() == 0)
		{
			string errorMessage = __FILEREF__ + "No inputsRoot are present" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", inputsRoot.size: " + to_string(inputsRoot.size());
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			json firstInputRoot = inputsRoot[0];

			field = "timePeriod";
			timePeriod = JSONUtils::asBool(firstInputRoot, field, false);

			if (timePeriod)
			{
				field = "utcScheduleStart";
				utcProxyPeriodStart = JSONUtils::asInt64(firstInputRoot, field, -1);
			}

			json lastInputRoot = inputsRoot[inputsRoot.size() - 1];

			field = "timePeriod";
			timePeriod = JSONUtils::asBool(lastInputRoot, field, false);

			if (timePeriod)
			{
				field = "utcScheduleEnd";
				utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, field, -1);
			}
		}
	}

	if (timePeriod)
	{
		time_t utcNow;

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		// MMS allocates a thread just 5 minutes before the beginning of the
		// recording
		if (utcNow < utcProxyPeriodStart)
		{
			if (utcProxyPeriodStart - utcNow >= _timeBeforeToPrepareResourcesInMinutes * 60)
			{
				_logger->info(
					__FILEREF__ + "Too early to allocate a thread for proxing" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", utcProyPeriodStart - utcNow: " + to_string(utcProxyPeriodStart - utcNow) +
					", _timeBeforeToPrepareResourcesInSeconds: " + to_string(_timeBeforeToPrepareResourcesInMinutes * 60)
				);

				// it is simulated a MaxConcurrentJobsReached to avoid
				// to increase the error counter
				throw MaxConcurrentJobsReached();
			}
		}

		if (utcProxyPeriodEnd <= utcNow)
		{
			string errorMessage = __FILEREF__ + "Too late to activate the proxy" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) + ", utcNow: " + to_string(utcNow);
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
								// + ", encodingParameters: " +
								// encodingParameters
							);

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
					int segmentDuration;
					int playlistEntriesNumber;
					bool channelAlreadyReserved;
					tie(reservedLabel, deliveryCode, segmentDuration, playlistEntriesNumber, channelAlreadyReserved) = hlsChannelDetails;

					// update outputsRoot with the new details
					{
						field = "deliveryCode";
						outputRoot[field] = deliveryCode;

						if (segmentDuration > 0)
						{
							// if not present, default is decided by the encoder
							field = "segmentDurationInSeconds";
							outputRoot[field] = segmentDuration;
						}

						if (playlistEntriesNumber > 0)
						{
							// if not present, default is decided by the encoder
							field = "playlistEntriesNumber";
							outputRoot[field] = playlistEntriesNumber;
						}

						string manifestDirectoryPath = _mmsStorage->getLiveDeliveryAssetPath(to_string(deliveryCode), _encodingItem->_workspace);
						string manifestFileName = to_string(deliveryCode) + ".m3u8";

						field = "manifestDirectoryPath";
						outputRoot[field] = manifestDirectoryPath;

						field = "manifestFileName";
						outputRoot[field] = manifestFileName;

						field = "otherOutputOptions";
						string otherOutputOptions = JSONUtils::asString(outputRoot, field, "");

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
								", deliveryCode: " + to_string(deliveryCode) + ", segmentDuration: " + to_string(segmentDuration) +
								", playlistEntriesNumber: " + to_string(playlistEntriesNumber) + ", manifestDirectoryPath: " + manifestDirectoryPath +
								", manifestFileName: " + manifestFileName + ", channelAlreadyReserved: " + to_string(channelAlreadyReserved)
							);

							_mmsEngineDBFacade->updateOutputHLSDetails(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, deliveryCode, segmentDuration,
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

			killedByUser = liveProxy_through_ffmpeg(proxyType);

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

			if (killedByUser)
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

bool EncoderProxy::liveProxy_through_ffmpeg(string proxyType)
{

	string url;
	long waitingSecondsBetweenAttemptsInCaseOfErrors;
	long maxAttemptsNumberInCaseOfErrors;
	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	string streamSourceType;

	// string field = "inputsRoot";
	json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];

	json firstInputRoot = inputsRoot[0];

	string field;
	if (proxyType == "vodProxy")
		field = "vodInput";
	else if (proxyType == "liveProxy")
		field = "streamInput";
	else if (proxyType == "countdownProxy")
		field = "countdownInput";
	json streamInputRoot = firstInputRoot[field];

	{
		if (proxyType == "liveProxy")
		{
			url = JSONUtils::asString(streamInputRoot, "url", "");

			streamSourceType = JSONUtils::asString(streamInputRoot, "streamSourceType", "");
		}

		waitingSecondsBetweenAttemptsInCaseOfErrors =
			JSONUtils::asInt(_encodingItem->_encodingParametersRoot, "waitingSecondsBetweenAttemptsInCaseOfErrors", 600);

		maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(_encodingItem->_ingestedParametersRoot, "maxAttemptsNumberInCaseOfErrors", -1);

		{
			timePeriod = JSONUtils::asBool(firstInputRoot, "timePeriod", false);

			if (timePeriod)
				utcProxyPeriodStart = JSONUtils::asInt64(firstInputRoot, "utcScheduleStart", -1);

			json lastInputRoot = inputsRoot[inputsRoot.size() - 1];

			timePeriod = JSONUtils::asBool(lastInputRoot, "timePeriod", false);

			if (timePeriod)
			{
				utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, "utcScheduleEnd", -1);
				if (utcProxyPeriodEnd == -1)
					utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, "utcProxyPeriodEnd", -1);
			}
		}
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	SPDLOG_INFO(
		"check maxAttemptsNumberInCaseOfErrors"
		", ingestionJobKey: {}"
		", encodingJobKey: {}"
		", maxAttemptsNumberInCaseOfErrors: {}",
		_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, maxAttemptsNumberInCaseOfErrors
	);

	long currentAttemptsNumberInCaseOfErrors = 0;

	bool alwaysRetry = false;

	// long encodingStatusFailures = 0;
	if (maxAttemptsNumberInCaseOfErrors == -1)
	{
		// 2022-07-20: -1 means we always has to retry, so we will reset
		// encodingStatusFailures to 0
		alwaysRetry = true;

		// 2022-07-20: this is to allow the next loop to exit after 2 errors
		maxAttemptsNumberInCaseOfErrors = 2;

		// 2020-04-19: Reset encodingStatusFailures into DB.
		// That because if we comes from an error/exception
		//	encodingStatusFailures is > than 0 but we consider here like
		//	it is 0 because our variable is set to 0
		try
		{
			SPDLOG_INFO(
				"updateEncodingJobFailuresNumber"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				// + ", encodingStatusFailures: " +
				// to_string(encodingStatusFailures)
				", currentAttemptsNumberInCaseOfErrors: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
			);

			killedByUser = _mmsEngineDBFacade->updateEncodingJobFailuresNumber(
				_encodingItem->_encodingJobKey,
				// encodingStatusFailures
				currentAttemptsNumberInCaseOfErrors
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJobFailuresNumber FAILED" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey)
				// + ", encodingStatusFailures: " +
				// to_string(encodingStatusFailures)
				+ ", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors)
			);
		}
	}

	// 2020-03-11: we saw the following scenarios:
	//	1. ffmpeg was running
	//	2. after several hours it failed (1:34 am)
	//	3. our below loop tried again and this new attempt returned 404 URL NOT
	// FOUND
	//	4. we exit from this loop
	//	5. crontab started again it after 15 minutes
	//	In this scenarios, we have to retry again without waiting the crontab
	// check
	// 2020-03-12: Removing the urlNotFound management generated duplication of
	// ffmpeg process
	//	For this reason we rollbacked as it was before
	// 2021-05-29: LiveProxy has to exit if:
	//	- was killed OR
	//	- if timePeriod true
	//		- no way to exit (we have to respect the timePeriod)
	//	- if timePeriod false
	//		- exit if too many error or urlForbidden or urlNotFound
	time_t utcNowCheckToExit = 0;
	// while (!killedByUser && !urlForbidden && !urlNotFound
	// check on currentAttemptsNumberInCaseOfErrors is done only if there is no
	// timePeriod
	// 	&& (timePeriod || currentAttemptsNumberInCaseOfErrors <
	// maxAttemptsNumberInCaseOfErrors)
	// )
	while (! // while we are NOT in the exit condition
		   (
			   // exit condition
			   killedByUser ||
			   (!timePeriod && (urlForbidden || urlNotFound || currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors))
		   ))
	{
		if (timePeriod)
		{
			if (utcNowCheckToExit >= utcProxyPeriodEnd)
				break;
			else
				_logger->info(
					__FILEREF__ + "check to exit" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", still miss (secs): " + to_string(utcProxyPeriodEnd - utcNowCheckToExit)
				);
		}

		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveProxyURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			_currentUsedFFMpegExternalEncoder = false;

			if (_encodingItem->_encoderKey == -1)
			{
				// 2021-12-14: we have to read again encodingParametersRoot because, in case the playlist (inputsRoot) is changed, the
				// updated inputsRoot is into DB
				{
					try
					{
						// 2022-12-18: fromMaster true because the inputsRoot maybe was just updated (modifying the playlist)
						_encodingItem->_encodingParametersRoot = _mmsEngineDBFacade->encodingJob_Parameters(_encodingItem->_encodingJobKey, true);

						// 2024-12-01: ricarichiamo ingestedParameters perchè potrebbe essere stato modificato con un nuovo 'encodersDetails' (nello
						// scenario in cui si vuole eseguire lo switch di un ingestionjob su un nuovo encoder)
						_encodingItem->_ingestedParametersRoot = _mmsEngineDBFacade->ingestionJob_MetadataContent(
							_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, true
						);
					}
					catch (DBRecordNotFound &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
				}

				// IN ingestionJob->metadataParameters abbiamo già il campo encodersPool.
				// Aggiungiamo encoderKey nel caso di IP_PUSH in modo da avere un posto unico (ingestionJob->metadataParameters)
				// per questa informazione
				if (streamSourceType == "IP_PUSH")
				{
					// scenario:
					// 	- viene configurato uno Stream per un IP_PUSH su un encoder specifico
					// 	- questo encoder ha un fault e va giu
					// 	- finche questo encode non viene ripristinato (dipende da Hetzner) abbiamo un outage
					// 	- Per evitare l'outage, posso io cambiare encoder nella configurazione dello Stream
					// 	- La getStreamInputPushDetails sotto mi serve in questo loop per recuperare avere l'encoder aggiornato configurato nello
					// Stream 		altrimenti rimarremmo con l'encoder e l'url calcolata all'inizio e non potremmo evitare l'outage
					// 2024-06-25: In uno scenario di Broadcaster e Broadcast, il cambiamento descritto sopra
					// 		risolve il problema del broadcaster ma non quello del broadcast. Infatti il broadcast ha il campo udpUrl
					// 		nell'outputRoot che punta al transcoder iniziale. Questo campo udpUrl è stato inizializzato
					// 		in CatraMMSBroadcaster.java (method: addBroadcaster).

					int64_t updatedPushEncoderKey = -1;
					string updatedUrl;
					{
						json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];

						// se è IP_PUSH vuol dire anche che siamo nel caso di proxyType == "liveProxy" che quindi ha il campo streamInput
						json streamInputRoot = inputsRoot[0]["streamInput"];

						string streamConfigurationLabel = JSONUtils::asString(streamInputRoot, "configurationLabel", "");

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
								updatedPushEncoderKey, pushPublicEncoderName
							);
						}
					}

					streamInputRoot["pushEncoderKey"] = updatedPushEncoderKey;
					streamInputRoot["url"] = updatedUrl;
					inputsRoot[0]["streamInput"] = streamInputRoot;
					(_encodingItem->_encodingParametersRoot)["inputsRoot"] = inputsRoot;

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
						"LiveProxy. Retrieved updated Stream info"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", updatedPushEncoderKey: {}"
						", updatedUrl: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, updatedPushEncoderKey, updatedUrl
					);
				}
				else
				{
					string encodersPool;
					if (proxyType == "vodProxy" || proxyType == "countdownProxy")
					{
						// both vodProxy and countdownProxy work with VODs and
						// the encodersPool is defined by the ingestedParameters field
						encodersPool = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, "encodersPool", "");
					}
					else
					{
						json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];

						json streamInputRoot = inputsRoot[0]["streamInput"];

						json internalMMSRoot = JSONUtils::asJson(_encodingItem->_ingestedParametersRoot, "internalMMS", nullptr);
						json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails", nullptr);
						if (encodersDetailsRoot == nullptr)
							encodersPool = JSONUtils::asString(streamInputRoot, "encodersPoolLabel", "");
						else // questo quello corretto, l'if sopra dovrebbe essere eliminato
							encodersPool = JSONUtils::asString(encodersDetailsRoot, "encodersPoolLabel", string());
					}

					int64_t encoderKeyToBeSkipped = -1;
					bool externalEncoderAllowed = true;
					tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) =
						_encodersLoadBalancer->getEncoderURL(
							_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace, encoderKeyToBeSkipped, externalEncoderAllowed
						);
				}

				SPDLOG_INFO(
					"Configuration item"
					", _proxyIdentifier: {}"
					", _currentUsedFFMpegEncoderHost: {}"
					", _currentUsedFFMpegEncoderKey: {}",
					_proxyIdentifier, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
				);
				ffmpegEncoderURL = fmt::format(
					"{}{}/{}/{}", _currentUsedFFMpegEncoderHost, ffmpegURI, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);

				string body;
				{
					json liveProxyMetadata;

					// 2023-03-21: rimuovere il parametro ingestionJobKey se il
					// trascoder deployed è > 1.0.5315
					liveProxyMetadata["ingestionJobKey"] = _encodingItem->_ingestionJobKey;
					liveProxyMetadata["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
					liveProxyMetadata["liveURL"] = url;
					liveProxyMetadata["ingestedParametersRoot"] = _encodingItem->_ingestedParametersRoot;
					liveProxyMetadata["encodingParametersRoot"] = _encodingItem->_encodingParametersRoot;

					body = JSONUtils::toString(liveProxyMetadata);
				}

				SPDLOG_INFO(
					"LiveProxy. Selection of the transcoder. The transcoder is selected by load balancer"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", transcoder: {}"
					", _currentUsedFFMpegEncoderKey: {}"
					", body: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost,
					_currentUsedFFMpegEncoderKey, body
				);

				vector<string> otherHeaders;
				json liveProxyContentResponse;
				try
				{
					liveProxyContentResponse = MMSCURL::httpPostStringAndGetJson(
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
						// Questo scenario indica che per il DB "l'encoding è da eseguire" mentre abbiamo un Encoder che lo sta già
						// eseguendo Si tratta di una inconsistenza che non dovrebbe mai accadere. Oggi pero' ho visto questo
						// scenario e l'ho risolto facendo ripartire sia l'encoder che gli engines Gestire questo scenario
						// rende il sistema piu' robusto e recupera facilmente una situazione che altrimenti richiederebbe una
						// gestione manuale Inoltre senza guardare nel log, non si riuscirebbe a capire che siamo in questo scenario.

						// La gestione di questo scenario consiste nell'ignorare questa eccezione facendo andare avanti la procedura,
						// come se non avesse generato alcun errore
						_logger->error(
							__FILEREF__ +
							"inconsistency: DB says the encoding has to be "
							"executed but the Encoder is already executing it. "
							"We will manage it" +
							", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
							to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
							", body: " + regex_replace(body, regex("\n"), " ") + ", e.what: " + e.what()
						);
					}
					else
						throw e;
				}
			}
			else
			{
				SPDLOG_INFO(
					"LiveProxy. Selection of the transcoder. The transcoder is already saved (DB), the encoding should be already running"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", encoderKey: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_encoderKey
				);

				tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

				// we have to reset _encodingItem->_encoderKey because in case we will come back in the above 'while' loop, we have to
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
				__FILEREF__ + "Update EncodingJob" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", transcoder: " + _currentUsedFFMpegEncoderHost +
				", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			if (timePeriod)
				;
			else
			{
				// encodingProgress: fixed to -1 (LIVE)

				try
				{
					double encodingProgress = -1.0;

					_logger->info(
						__FILEREF__ + "updateEncodingJobProgress" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encodingProgress: " + to_string(encodingProgress)
					);
					_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
				}
				catch (runtime_error &e)
				{
					_logger->error(
						__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
					);
				}
				catch (exception &e)
				{
					_logger->error(
						__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}
			}

			// loop waiting the end of the encoding
			bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
			// string lastRecordedAssetFileName;
			chrono::system_clock::time_point startCheckingEncodingStatus = chrono::system_clock::now();

			int encoderNotReachableFailures = 0;
			int lastEncodingPid = 0;
			long lastRealTimeFrameRate = 0;
			double lastRealTimeBitRate = 0;
			int encodingPid;
			long realTimeFrameRate;
			double realTimeBitRate;
			long lastNumberOfRestartBecauseOfFailure = 0;
			long numberOfRestartBecauseOfFailure;

			_logger->info(
				__FILEREF__ + "starting loop" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encodingFinished: " + to_string(encodingFinished) +
				", encoderNotReachableFailures: " + to_string(encoderNotReachableFailures) +
				", _maxEncoderNotReachableFailures: " + to_string(_maxEncoderNotReachableFailures) +
				", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors) +
				", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors) + ", alwaysRetry: " + to_string(alwaysRetry)
			);

			// 2020-11-28: the next while, it was added encodingStatusFailures
			// condition because,
			//  in case the transcoder is down (once I had to upgrade his
			//  operative system), the engine has to select another encoder and
			//  not remain in the next loop indefinitely
			while (!(
				encodingFinished || encoderNotReachableFailures >= _maxEncoderNotReachableFailures
				// || currentAttemptsNumberInCaseOfErrors >=
				// maxAttemptsNumberInCaseOfErrors
			))
			// while(!encodingFinished)
			{
				_logger->info(
					__FILEREF__ + "sleep_for" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);
				this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

				try
				{
					// tuple<bool, bool, bool, string, bool, bool, double, int>
					// encodingStatus = getEncodingStatus(
					/* _encodingItem->_encodingJobKey */
					// );
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage, urlForbidden, urlNotFound, ignore, encodingPid,
						realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure) = getEncodingStatus();
					SPDLOG_INFO(
						"getEncodingStatus"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", currentAttemptsNumberInCaseOfErrors: {}"
						", maxAttemptsNumberInCaseOfErrors: {}"
						", encodingFinished: {}"
						", killedByUser: {}"
						", completedWithError: {}"
						", urlForbidden: {}"
						", urlNotFound: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors,
						maxAttemptsNumberInCaseOfErrors, encodingFinished, killedByUser, completedWithError, urlForbidden, urlNotFound
					);

					encoderNotReachableFailures = 0;

					// health check and retention is done by ffmpegEncoder.cpp

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

					if (completedWithError) // || chunksWereNotGenerated)
					{
						if (urlForbidden || urlNotFound)
						// see my comment at the beginning of the while loop
						{
							string errorMessage = __FILEREF__ +
												  "Encoding failed because of URL Forbidden or "
												  "Not Found (look the Transcoder logs)" +
												  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
												  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
												  ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost +
												  ", encodingErrorMessage: " + encodingErrorMessage;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						currentAttemptsNumberInCaseOfErrors++;

						string errorMessage = __FILEREF__ + "Encoding failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
											  ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost +
											  ", encodingErrorMessage: " + regex_replace(encodingErrorMessage, regex("\n"), " ");
						_logger->error(errorMessage);

						if (alwaysRetry)
						{
							// encodingStatusFailures++;

							// in this scenario encodingFinished is true

							_logger->info(
								__FILEREF__ + "Start waiting loop for the next call" + ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							);

							chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
							chrono::system_clock::time_point now;
							do
							{
								_logger->info(
									__FILEREF__ + "sleep_for" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									", "
									"_intervalInSecondsToCheckEncodingFinished:"
									" " +
									to_string(_intervalInSecondsToCheckEncodingFinished) +
									", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors) +
									", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors)
									// + ", encodingStatusFailures: " +
									// to_string(encodingStatusFailures)
								);
								// 2021-02-12: moved sleep here because, in this
								// case, if the task was killed during the
								// sleep, it will check that. Before the sleep
								// was after the check, so when the sleep is
								// finished, the flow will go out of the loop
								// and no check is done and Task remains up even
								// if user kiiled it.
								this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

								// update EncodingJob failures number to notify
								// the GUI EncodingJob is failing
								try
								{
									bool isKilled = _mmsEngineDBFacade->updateEncodingJobFailuresNumber(
										_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
									);

									_logger->info(
										__FILEREF__ +
										"check and update encodingJob "
										"FailuresNumber" +
										", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
										", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
										", "
										"currentAttemptsNumberInCaseOfErrors:"
										" " +
										to_string(currentAttemptsNumberInCaseOfErrors) + ", isKilled: " + to_string(isKilled)
									);

									if (isKilled)
									{
										_logger->info(
											__FILEREF__ +
											"LiveProxy Killed by user during "
											"waiting loop" +
											", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
											", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										);

										// when previousEncodingStatusFailures
										// is < 0 means:
										// 1. the live proxy is not starting
										// (ffmpeg is generating continuously an
										// error)
										// 2. User killed the encoding through
										// MMS GUI or API
										// 3. the kill procedure (in API module)
										// was not able to kill the ffmpeg
										// process,
										//		because it does not exist the
										// process and set the failuresNumber DB
										// field 		to a negative value in order to
										// communicate with this thread
										// 4. This thread, when it finds a
										// negative failuresNumber, knows the
										// encoding
										//		was killed and exit from the
										// loop
										encodingFinished = true;
										killedByUser = true;
									}
								}
								catch (...)
								{
									_logger->error(
										__FILEREF__ +
										"updateEncodingJobFailuresNumber "
										"FAILED" +
										", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
										", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									);
								}

								now = chrono::system_clock::now();
							} while (chrono::duration_cast<chrono::seconds>(now - startWaiting) <
										 chrono::seconds(waitingSecondsBetweenAttemptsInCaseOfErrors) &&
									 (timePeriod || currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors) && !killedByUser);
						}

						// if (chunksWereNotGenerated)
						// 	encodingFinished = true;

						throw runtime_error(errorMessage);
					}
					else
					{
						// ffmpeg is running successful,
						// we will make sure currentAttemptsNumberInCaseOfErrors
						// is reset
						currentAttemptsNumberInCaseOfErrors = 0;

						// if (encodingStatusFailures > 0)
						{
							try
							{
								// update EncodingJob failures number to notify
								// the GUI encodingJob is successful
								// encodingStatusFailures = 0;

								_logger->info(
									__FILEREF__ + "updateEncodingJobFailuresNumber" + ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors)
								);

								int64_t mediaItemKey = -1;
								int64_t encodedPhysicalPathKey = -1;
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber(
									_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
								);
							}
							catch (...)
							{
								_logger->error(
									__FILEREF__ + "updateEncodingJobFailuresNumber FAILED" + ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								);
							}
						}
					}

					// encodingProgress/encodingPid
					{
						if (timePeriod)
						{
							time_t utcNow;

							{
								chrono::system_clock::time_point now = chrono::system_clock::now();
								utcNow = chrono::system_clock::to_time_t(now);
							}

							double encodingProgress;

							if (utcNow < utcProxyPeriodStart)
								encodingProgress = 0.0;
							else if (utcProxyPeriodStart < utcNow && utcNow < utcProxyPeriodEnd)
							{
								double elapsed = utcNow - utcProxyPeriodStart;
								double proxyPeriod = utcProxyPeriodEnd - utcProxyPeriodStart;
								encodingProgress = (elapsed * 100) / proxyPeriod;
							}
							else
								encodingProgress = 100.0;

							try
							{
								_logger->info(
									__FILEREF__ + "updateEncodingJobProgress" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encodingProgress: " +
									to_string(encodingProgress) + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) +
									", utcNow: " + to_string(utcNow) + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
								);
								_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
							}
							catch (runtime_error &e)
							{
								_logger->error(
									__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									", encodingProgress: " + to_string(encodingProgress) + ", e.what(): " + e.what()
								);
							}
							catch (exception &e)
							{
								_logger->error(
									__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									", encodingProgress: " + to_string(encodingProgress)
								);
							}
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
						else
						{
							SPDLOG_INFO(
								"encoderPid/bitrate/framerate check, not changed"
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
				catch (EncoderNotReachable &e)
				{
					encoderNotReachableFailures++;

					// 2020-11-23. Scenario:
					//	1. I shutdown the encoder because I had to upgrade OS
					// version
					//	2. this thread remained in this loop
					//(while(!encodingFinished)) 		and the channel did not work
					// until the Encoder was working again 	In this scenario, so
					// when the encoder is not reachable at all, the engine 	has
					// to select a new encoder. 	For this reason we added this
					// EncoderNotReachable catch 	and the
					// encoderNotReachableFailures variable

					_logger->error(
						__FILEREF__ +
						"Transcoder is not reachable at all, let's select "
						"another encoder" +
						", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
						to_string(_encodingItem->_encodingJobKey) + ", encoderNotReachableFailures: " + to_string(encoderNotReachableFailures) +
						", _maxEncoderNotReachableFailures: " + to_string(_maxEncoderNotReachableFailures) + ", _currentUsedFFMpegEncoderHost: " +
						_currentUsedFFMpegEncoderHost + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
					);
				}
				catch (...)
				{
					encoderNotReachableFailures++;

					// 2020-11-23. Scenario:
					//	1. I shutdown the encoder because I had to upgrade OS
					// version
					//	2. this thread remained in this loop
					//(while(!encodingFinished)) 		and the channel did not work
					// until the Encoder was working again 	In this scenario, so
					// when the encoder is not reachable at all, the engine 	has
					// to select a new encoder. 	For this reason we added this
					// EncoderNotReachable catch 	and the
					// encoderNotReachableFailures variable

					_logger->error(
						__FILEREF__ + "getEncodingStatus failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encoderNotReachableFailures: " +
						to_string(encoderNotReachableFailures) + ", _maxEncoderNotReachableFailures: " + to_string(_maxEncoderNotReachableFailures) +
						", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost +
						", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
					);
				}
			}

			chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

			utcNowCheckToExit = chrono::system_clock::to_time_t(endEncoding);

			if (timePeriod)
			{
				if (utcNowCheckToExit < utcProxyPeriodEnd)
				{
					_logger->error(
						__FILEREF__ + "LiveProxy media file completed unexpected" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
						", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
						", still remaining seconds (utcProxyPeriodEnd - "
						"utcNow): " +
						to_string(utcProxyPeriodEnd - utcNowCheckToExit) + ", ffmpegEncoderURL: " + ffmpegEncoderURL + ", encodingFinished: " +
						to_string(encodingFinished)
						// + ", encodingStatusFailures: " +
						// to_string(encodingStatusFailures)
						+ ", killedByUser: " + to_string(killedByUser) + ", @MMS statistics@ - encodingDuration (secs): @" +
						to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@" +
						", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
					);

					try
					{
						char strDateTime[64];
						{
							time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
							tm tmDateTime;
							localtime_r(&utcTime, &tmDateTime);
							sprintf(
								strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
								tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
							);
						}
						string errorMessage = string(strDateTime) + " LiveProxy media file completed unexpected";

						string firstLineOfErrorMessage;
						{
							string firstLine;
							stringstream ss(errorMessage);
							if (getline(ss, firstLine))
								firstLineOfErrorMessage = firstLine;
							else
								firstLineOfErrorMessage = errorMessage;
						}

						_mmsEngineDBFacade->appendIngestionJobErrorMessage(_encodingItem->_ingestionJobKey, firstLineOfErrorMessage);
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
				else
				{
					_logger->info(
						__FILEREF__ + "LiveProxy media file completed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
						", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
						", encodingFinished: " + to_string(encodingFinished) + ", killedByUser: " + to_string(killedByUser) +
						", @MMS statistics@ - encodingDuration (secs): @" +
						to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@" +
						", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
					);
				}
			}
			else
			{
				_logger->error(
					__FILEREF__ + "LiveProxy media file completed unexpected" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
					", encodingFinished: " + to_string(encodingFinished) + ", killedByUser: " + to_string(killedByUser) +
					", @MMS statistics@ - encodingDuration (secs): @" +
					to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@" +
					", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);

				try
				{
					char strDateTime[64];
					{
						time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
						tm tmDateTime;
						localtime_r(&utcTime, &tmDateTime);
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
						);
					}
					string errorMessage = string(strDateTime) + " LiveProxy media file completed unexpected";

					string firstLineOfErrorMessage;
					{
						string firstLine;
						stringstream ss(errorMessage);
						if (getline(ss, firstLine))
							firstLineOfErrorMessage = firstLine;
						else
							firstLineOfErrorMessage = errorMessage;
					}

					_mmsEngineDBFacade->appendIngestionJobErrorMessage(_encodingItem->_ingestionJobKey, firstLineOfErrorMessage);
				}
				catch (runtime_error &e)
				{
					_logger->error(
						__FILEREF__ + "appendIngestionJobErrorMessage failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
					);
				}
				catch (exception &e)
				{
					_logger->error(
						__FILEREF__ + "appendIngestionJobErrorMessage failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}
			}
		}
		catch (YouTubeURLNotRetrieved &e)
		{
			string errorMessage = string("YouTubeURLNotRetrieved") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", response.str(): " + (responseInitialized ? response.str() : "") + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			// in this case we will through the exception independently
			// if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
			throw e;
		}
		catch (EncoderNotFound e)
		{
			_logger->error(
				__FILEREF__ + "Encoder not found" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			// update EncodingJob failures number to notify the GUI EncodingJob
			// is failing
			try
			{
				// 2021-02-12: scenario, encodersPool does not exist, a
				// runtime_error is generated contiuosly. The task will never
				// exist from this loop because
				// currentAttemptsNumberInCaseOfErrors always remain to 0 and
				// the main loop look currentAttemptsNumberInCaseOfErrors. So
				// added currentAttemptsNumberInCaseOfErrors++
				currentAttemptsNumberInCaseOfErrors++;

				_logger->info(
					__FILEREF__ + "updateEncodingJobFailuresNumber" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
			}
			catch (...)
			{
				_logger->error(
					__FILEREF__ + "updateEncodingJobFailuresNumber FAILED" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

			// throw e;
		}
		catch (MaxConcurrentJobsReached &e)
		{
			string errorMessage = string("MaxConcurrentJobsReached") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", response.str(): " + (responseInitialized ? response.str() : "") + ", e.what(): " + e.what();
			_logger->warn(__FILEREF__ + errorMessage);

			// in this case we will through the exception independently
			// if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
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
					__FILEREF__ + "Encoding URL failed/runtime_error" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
					", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				// update EncodingJob failures number to notify the GUI
				// EncodingJob is failing
				try
				{
					// 2021-02-12: scenario, encodersPool does not exist, a
					// runtime_error is generated contiuosly. The task will
					// never exist from this loop because
					// currentAttemptsNumberInCaseOfErrors always remain to 0
					// and the main loop look
					// currentAttemptsNumberInCaseOfErrors. So added
					// currentAttemptsNumberInCaseOfErrors++
					currentAttemptsNumberInCaseOfErrors++;

					_logger->info(
						__FILEREF__ + "updateEncodingJobFailuresNumber" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
						", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors)
					);

					int64_t mediaItemKey = -1;
					int64_t encodedPhysicalPathKey = -1;
					_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
				}
				catch (...)
				{
					_logger->error(
						__FILEREF__ + "updateEncodingJobFailuresNumber FAILED" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}

				// sleep a bit and try again
				int sleepTime = 30;
				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNowCheckToExit = chrono::system_clock::to_time_t(now);
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

			// update EncodingJob failures number to notify the GUI EncodingJob
			// is failing
			try
			{
				currentAttemptsNumberInCaseOfErrors++;
				// encodingStatusFailures++;

				_logger->info(
					__FILEREF__ + "updateEncodingJobFailuresNumber" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
			}
			catch (...)
			{
				_logger->error(
					__FILEREF__ + "updateEncodingJobFailuresNumber FAILED" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

			// throw e;
		}
	}

	if (urlForbidden)
	{
		string errorMessage = __FILEREF__ + "LiveProxy: URL forbidden" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
		_logger->error(errorMessage);

		throw FFMpegURLForbidden();
	}
	else if (urlNotFound)
	{
		string errorMessage = __FILEREF__ + "LiveProxy: URL Not Found" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
		_logger->error(errorMessage);

		throw FFMpegURLNotFound();
	}
	else if (currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)
	{
		string errorMessage = __FILEREF__ + "Reached the max number of attempts to the URL" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
							  ", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors) +
							  ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors);
		_logger->error(errorMessage);

		throw EncoderError();
	}

	return killedByUser;
}

void EncoderProxy::processLiveProxy(bool killedByUser)
{
	try
	{
		// In case of Liveproxy where TimePeriod is false, this method is never
		// called because,
		//	in both the scenarios below, an exception by
		// EncoderProxy::liveProxy will be raised:
		// - transcoding killed by the user
		// - The max number of calls to the URL were all done and all failed
		//
		// In case of LiveProxy where TimePeriod is true, this method is called

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
			__FILEREF__ + "processLiveProxy failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName + ", e.what(): " + e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "processLiveProxy failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
		);

		throw e;
	}
}
