/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   CURL.cpp
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#include "MMSDeliveryAuthorization.h"

#include "AWSSigner.h"
#include "JSONUtils.h"
#include "catralibraries/Convert.h"
#include "spdlog/fmt/fmt.h"
// #include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

MMSDeliveryAuthorization::MMSDeliveryAuthorization(
	json configuration, shared_ptr<MMSStorage> mmsStorage, shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<spdlog::logger> logger
)
{
	_configuration = configuration;
	_mmsStorage = mmsStorage;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_logger = logger;

	_keyPairId = JSONUtils::asString(_configuration["aws"], "keyPairId", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->keyPairId: " + _keyPairId);
	_privateKeyPEMPathName = JSONUtils::asString(_configuration["aws"], "privateKeyPEMPathName", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->privateKeyPEMPathName: " + _privateKeyPEMPathName);
	_vodCloudFrontHostNamesRoot = _configuration["aws"]["vodCloudFrontHostNames"];
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->vodCloudFrontHostNames: " + "...");

	json api = _configuration["api"];

	_deliveryProtocol = JSONUtils::asString(api["delivery"], "deliveryProtocol", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", api->delivery->deliveryProtocol: " + _deliveryProtocol);
	_deliveryHost_authorizationThroughParameter = JSONUtils::asString(api["delivery"], "deliveryHost_authorizationThroughParameter", "");
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", api->delivery->deliveryHost_authorizationThroughParameter: " + _deliveryHost_authorizationThroughParameter
	);
	_deliveryHost_authorizationThroughPath = JSONUtils::asString(api["delivery"], "deliveryHost_authorizationThroughPath", "");
	_logger->info(
		__FILEREF__ + "Configuration item" + ", api->delivery->deliveryHost_authorizationThroughPath: " + _deliveryHost_authorizationThroughPath
	);
}

pair<string, string> MMSDeliveryAuthorization::createDeliveryAuthorization(
	int64_t userKey, shared_ptr<Workspace> requestWorkspace, string clientIPAddress,

	int64_t mediaItemKey, string uniqueName, int64_t encodingProfileKey, string encodingProfileLabel,

	int64_t physicalPathKey,

	int64_t ingestionJobKey, int64_t deliveryCode,

	int ttlInSeconds, int maxRetries,

	bool save,
	// deliveryType:
	// MMS_Token: delivery by MMS with a Token
	// MMS_SignedToken: delivery by MMS with a signed URL
	// AWSCloudFront: delivery by AWS CloudFront without a signed URL
	// AWSCloudFront_Signed: delivery by AWS CloudFront with a signed URL
	string deliveryType,

	bool warningIfMissingMediaItemKey, bool filteredByStatistic, string userId
)
{
	string deliveryURL;
	string deliveryFileName;

	if (ingestionJobKey == -1)
	{
		string deliveryURI;
		int mmsPartitionNumber;
		int64_t localPhysicalPathKey = physicalPathKey;
		string title;

		if (localPhysicalPathKey != -1)
		{
			tuple<string, int, string, string> deliveryFileNameAndDeliveryURI =
				_mmsStorage->getVODDeliveryURI(localPhysicalPathKey, save, requestWorkspace);

			tie(title, mmsPartitionNumber, deliveryFileName, deliveryURI) = deliveryFileNameAndDeliveryURI;
		}
		else
		{
			if (uniqueName != "" && mediaItemKey == -1)
			{
				// initialize mediaItemKey

				pair<int64_t, MMSEngineDBFacade::ContentType> mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					requestWorkspace->_workspaceKey, uniqueName, warningIfMissingMediaItemKey,
					// 2022-12-18: assumo il MIK sia nel DB da un po di tempo
					false
				);
				tie(mediaItemKey, ignore) = mediaItemKeyDetails;
			}

			if (encodingProfileKey == -1 && encodingProfileLabel != "")
			{
				// initialize encodingProfileKey

				MMSEngineDBFacade::ContentType contentType;

				tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemDetails;
				mediaItemDetails = _mmsEngineDBFacade->getMediaItemKeyDetails(
					requestWorkspace->_workspaceKey, mediaItemKey,
					false, // warningIfMissing
					// 2022-12-18: assumo il MIK sia nel DB da un po di tempo
					false
				);
				tie(contentType, ignore, ignore, ignore, ignore, ignore) = mediaItemDetails;

				encodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
					requestWorkspace->_workspaceKey, contentType, encodingProfileLabel,
					true // contentTypeToBeUsed
				);
			}

			tuple<string, int, int64_t, string, string> vodDeliveryURIDetails =
				_mmsStorage->getVODDeliveryURI(mediaItemKey, encodingProfileKey, save, requestWorkspace);
			tie(title, mmsPartitionNumber, localPhysicalPathKey, deliveryFileName, deliveryURI) = vodDeliveryURIDetails;
		}

		if (deliveryType == "AWSCloudFront_Signed")
		{
			time_t expirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			expirationTime += ttlInSeconds;

			// deliverURI: /MMS_0000/2/.....
			size_t beginURIIndex = deliveryURI.find("/", 1);
			if (beginURIIndex == string::npos)
			{
				string errorMessage = string("wrong deliveryURI") + ", deliveryURI: " + deliveryURI;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string uriPath = deliveryURI.substr(beginURIIndex + 1);

			if (mmsPartitionNumber >= _vodCloudFrontHostNamesRoot.size())
			{
				string errorMessage = string("no CloudFrontHostName available") + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber) +
									  ", _vodCloudFrontHostNamesRoot.size: " + to_string(_vodCloudFrontHostNamesRoot.size());
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string cloudFrontHostName = JSONUtils::asString(_vodCloudFrontHostNamesRoot[mmsPartitionNumber]);

			AWSSigner awsSigner(_logger);
			string signedPlayURL = awsSigner.calculateSignedURL(cloudFrontHostName, uriPath, _keyPairId, _privateKeyPEMPathName, expirationTime);

			deliveryURL = signedPlayURL;
		}
		else if (deliveryType == "AWSCloudFront")
		{
			// deliverURI: /MMS_0000/2/.....
			size_t beginURIIndex = deliveryURI.find("/", 1);
			if (beginURIIndex == string::npos)
			{
				string errorMessage = string("wrong deliveryURI") + ", deliveryURI: " + deliveryURI;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string uriPath = deliveryURI.substr(beginURIIndex);

			if (mmsPartitionNumber >= _vodCloudFrontHostNamesRoot.size())
			{
				string errorMessage = string("no CloudFrontHostName available") + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber) +
									  ", _vodCloudFrontHostNamesRoot.size: " + to_string(_vodCloudFrontHostNamesRoot.size());
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string cloudFrontHostName = JSONUtils::asString(_vodCloudFrontHostNamesRoot[mmsPartitionNumber]);

			deliveryURL = "https://" + cloudFrontHostName + uriPath;
		}
		else if (deliveryType == "MMS_Token")
		{
			int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
				userKey, clientIPAddress, localPhysicalPathKey, -1, deliveryURI, ttlInSeconds, maxRetries
			);

			deliveryURL =
				_deliveryProtocol + "://" + _deliveryHost_authorizationThroughParameter + deliveryURI + "?token=" + to_string(authorizationKey);

			if (save && deliveryFileName != "")
				deliveryURL.append("&deliveryFileName=").append(deliveryFileName);
		}
		else // if (deliveryType == "MMS_SignedToken")
		{
			time_t expirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			expirationTime += ttlInSeconds;

			string uriToBeSigned;
			{
				string m3u8Suffix(".m3u8");
				if (deliveryURI.size() >= m3u8Suffix.size() &&
					0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
				{
					size_t endPathIndex = deliveryURI.find_last_of("/");
					if (endPathIndex == string::npos)
						uriToBeSigned = deliveryURI;
					else
						uriToBeSigned = deliveryURI.substr(0, endPathIndex);
				}
				else
					uriToBeSigned = deliveryURI;
			}
			string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

			deliveryURL = _deliveryProtocol + "://" + _deliveryHost_authorizationThroughPath + "/token_" + md5Base64 + "," +
						  to_string(expirationTime) + deliveryURI;
		}
		/*
		else
		{
			string errorMessage = string("wrong vodDeliveryType")
				+ ", deliveryType: " + deliveryType
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		if (!filteredByStatistic && userId != "")
		{
			try
			{
				_mmsEngineDBFacade->addRequestStatistic(
					requestWorkspace->_workspaceKey, clientIPAddress, userId, localPhysicalPathKey,
					-1, // confStreamKey
					title
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string("mmsEngineDBFacade->addRequestStatistic failed") + ", e.what: " + e.what();
				_logger->error(__FILEREF__ + errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "createDeliveryAuthorization info" + ", title: " + title + ", deliveryURI: " + deliveryURI +
			", deliveryType: " + deliveryType + ", deliveryURL (authorized): " + deliveryURL
		);
	}
	else
	{
		// create authorization for a live request

		tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
			_mmsEngineDBFacade->getIngestionJobDetails(
				requestWorkspace->_workspaceKey, ingestionJobKey,
				// 2022-12-18: it is a live request, it has time to be present into the slave
				false
			);
		MMSEngineDBFacade::IngestionType ingestionType;
		string metaDataContent;
		tie(ignore, ingestionType, ignore, metaDataContent, ignore) = ingestionJobDetails;

		if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy && ingestionType != MMSEngineDBFacade::IngestionType::VODProxy &&
			ingestionType != MMSEngineDBFacade::IngestionType::LiveGrid && ingestionType != MMSEngineDBFacade::IngestionType::LiveRecorder &&
			ingestionType != MMSEngineDBFacade::IngestionType::Countdown)
		{
			string errorMessage = string("ingestionJob is not a LiveProxy") + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		json ingestionJobRoot = JSONUtils::toJson(metaDataContent, false);

		if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy || ingestionType == MMSEngineDBFacade::IngestionType::VODProxy ||
			ingestionType == MMSEngineDBFacade::IngestionType::Countdown || ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
		{
			string field = "outputs";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage =
					string("A Proxy/Countdown/Recorder without outputs cannot be delivered") + ", ingestionJobKey: " + to_string(ingestionJobKey);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			json outputsRoot;
			if (JSONUtils::isMetadataPresent(ingestionJobRoot, "outputs"))
				outputsRoot = ingestionJobRoot["outputs"];
			else // if (JSONUtils::isMetadataPresent(ingestionJobRoot, "Outputs"))
				outputsRoot = ingestionJobRoot["Outputs"];

			// Option 1: OutputType HLS with deliveryCode
			// Option 2: OutputType RTMP_Channel/CDN_AWS/CDN_CDN77 with playURL
			// tuple<string, int64_t, string> means OutputType, deliveryCode, playURL
			vector<tuple<string, int64_t, string>> outputDeliveryOptions;
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType;
				int64_t localDeliveryCode = -1;
				string playURL;

				field = "outputType";
				outputType = JSONUtils::asString(outputRoot, field, "HLS_Channel");

				/*
				if (outputType == "HLS" || outputType == "DASH")
				{
					field = "DeliveryCode";
					localDeliveryCode = JSONUtils::asInt64(outputRoot, field, -1);
				}
				else */
				if (outputType == "RTMP_Channel" || outputType == "CDN_AWS" || outputType == "CDN_CDN77")
				{
					field = "playUrl";
					playURL = JSONUtils::asString(outputRoot, field, "");
					if (playURL == "")
						continue;
				}
				else if (outputType == "HLS_Channel")
				{
					field = "deliveryCode";
					localDeliveryCode = JSONUtils::asInt64(outputRoot, field, -1);
				}

				outputDeliveryOptions.push_back(make_tuple(outputType, localDeliveryCode, playURL));
			}

			string outputType;
			string playURL;

			if (deliveryCode == -1) // requested delivery code (it is an input)
			{
				if (outputDeliveryOptions.size() == 0)
				{
					string errorMessage =
						string("No outputDeliveryOptions, it cannot be delivered") + ", ingestionJobKey: " + to_string(ingestionJobKey);
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (outputDeliveryOptions.size() > 1)
				{
					string errorMessage =
						string("Live authorization with several option. Just get the first") + ", ingestionJobKey: " + to_string(ingestionJobKey);
					_logger->warn(__FILEREF__ + errorMessage);

					tie(outputType, deliveryCode, playURL) = outputDeliveryOptions[0];
				}
				else
				{
					// we have just one delivery code, it will be this one
					tie(outputType, deliveryCode, playURL) = outputDeliveryOptions[0];
				}
			}
			else
			{
				bool deliveryCodeFound = false;

				for (tuple<string, int64_t, string> outputDeliveryOption : outputDeliveryOptions)
				{
					int64_t localDeliveryCode;
					tie(outputType, localDeliveryCode, playURL) = outputDeliveryOption;

					if (outputType == "HLS_Channel" && localDeliveryCode == deliveryCode)
					{
						deliveryCodeFound = true;

						break;
					}
				}

				if (!deliveryCodeFound)
				{
					string errorMessage =
						string("DeliveryCode received does not exist for the ingestionJob") + ", ingestionJobKey: " + to_string(ingestionJobKey);
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			if (outputType == "RTMP_Channel" || outputType == "CDN_AWS" || outputType == "CDN_CDN77")
			{
				deliveryURL = playURL;
			}
			else if (outputType == "HLS_Channel")
			{
				string deliveryURI;
				string liveFileExtension;
				// if (outputType == "HLS")
				liveFileExtension = "m3u8";
				// else
				// 	liveFileExtension = "mpd";

				tuple<string, string, string> liveDeliveryDetails =
					_mmsStorage->getLiveDeliveryDetails(to_string(deliveryCode), liveFileExtension, requestWorkspace);
				tie(deliveryURI, ignore, deliveryFileName) = liveDeliveryDetails;

				if (deliveryType == "MMS_Token")
				{
					int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
						userKey, clientIPAddress,
						-1,			  // physicalPathKey,	vod key
						deliveryCode, // live key
						deliveryURI, ttlInSeconds, maxRetries
					);

					deliveryURL = _deliveryProtocol + "://" + _deliveryHost_authorizationThroughParameter + deliveryURI +
								  "?token=" + to_string(authorizationKey);
				}
				else // if (deliveryType == "MMS_SignedToken")
				{
					time_t expirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
					expirationTime += ttlInSeconds;

					string uriToBeSigned;
					{
						string m3u8Suffix(".m3u8");
						if (deliveryURI.size() >= m3u8Suffix.size() &&
							0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
						{
							size_t endPathIndex = deliveryURI.find_last_of("/");
							if (endPathIndex == string::npos)
								uriToBeSigned = deliveryURI;
							else
								uriToBeSigned = deliveryURI.substr(0, endPathIndex);
						}
						else
							uriToBeSigned = deliveryURI;
					}
					string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

					deliveryURL = _deliveryProtocol + "://" + _deliveryHost_authorizationThroughPath + "/token_" + md5Base64 + "," +
								  to_string(expirationTime) + deliveryURI;
				}
				/*
				else
				{
					string errorMessage = string("wrong deliveryType")
						+ ", deliveryType: " + deliveryType
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				*/
			}
			else
			{
				string deliveryURI;
				string liveFileExtension;
				if (outputType == "HLS_Channel")
					liveFileExtension = "m3u8";
				else
					liveFileExtension = "mpd";

				tuple<string, string, string> liveDeliveryDetails =
					_mmsStorage->getLiveDeliveryDetails(to_string(deliveryCode), liveFileExtension, requestWorkspace);
				tie(deliveryURI, ignore, deliveryFileName) = liveDeliveryDetails;

				if (deliveryType == "MMS_Token")
				{
					int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
						userKey, clientIPAddress,
						-1,			  // physicalPathKey,	vod key
						deliveryCode, // live key
						deliveryURI, ttlInSeconds, maxRetries
					);

					deliveryURL = _deliveryProtocol + "://" + _deliveryHost_authorizationThroughParameter + deliveryURI +
								  "?token=" + to_string(authorizationKey);
				}
				else // if (deliveryType == "MMS_SignedToken")
				{
					time_t expirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
					expirationTime += ttlInSeconds;

					string uriToBeSigned;
					{
						string m3u8Suffix(".m3u8");
						if (deliveryURI.size() >= m3u8Suffix.size() &&
							0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
						{
							size_t endPathIndex = deliveryURI.find_last_of("/");
							if (endPathIndex == string::npos)
								uriToBeSigned = deliveryURI;
							else
								uriToBeSigned = deliveryURI.substr(0, endPathIndex);
						}
						else
							uriToBeSigned = deliveryURI;
					}
					string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

					deliveryURL = _deliveryProtocol + "://" + _deliveryHost_authorizationThroughPath + "/token_" + md5Base64 + "," +
								  to_string(expirationTime) + deliveryURI;
				}
				/*
				else
				{
					string errorMessage = string("wrong deliveryType")
						+ ", deliveryType: " + deliveryType
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				*/
			}

			try
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage = field + " field missing" + ", ingestionJobKey: " + to_string(ingestionJobKey);
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				string configurationLabel = JSONUtils::asString(ingestionJobRoot, field, "");

				bool warningIfMissing = false;
				tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
					streamDetails = _mmsEngineDBFacade->getStreamDetails(requestWorkspace->_workspaceKey, configurationLabel, warningIfMissing);

				int64_t streamConfKey;
				tie(streamConfKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore) = streamDetails;

				_mmsEngineDBFacade->addRequestStatistic(
					requestWorkspace->_workspaceKey, clientIPAddress, userId,
					-1, // localPhysicalPathKey,
					streamConfKey, configurationLabel
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string("mmsEngineDBFacade->addRequestStatistic failed") + ", e.what: " + e.what();
				_logger->error(__FILEREF__ + errorMessage);
			}
		}
		/*
		else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
		{
			// outputType, localDeliveryCode, playURL
			vector<tuple<string, int64_t, string>> outputDeliveryOptions;

			string field = "outputs";
			if (JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				json outputsRoot = ingestionJobRoot[field];

				// Option 1: OutputType HLS with deliveryCode
				// Option 2: OutputType RTMP_Channel/CDN_AWS/CDN_CDN77 with playURL
				// tuple<string, int64_t, string> means OutputType, deliveryCode, playURL
				for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
				{
					json outputRoot = outputsRoot[outputIndex];

					string outputType;
					int64_t localDeliveryCode = -1;
					string playURL;

					field = "outputType";
					outputType = JSONUtils::asString(outputRoot, field, "HLS_Channel");

					if (outputType == "RTMP_Channel"
						|| outputType == "CDN_AWS"
						|| outputType == "CDN_CDN77")
					{
						field = "playUrl";
						playURL = JSONUtils::asString(outputRoot, field, "");
						if (playURL == "")
							continue;
					}
					else if (outputType == "HLS_Channel")
					{
						field = "deliveryCode";
						localDeliveryCode = JSONUtils::asInt64(outputRoot, field, -1);
					}

					outputDeliveryOptions.push_back(make_tuple(outputType, localDeliveryCode, playURL));
				}
			}

			string outputType;
			string playURL;

			bool monitorHLS = false;
			bool liveRecorderVirtualVOD = false;

			field = "monitorHLS";
			if (JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				monitorHLS = true;

			field = "liveRecorderVirtualVOD";
			if (JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				liveRecorderVirtualVOD = true;


			if (!(monitorHLS || liveRecorderVirtualVOD) && deliveryCode == -1)
			{
				// no monitorHLS and no input delivery code

				if (outputDeliveryOptions.size() == 0)
				{
					string errorMessage = string("No outputDeliveryOptions, it cannot be delivered")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (outputDeliveryOptions.size() > 1)
				{
					string errorMessage = string("Live authorization with several option. Just get the first")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->warn(__FILEREF__ + errorMessage);

					tie(outputType, deliveryCode, playURL) = outputDeliveryOptions[0];
				}
				else
				{
					// we have just one delivery code, it will be this one
					tie(outputType, deliveryCode, playURL) = outputDeliveryOptions[0];
				}
			}
			else if (!(monitorHLS || liveRecorderVirtualVOD) && deliveryCode != -1)
			{
				// no monitorHLS and delivery code received as input

				bool deliveryCodeFound = false;

				for (tuple<string, int64_t, string> outputDeliveryOption: outputDeliveryOptions)
				{
					int64_t localDeliveryCode;
					tie(outputType, localDeliveryCode, playURL) = outputDeliveryOption;

					if (outputType == "HLS_Channel" && localDeliveryCode == deliveryCode)
					{
						deliveryCodeFound = true;

						break;
					}
				}

				if (!deliveryCodeFound)
				{
					string errorMessage = string("DeliveryCode received does not exist for the ingestionJob")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else if ((monitorHLS || liveRecorderVirtualVOD) && deliveryCode == -1)
			{
				// monitorHLS and no delivery code received as input

				field = "DeliveryCode";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage = string("A Live-LiveRecorder Monitor HLS without DeliveryCode cannot be delivered")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				deliveryCode = JSONUtils::asInt64(ingestionJobRoot, field, 0);
				// outputType = "HLS";
				outputType = "HLS_Channel";
			}
			else if ((monitorHLS || liveRecorderVirtualVOD) && deliveryCode != -1)	// requested delivery code (it is an input)
			{
				// monitorHLS and delivery code received as input
				// monitorHLS is selected

				field = "DeliveryCode";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage = string("A Live-LiveRecorder Monitor HLS without DeliveryCode cannot be delivered")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				deliveryCode = JSONUtils::asInt64(ingestionJobRoot, field, 0);
				// outputType = "HLS";
				outputType = "HLS_Channel";
			}

			if (outputType == "RTMP_Channel"
				|| outputType == "CDN_AWS"
				|| outputType == "CDN_CDN77")
			{
				deliveryURL = playURL;
			}
			else if (outputType == "HLS_Channel")
			{
				string deliveryURI;
				string liveFileExtension = "m3u8";
				tuple<string, string, string> liveDeliveryDetails
					= _mmsStorage->getLiveDeliveryDetails(
					to_string(deliveryCode),
					liveFileExtension, requestWorkspace);
				tie(deliveryURI, ignore, deliveryFileName) =
					liveDeliveryDetails;

				if (deliveryType == "MMS_Token")
				{
					int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
						userKey,
						clientIPAddress,
						-1,	// physicalPathKey,
						deliveryCode,
						deliveryURI,
						ttlInSeconds,
						maxRetries);

					deliveryURL =
						_deliveryProtocol
						+ "://"
						+ _deliveryHost_authorizationThroughParameter
						+ deliveryURI
						+ "?token=" + to_string(authorizationKey)
					;
				}
				else // if (deliveryType == "MMS_SignedToken")
				{
					time_t expirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
					expirationTime += ttlInSeconds;

					string uriToBeSigned;
					{
						string m3u8Suffix(".m3u8");
						if (deliveryURI.size() >= m3u8Suffix.size()
							&& 0 == deliveryURI.compare(deliveryURI.size()-m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
						{
							size_t endPathIndex = deliveryURI.find_last_of("/");
							if (endPathIndex == string::npos)
								uriToBeSigned = deliveryURI;
							else
								uriToBeSigned = deliveryURI.substr(0, endPathIndex);
						}
						else
							uriToBeSigned = deliveryURI;
					}
					string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

					deliveryURL =
						_deliveryProtocol
						+ "://"
						+ _deliveryHost_authorizationThroughPath
						+ "/token_" + md5Base64 + "," + to_string(expirationTime)
						+ deliveryURI
					;
				}
			}
			else	// HLS
			{
				string deliveryURI;
				string liveFileExtension = "m3u8";
				tuple<string, string, string> liveDeliveryDetails
					= _mmsStorage->getLiveDeliveryDetails(
					to_string(deliveryCode),
					liveFileExtension, requestWorkspace);
				tie(deliveryURI, ignore, deliveryFileName) =
					liveDeliveryDetails;

				if (deliveryType == "MMS_Token")
				{
					int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
						userKey,
						clientIPAddress,
						-1,	// physicalPathKey,
						deliveryCode,
						deliveryURI,
						ttlInSeconds,
						maxRetries);

					deliveryURL =
						_deliveryProtocol
						+ "://"
						+ _deliveryHost_authorizationThroughParameter
						+ deliveryURI
						+ "?token=" + to_string(authorizationKey)
					;
				}
				else // if (deliveryType == "MMS_SignedToken")
				{
					time_t expirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
					expirationTime += ttlInSeconds;

					string uriToBeSigned;
					{
						string m3u8Suffix(".m3u8");
						if (deliveryURI.size() >= m3u8Suffix.size()
							&& 0 == deliveryURI.compare(deliveryURI.size()-m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
						{
							size_t endPathIndex = deliveryURI.find_last_of("/");
							if (endPathIndex == string::npos)
								uriToBeSigned = deliveryURI;
							else
								uriToBeSigned = deliveryURI.substr(0, endPathIndex);
						}
						else
							uriToBeSigned = deliveryURI;
					}
					string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

					deliveryURL =
						_deliveryProtocol
						+ "://"
						+ _deliveryHost_authorizationThroughPath
						+ "/token_" + md5Base64 + "," + to_string(expirationTime)
						+ deliveryURI
					;
				}
			}

			try
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage =
						field + " field missing"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				string configurationLabel = JSONUtils::asString(ingestionJobRoot, field, "");

				bool warningIfMissing = false;
				tuple<int64_t, string, string, string, string, int64_t, string, int, string, int,
					int, string, int, int, int, int, int, int64_t> streamDetails
					= _mmsEngineDBFacade->getStreamDetails(
					requestWorkspace->_workspaceKey,
					configurationLabel, warningIfMissing
				);

				int64_t streamConfKey;
				tie(streamConfKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore) = streamDetails;

				_mmsEngineDBFacade->addRequestStatistic(
					requestWorkspace->_workspaceKey,
					userId,
					-1,	// localPhysicalPathKey,
					streamConfKey,
					configurationLabel
				);
			}
			catch(runtime_error& e)
			{
				string errorMessage = string("mmsEngineDBFacade->addRequestStatistic failed")
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
			}

			_logger->info(__FILEREF__ + "createDeliveryAuthorization for LiveRecorder"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", deliveryCode: " + to_string(deliveryCode)
				+ ", deliveryURL: " + deliveryURL
			);
		}
		*/
		else // if (ingestionType != MMSEngineDBFacade::IngestionType::LiveGrid)
		{
			string field = "DeliveryCode";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage =
					string("A LiveGrid without DeliveryCode cannot be delivered") + ", ingestionJobKey: " + to_string(ingestionJobKey);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			deliveryCode = JSONUtils::asInt64(ingestionJobRoot, field, 0);

			string deliveryURI;
			string liveFileExtension = "m3u8";
			tuple<string, string, string> liveDeliveryDetails =
				_mmsStorage->getLiveDeliveryDetails(to_string(deliveryCode), liveFileExtension, requestWorkspace);
			tie(deliveryURI, ignore, deliveryFileName) = liveDeliveryDetails;

			if (deliveryType == "MMS_Token")
			{
				int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
					userKey, clientIPAddress,
					-1, // physicalPathKey,
					deliveryCode, deliveryURI, ttlInSeconds, maxRetries
				);

				deliveryURL =
					_deliveryProtocol + "://" + _deliveryHost_authorizationThroughParameter + deliveryURI + "?token=" + to_string(authorizationKey);
			}
			else // if (deliveryType == "MMS_SignedToken")
			{
				time_t expirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
				expirationTime += ttlInSeconds;

				string uriToBeSigned;
				{
					string m3u8Suffix(".m3u8");
					if (deliveryURI.size() >= m3u8Suffix.size() &&
						0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
					{
						size_t endPathIndex = deliveryURI.find_last_of("/");
						if (endPathIndex == string::npos)
							uriToBeSigned = deliveryURI;
						else
							uriToBeSigned = deliveryURI.substr(0, endPathIndex);
					}
					else
						uriToBeSigned = deliveryURI;
				}
				string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

				deliveryURL = _deliveryProtocol + "://" + _deliveryHost_authorizationThroughPath + "/token_" + md5Base64 + "," +
							  to_string(expirationTime) + deliveryURI;
			}
			/*
			else
			{
				string errorMessage = string("wrong deliveryType")
					+ ", deliveryType: " + deliveryType
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			*/

			_logger->info(
				__FILEREF__ + "createDeliveryAuthorization for LiveGrid" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", deliveryCode: " + to_string(deliveryCode) + ", deliveryURL: " + deliveryURL
			);
		}
	}

	return make_pair(deliveryURL, deliveryFileName);
}

string MMSDeliveryAuthorization::getSignedMMSPath(string contentURI, time_t expirationTime)
{
	string token = fmt::format("{}{}", expirationTime, contentURI);
	string md5Base64;
	{
		// unsigned char digest[MD5_DIGEST_LENGTH];
		// MD5((unsigned char*) token.c_str(), token.size(), digest);
		// md5Base64 = Convert::base64_encode(digest, MD5_DIGEST_LENGTH);
		{
			unsigned char *md5_digest;
			unsigned int md5_digest_len = EVP_MD_size(EVP_md5());

			EVP_MD_CTX *mdctx;

			// MD5_Init
			mdctx = EVP_MD_CTX_new();
			EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

			// MD5_Update
			EVP_DigestUpdate(mdctx, (unsigned char *)token.c_str(), token.size());

			// MD5_Final
			md5_digest = (unsigned char *)OPENSSL_malloc(md5_digest_len);
			EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);

			md5Base64 = Convert::base64_encode(md5_digest, md5_digest_len);

			OPENSSL_free(md5_digest);

			EVP_MD_CTX_free(mdctx);
		}

		transform(
			md5Base64.begin(), md5Base64.end(), md5Base64.begin(),
			[](unsigned char c)
			{
				if (c == '+')
					return '-';
				else if (c == '/')
					return '_';
				else
					return (char)c;
			}
		);
	}

	_logger->info(
		__FILEREF__ + "Authorization through path" + ", contentURI: " + contentURI + ", expirationTime: " + to_string(expirationTime) +
		", token: " + token + ", md5Base64: " + md5Base64
	);

	return md5Base64;
}

string MMSDeliveryAuthorization::getSignedCDN77URL(
	string resourceURL, // i.e.: 1234456789.rsc.cdn77.org
	string filePath,	// /file/playlist/d.m3u8
	string secureToken, long expirationInMinutes, shared_ptr<spdlog::logger> logger
)
{
	logger->info(
		__FILEREF__ + "getSignedCDN77URL" + ", resourceURL: " + resourceURL + ", filePath: " + filePath + ", secureToken: " + secureToken +
		", expirationInMinutes: " + to_string(expirationInMinutes)
	);

	try
	{
		//  It's smart to set the expiration time as current time plus 5 minutes { time() + 300}.
		//  This way the link will be available only for the time needed to start the download.

		long expiryTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now()) + (expirationInMinutes * 60);

		string signedURL;
		{
			string strippedPath;
			{
				// because of hls/dash, anything included after the last slash (e.g. playlist/{chunk}) shouldn't be part of the path string,
				// for which we generate the secure token. Because of that, everything included after the last slash is stripped.
				// $strippedPath = substr($filePath, 0, strrpos($filePath, '/'));
				size_t fileNameStart = filePath.find_last_of("/");
				if (fileNameStart == string::npos)
				{
					string errorMessage = string("filePath format is wrong") + ", filePath: " + filePath;
					logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				strippedPath = filePath.substr(0, fileNameStart);

				// replace invalid URL query string characters +, =, / with valid characters -, _, ~
				// $invalidChars = ['+','/'];
				// $validChars = ['-','_'];
				// GIU: replace is done below

				// if ($strippedPath[0] != '/') {
				// 	$strippedPath = '/' . $strippedPath;
				// }
				// GIU: our strippedPath already starts with /

				// if ($pos = strpos($strippedPath, '?')) {
				// 	$filePath = substr($strippedPath, 0, $pos);
				// }
				// GIU: our strippedPath does not have ?
			}

			// $hashStr = $strippedPath . $secureToken;
			string hashStr = strippedPath + secureToken;

			// if ($expiryTimestamp) {
			// 	$hashStr = $expiryTimestamp . $hashStr;
			// 	$expiryTimestamp = ',' . $expiryTimestamp;
			// }
			hashStr = to_string(expiryTimestamp) + hashStr;
			string sExpiryTimestamp = string(",") + to_string(expiryTimestamp);

			logger->info(
				__FILEREF__ + "getSignedCDN77URL" + ", strippedPath: " + strippedPath + ", hashStr: " + hashStr +
				", sExpiryTimestamp: " + sExpiryTimestamp
			);

			// the URL is however, intensionaly returned with the previously stripped parts (eg. playlist/{chunk}..)
			// return 'http://' . $cdnResourceUrl . '/' .
			// 	str_replace($invalidChars, $validChars, base64_encode(md5($hashStr, TRUE))) .
			// 	$expiryTimestamp . $filePath;
			string md5Base64;
			{
				// unsigned char digest[MD5_DIGEST_LENGTH];
				// MD5((unsigned char*) hashStr.c_str(), hashStr.size(), digest);
				// md5Base64 = Convert::base64_encode(digest, MD5_DIGEST_LENGTH);

				{
					unsigned char *md5_digest;
					unsigned int md5_digest_len = EVP_MD_size(EVP_md5());

					EVP_MD_CTX *mdctx;

					// MD5_Init
					mdctx = EVP_MD_CTX_new();
					EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

					// MD5_Update
					EVP_DigestUpdate(mdctx, (unsigned char *)hashStr.c_str(), hashStr.size());

					// MD5_Final
					md5_digest = (unsigned char *)OPENSSL_malloc(md5_digest_len);
					EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);

					/*
					{
						string md5 = string((char *) md5_digest, md5_digest_len);

						logger->info(__FILEREF__ + "getSignedCDN77URL"
							+ ", hashStr: " + hashStr
							+ ", md5_digest_len: " + to_string(md5_digest_len)
							+ ", md5: " + md5
						);
					}
					*/

					// md5Base64 = Convert::base64_encode(md5_digest, md5_digest_len);
					{
						BIO *bio, *b64;
						BUF_MEM *bufferPtr;

						logger->debug(__FILEREF__ + "BIO_new...");
						b64 = BIO_new(BIO_f_base64());
						// By default there must be a newline at the end of input.
						// Next flag remove new line at the end
						BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
						bio = BIO_new(BIO_s_mem());

						logger->debug(__FILEREF__ + "BIO_push...");
						bio = BIO_push(b64, bio);

						logger->debug(__FILEREF__ + "BIO_write...");
						BIO_write(bio, md5_digest, md5_digest_len);
						BIO_flush(bio);
						BIO_get_mem_ptr(bio, &bufferPtr);

						logger->debug(__FILEREF__ + "BIO_set_close...");
						BIO_set_close(bio, BIO_NOCLOSE);
						logger->debug(__FILEREF__ + "BIO_free_all...");
						BIO_free_all(bio);
						// _logger->info(__FILEREF__ + "BIO_free...");
						// BIO_free(b64);	// useless because of BIO_free_all

						logger->debug(__FILEREF__ + "base64Text set...");
						// char* base64Text=(*bufferPtr).data;

						logger->info(
							__FILEREF__ + "getSignedCDN77URL" + ", (*bufferPtr).length: " + to_string((*bufferPtr).length) +
							", (*bufferPtr).data: " + (*bufferPtr).data
						);

						md5Base64 = string((*bufferPtr).data, (*bufferPtr).length);

						BUF_MEM_free(bufferPtr);
					}

					OPENSSL_free(md5_digest);

					EVP_MD_CTX_free(mdctx);
				}

				logger->info(__FILEREF__ + "getSignedCDN77URL" + ", md5Base64: " + md5Base64);

				// $invalidChars = ['+','/'];
				// $validChars = ['-','_'];
				transform(
					md5Base64.begin(), md5Base64.end(), md5Base64.begin(),
					[](unsigned char c)
					{
						if (c == '+')
							return '-';
						else if (c == '/')
							return '_';
						else
							return (char)c;
					}
				);
			}

			signedURL = "https://" + resourceURL + "/" + md5Base64 + sExpiryTimestamp + filePath;
		}

		logger->info(
			__FILEREF__ + "end getSignedCDN77URL" + ", resourceURL: " + resourceURL + ", filePath: " + filePath + ", secureToken: " + secureToken +
			", expirationInMinutes: " + to_string(expirationInMinutes) + ", signedURL: " + signedURL
		);

		return signedURL;
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("getSignedCDN77URL failed") + ", e.what(): " + e.what();
		logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		string errorMessage = string("getSignedCDN77URL failed") + ", e.what(): " + e.what();
		logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}
