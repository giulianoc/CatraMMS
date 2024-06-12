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
#include "catralibraries/Encrypt.h"
#include "catralibraries/StringUtils.h"
#include "spdlog/fmt/fmt.h"
// #include <openssl/md5.h>
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <regex>

#define AWSCLOUDFRONT

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
	_vodCloudFrontHostName = _configuration["aws"]["vodCloudFrontHostName"];
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->vodCloudFrontHostName: " + _vodCloudFrontHostName);
	_vodDeliveryCloudFrontHostName = _configuration["aws"]["vodDeliveryCloudFrontHostName"];
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->vodDeliveryCloudFrontHostName: " + _vodDeliveryCloudFrontHostName);
	_vodDeliveryPathCloudFrontHostName = _configuration["aws"]["vodDeliveryPathCloudFrontHostName"];
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->vodDeliveryPathCloudFrontHostName: " + _vodDeliveryPathCloudFrontHostName);

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
	// MMS_URLWithTokenAsParam_DB (ex MMS_Token): delivery by MMS with a Token retrieved by DB
	// MMS_URLWithTokenAsParam_Signed (ex MMS_Token): delivery by MMS with a Token signed
	// MMS_SignedURL: delivery by MMS with a signed URL
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

			/*
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
			*/
			AWSSigner awsSigner(_logger);
			string signedPlayURL =
				awsSigner.calculateSignedURL(_vodCloudFrontHostName, deliveryURI, _keyPairId, _privateKeyPEMPathName, expirationTime);

			deliveryURL = signedPlayURL;
		}
		/*
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
		*/
		else if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "MMS_URLWithTokenAsParam_Signed" ||
				 deliveryType == "MMS_URLWithTokenAsParam" || deliveryType == "MMS_Token") // da eliminare dopo il deploy
		{
			string deliveryHost;
#ifdef AWSCLOUDFRONT
			deliveryHost = _vodDeliveryCloudFrontHostName;
#else
			deliveryHost = _deliveryHost_authorizationThroughParameter;
#endif
			if (deliveryType == "MMS_URLWithTokenAsParam_DB")
			{
				int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
					userKey, clientIPAddress, localPhysicalPathKey, -1, deliveryURI, ttlInSeconds, maxRetries, true
				);

				deliveryURL = fmt::format("{}://{}{}?token={}", _deliveryProtocol, deliveryHost, deliveryURI, authorizationKey);
			}
			else
			{
				time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

				string uriToBeSigned;
				{
					string m3u8Suffix(".m3u8");
					// if (deliveryURI.size() >= m3u8Suffix.size() &&
					// 	0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
					if (StringUtils::endWith(deliveryURI, m3u8Suffix))
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
					fmt::format("{}://{}{}?token={},{}", _deliveryProtocol, deliveryHost, deliveryURI, curlpp::escape(md5Base64), expirationTime);
			}

			if (save && deliveryFileName != "")
				deliveryURL.append("&deliveryFileName=").append(deliveryFileName);
		}
		else // if (deliveryType == "MMS_SignedURL")
		{
			time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

			string uriToBeSigned;
			{
				string m3u8Suffix(".m3u8");
				// if (deliveryURI.size() >= m3u8Suffix.size() &&
				// 	0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
				if (StringUtils::endWith(deliveryURI, m3u8Suffix))
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

			string deliveryHost;
#ifdef AWSCLOUDFRONT
			deliveryHost = _vodDeliveryPathCloudFrontHostName;
#else
			deliveryHost = _deliveryHost_authorizationThroughPath;
#endif
			deliveryURL = fmt::format("{}://{}/token_{},{}{}", _deliveryProtocol, deliveryHost, md5Base64, expirationTime, deliveryURI);
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

				if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "MMS_URLWithTokenAsParam_Signed" ||
					deliveryType == "MMS_URLWithTokenAsParam" || deliveryType == "MMS_Token") // da eliminare dopo il deploy
				{
					if (deliveryType == "MMS_URLWithTokenAsParam_DB")
					{
						int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
							userKey, clientIPAddress,
							-1,			  // physicalPathKey,	vod key
							deliveryCode, // live key
							deliveryURI, ttlInSeconds, maxRetries, true
						);

						deliveryURL = fmt::format(
							"{}://{}{}?token={}", _deliveryProtocol, _deliveryHost_authorizationThroughParameter, deliveryURI, authorizationKey
						);
					}
					else
					{
						time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

						string uriToBeSigned;
						{
							string m3u8Suffix(".m3u8");
							// if (deliveryURI.size() >= m3u8Suffix.size() &&
							// 	0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
							if (StringUtils::endWith(deliveryURI, m3u8Suffix))
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

						deliveryURL = fmt::format(
							"{}://{}{}?token={},{}", _deliveryProtocol, _deliveryHost_authorizationThroughParameter, deliveryURI,
							curlpp::escape(md5Base64), expirationTime
						);
					}
				}
				else // if (deliveryType == "MMS_SignedURL")
				{
					time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

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

				if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "MMS_URLWithTokenAsParam_Signed" ||
					deliveryType == "MMS_URLWithTokenAsParam" || deliveryType == "MMS_Token") // da eliminare dopo il deploy
				{
					if (deliveryType == "MMS_URLWithTokenAsParam_DB")
					{
						int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
							userKey, clientIPAddress,
							-1,			  // physicalPathKey,	vod key
							deliveryCode, // live key
							deliveryURI, ttlInSeconds, maxRetries, true
						);

						deliveryURL = fmt::format(
							"{}://{}{}?token={}", _deliveryProtocol, _deliveryHost_authorizationThroughParameter, deliveryURI, authorizationKey
						);
					}
					else
					{
						time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

						string uriToBeSigned;
						{
							string m3u8Suffix(".m3u8");
							// if (deliveryURI.size() >= m3u8Suffix.size() &&
							// 	0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
							if (StringUtils::endWith(deliveryURI, m3u8Suffix))
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

						deliveryURL = fmt::format(
							"{}://{}{}?token={},{}", _deliveryProtocol, _deliveryHost_authorizationThroughParameter, deliveryURI,
							curlpp::escape(md5Base64), expirationTime
						);
					}
				}
				else // if (deliveryType == "MMS_SignedURL")
				{
					time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

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

			if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "MMS_URLWithTokenAsParam_Signed" ||
				deliveryType == "MMS_URLWithTokenAsParam" || deliveryType == "MMS_Token") // da eliminare dopo il deploy
			{
				int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
					userKey, clientIPAddress,
					-1, // physicalPathKey,
					deliveryCode, deliveryURI, ttlInSeconds, maxRetries, true
				);

				deliveryURL =
					_deliveryProtocol + "://" + _deliveryHost_authorizationThroughParameter + deliveryURI + "?token=" + to_string(authorizationKey);
			}
			else // if (deliveryType == "MMS_SignedURL")
			{
				time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

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

string MMSDeliveryAuthorization::checkDeliveryAuthorizationThroughParameter(string contentURI, string tokenParameter)
{
	string tokenComingFromURL;
	try
	{
		SPDLOG_INFO(
			"checkDeliveryAuthorizationThroughParameter, received"
			", contentURI: {}"
			", tokenParameter: {}",
			contentURI, tokenParameter
		);

		string firstPartOfToken;
		string secondPartOfToken;
		{
			// token formats:
			// scenario in case of .ts (hls) delivery: <encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			// scenario in case of .m4s (dash) delivery: ---<cookie: encription of 'token'>
			//		both encryption were built in 'manageHTTPStreamingManifest'
			// scenario in case of .m3u8 delivery:
			//		case 1. master .m3u8: <token>---
			//		case 2. secondary .m3u8: <encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			// scenario in case of any other delivery: <token>---
			//		Any other delivery includes for example also the delivery/download of a .ts file, not part
			//		of an hls delivery. In this case we will have just a token as any normal download

			string separator = "---";
			size_t endOfTokenIndex = tokenParameter.rfind(separator);
			if (endOfTokenIndex == string::npos)
			{
				string errorMessage = fmt::format(
					"Wrong token format, no --- is present"
					", contentURI: {}"
					", tokenParameter: {}",
					contentURI, tokenParameter
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
			firstPartOfToken = tokenParameter.substr(0, endOfTokenIndex);
			{
				// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
				//	That  because if we have really a + char (%2B into the string), and we do the replace
				//	after curlpp::unescape, this char will be changed to space and we do not want it
				string plus = "\\+";
				string plusDecoded = " ";
				string firstDecoding = regex_replace(firstPartOfToken, regex(plus), plusDecoded);

				firstPartOfToken = curlpp::unescape(firstDecoding);
			}
			secondPartOfToken = tokenParameter.substr(endOfTokenIndex + separator.length());
		}

		// end with
		string tsExtension(".ts");	   // hls
		string m4sExtension(".m4s");   // dash
		string m3u8Extension(".m3u8"); // m3u8
		if ((secondPartOfToken != "")  // secondPartOfToken is the cookie
			&& (StringUtils::endWith(contentURI, tsExtension) || StringUtils::endWith(contentURI, m4sExtension) ||
				StringUtils::endWith(contentURI, m3u8Extension)))
		{
			// nel caso del manifest secondario, dovrebbe ricevere il cookie (secondPartOfToken) e quindi dovrebbe entrare qui
			// es: tokenParameter: Z4-QJrfFHFzIYMJ8WDufoiBGJJVCfHzu9cZ4jfTMsjhIG7o1b19~8jQQlxmdn8Y1---0S5rnsVsbCVR7ou9vWmyXA__
			// 		firstPartOfToken: Z4-QJrfFHFzIYMJ8WDufoiBGJJVCfHzu9cZ4jfTMsjhIG7o1b19~8jQQlxmdn8Y1
			// 		secondPartOfToken: 0S5rnsVsbCVR7ou9vWmyXA__
			// 		contentURI: /MMS_0000/1/001/471/861/8061992_2/360p/8061992_1653191.m3u8
			// 2024-06-09: sto notando che, nel caso del sito mms, il cookie non viene mandato (probabilmente per problemi di CORS con videop-js) e
			// 	quindi non entra in questo if e l'autorizzazione fallisce!!!
			// 	Con altri player, ad esempio VLC, il cookie viene mandato e lo streaming funziona bene
			SPDLOG_INFO(
				"HLS/DASH file to be checked for authorization"
				", tokenParameter: {}"
				", firstPartOfToken: {}"
				", secondPartOfToken: {}"
				", contentURI: {}",
				tokenParameter, firstPartOfToken, secondPartOfToken, contentURI
			);

			string encryptedToken = firstPartOfToken;
			string cookie = secondPartOfToken;

			if (cookie == "")
			{
				string errorMessage = fmt::format(
					"cookie is wrong"
					", contentURI: {}"
					", cookie: {}"
					", firstPartOfToken: {}"
					", secondPartOfToken: {}",
					contentURI, cookie, firstPartOfToken, secondPartOfToken
				);
				SPDLOG_INFO(errorMessage);

				throw runtime_error(errorMessage);
			}
			// manifestLineAndToken comes from ts URL
			string manifestLineAndToken = Encrypt::opensslDecrypt(encryptedToken);
			string manifestLine;
			// int64_t tokenComingFromURL;
			{
				string separator = "+++";
				size_t beginOfTokenIndex = manifestLineAndToken.rfind(separator);
				if (beginOfTokenIndex == string::npos)
				{
					string errorMessage = fmt::format(
						"Wrong parameter format"
						", contentURI: {}"
						", manifestLineAndToken: {}",
						contentURI, manifestLineAndToken
					);
					SPDLOG_INFO(errorMessage);

					throw runtime_error(errorMessage);
				}
				manifestLine = manifestLineAndToken.substr(0, beginOfTokenIndex);
				tokenComingFromURL = manifestLineAndToken.substr(beginOfTokenIndex + separator.length());
				// string sTokenComingFromURL = manifestLineAndToken.substr(beginOfTokenIndex + separator.length());
				// tokenComingFromURL = stoll(sTokenComingFromURL);
			}

			string tokenComingFromCookie = Encrypt::opensslDecrypt(cookie);
			// string sTokenComingFromCookie = Encrypt::opensslDecrypt(cookie);
			// int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

			SPDLOG_INFO(
				"check token info"
				", encryptedToken: {}"
				", manifestLineAndToken: {}"
				", manifestLine: {}"
				", tokenComingFromURL: {}"
				", cookie: {}"
				", tokenComingFromCookie: {}"
				", contentURI: {}",
				encryptedToken, manifestLineAndToken, manifestLine, tokenComingFromURL, cookie, tokenComingFromCookie, contentURI
			);

			if (tokenComingFromCookie != tokenComingFromURL

				// i.e., contentURI: /MMSLive/1/94/94446.ts, manifestLine: 94446.ts
				// 2020-02-04: commented because it does not work in case of dash
				// contentURI: /MMSLive/1/109/init-stream0.m4s
				// manifestLine: chunk-stream$RepresentationID$-$Number%05d$.m4s
				// || contentURI.find(manifestLine) == string::npos
			)
			{
				string errorMessage = fmt::format(
					"Wrong parameter format"
					", contentURI: {}"
					", manifestLine: {}"
					", tokenComingFromCookie: {}"
					", tokenComingFromURL: {}",
					contentURI, manifestLine, tokenComingFromCookie, tokenComingFromURL
				);
				SPDLOG_INFO(errorMessage);

				throw runtime_error(errorMessage);
			}

			SPDLOG_INFO(
				"token authorized"
				", contentURI: {}"
				", manifestLine: {}"
				", tokenComingFromURL: {}"
				", tokenComingFromCookie: {}",
				contentURI, manifestLine, tokenComingFromURL, tokenComingFromCookie
			);

			// authorized
			// string responseBody;
			// sendSuccess(request, 200, responseBody);
		}
		else
		{
			// 2024-06-09: se contentURI è un manifest secondario e siamo entrati in questo else
			// 	vuol dire che il player non sta mandando i cookie, vedi commento associato all'if

			SPDLOG_INFO(
				"file to be checked for authorization"
				", tokenParameter: {}"
				", firstPartOfToken: {}"
				", secondPartOfToken: {}"
				", contentURI: {}",
				tokenParameter, firstPartOfToken, secondPartOfToken, contentURI
			);

			if (StringUtils::isNumber(firstPartOfToken)) // MMS_URLWithTokenAsParam_DB
			{
				// tokenComingFromURL = stoll(firstPartOfToken);
				tokenComingFromURL = firstPartOfToken;
				if (_mmsEngineDBFacade->checkDeliveryAuthorization(stoll(tokenComingFromURL), contentURI))
				{
					SPDLOG_INFO(
						"token authorized"
						", tokenComingFromURL: {}",
						tokenComingFromURL
					);

					// authorized

					// string responseBody;
					// sendSuccess(request, 200, responseBody);
				}
				else
				{
					string errorMessage = fmt::format(
						"Not authorized: token invalid"
						", tokenComingFromURL: {}",
						tokenComingFromURL
					);
					SPDLOG_WARN(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else // MMS_URLWithTokenAsParam_Signed
			{
				string tokenSigned = firstPartOfToken;
				checkSignedMMSPath(tokenSigned, contentURI);
			}
		}
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("Not authorized");
		SPDLOG_WARN(errorMessage);

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		SPDLOG_WARN(errorMessage);

		throw e;
	}

	return tokenComingFromURL;
}

int64_t MMSDeliveryAuthorization::checkDeliveryAuthorizationThroughPath(string contentURI)
{
	int64_t tokenComingFromURL = -1;
	try
	{
		SPDLOG_INFO(
			"checkDeliveryAuthorizationThroughPath, received"
			", contentURI: {}",
			contentURI
		);

		string tokenLabel = "/token_";

		size_t startTokenIndex = contentURI.find("/token_");
		if (startTokenIndex == string::npos)
		{
			string errorMessage = fmt::format(
				"Wrong token format"
				", contentURI: {}",
				contentURI
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		startTokenIndex += tokenLabel.size();

		size_t endTokenIndex = contentURI.find(",", startTokenIndex);
		if (endTokenIndex == string::npos)
		{
			string errorMessage = fmt::format(
				"Wrong token format"
				", contentURI: {}",
				contentURI
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t endExpirationIndex = contentURI.find("/", endTokenIndex);
		if (endExpirationIndex == string::npos)
		{
			string errorMessage = fmt::format(
				"Wrong token format"
				", contentURI: {}",
				contentURI
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		string tokenSigned = contentURI.substr(startTokenIndex, endExpirationIndex - startTokenIndex);

		string contentURIToBeVerified;

		size_t endContentURIIndex = contentURI.find("?", endExpirationIndex);
		if (endContentURIIndex == string::npos)
			contentURIToBeVerified = contentURI.substr(endExpirationIndex);
		else
			contentURIToBeVerified = contentURI.substr(endExpirationIndex, endContentURIIndex - endExpirationIndex);

		return checkSignedMMSPath(tokenSigned, contentURIToBeVerified);
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("Not authorized");
		SPDLOG_WARN(errorMessage);

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		SPDLOG_WARN(errorMessage);

		throw e;
	}

	return tokenComingFromURL;
}

string MMSDeliveryAuthorization::checkDeliveryAuthorizationOfAManifest(bool secondaryManifest, string token, string cookie, string contentURI)
{
	try
	{
		SPDLOG_INFO(
			"checkDeliveryAuthorizationOfAManifest, received"
			", contentURI: {}",
			contentURI
		);

		string tokenComingFromURL;

		// we could have:
		//		- master manifest, token parameter: <token>--- (es: token=9163 oppure ic_vOSatb6TWp4ania5kaQ%3D%3D,1717958161)
		//			es: /MMS_0000/1/001/472/152/8063642_2/8063642_1653439.m3u8?token=9163
		//			es: /MMS_0000/1/001/470/566/8055007_2/8055007_1652158.m3u8?token=ic_vOSatb6TWp4ania5kaQ%3D%3D,1717958161
		//		- secondary manifest (that has to be treated as a .ts delivery), token parameter:
		//			<encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
		//			es:
		/// MMS_0000/1/001/472/152/8063642_2/360p/8063642_1653439.m3u8?token=Nw2npoRhfMLZC-GiRuZHpI~jGKBRA-NE-OARj~o68En4XFUriOSuXqexke21OTVd
		if (secondaryManifest)
		{
			/* unescape è gia in checkDeliveryAuthorizationThroughParameter
			string tokenParameter;
			{
				// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
				//	That  because if we have really a + char (%2B into the string), and we do the replace
				//	after curlpp::unescape, this char will be changed to space and we do not want it
				string plus = "\\+";
				string plusDecoded = " ";
				string firstDecoding = regex_replace(tokenIt->second, regex(plus), plusDecoded);

				tokenParameter = curlpp::unescape(firstDecoding);
			}
			tokenParameter = fmt::format("{}---{}", tokenParameter, cookie);
			*/
			string tokenParameter = fmt::format("{}---{}", token, cookie);
			SPDLOG_INFO(
				"Calling checkDeliveryAuthorizationThroughParameter"
				", contentURI: {}"
				", tokenParameter: {}",
				contentURI, tokenParameter
			);
			tokenComingFromURL = checkDeliveryAuthorizationThroughParameter(contentURI, tokenParameter);
		}
		else
		{
			SPDLOG_INFO(
				"manageHTTPStreamingManifest"
				", token: {}"
				", mmsInfoCookie: {}",
				token, cookie
			);

			tokenComingFromURL = token;

			if (cookie == "")
			{
				if (StringUtils::isNumber(token)) // MMS_URLWithTokenAsParam_DB
				{
					if (!_mmsEngineDBFacade->checkDeliveryAuthorization(stoll(token), contentURI))
					{
						string errorMessage = fmt::format(
							"Not authorized: token invalid"
							", contentURI: {}"
							", token: {}",
							contentURI, token
						);
						SPDLOG_INFO(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				else
				{
					{
						// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
						//	That  because if we have really a + char (%2B into the string), and we do the replace
						//	after curlpp::unescape, this char will be changed to space and we do not want it
						string plus = "\\+";
						string plusDecoded = " ";
						string firstDecoding = regex_replace(token, regex(plus), plusDecoded);

						tokenComingFromURL = curlpp::unescape(firstDecoding);
					}
					checkSignedMMSPath(tokenComingFromURL, contentURI);
				}

				SPDLOG_INFO(
					"token authorized"
					", tokenComingFromURL: {}",
					tokenComingFromURL
				);
			}
			else
			{
				string sTokenComingFromCookie = Encrypt::opensslDecrypt(cookie);
				// int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

				if (sTokenComingFromCookie != tokenComingFromURL)
				{
					string errorMessage = fmt::format(
						"cookie invalid, let's check the token"
						", sTokenComingFromCookie: {}"
						", tokenComingFromURL: {}",
						sTokenComingFromCookie, tokenComingFromURL
					);
					SPDLOG_INFO(errorMessage);

					if (StringUtils::isNumber(tokenComingFromURL)) // MMS_URLWithTokenAsParam_DB
					{
						if (!_mmsEngineDBFacade->checkDeliveryAuthorization(stoll(tokenComingFromURL), contentURI))
						{
							string errorMessage = fmt::format(
								"Not authorized: token invalid"
								", contentURI: {}"
								", tokenComingFromURL: {}",
								contentURI, tokenComingFromURL
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						checkSignedMMSPath(tokenComingFromURL, contentURI);
					}

					SPDLOG_INFO(
						"token authorized"
						", tokenComingFromURL: {}",
						tokenComingFromURL
					);
				}
				else
				{
					SPDLOG_INFO(
						"cookie authorized"
						", cookie: {}",
						cookie
					);
				}
			}
		}

		return tokenComingFromURL;
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("Not authorized");
		SPDLOG_WARN(errorMessage);

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		SPDLOG_WARN(errorMessage);

		throw e;
	}
}

int64_t MMSDeliveryAuthorization::checkSignedMMSPath(string tokenSigned, string contentURIToBeVerified)
{
	int64_t tokenComingFromURL = -1;
	try
	{
		SPDLOG_INFO(
			"checkSignedMMSPath, received"
			", tokenSigned: {}"
			", contentURIToBeVerified: {}",
			tokenSigned, contentURIToBeVerified
		);

		size_t endTokenIndex = tokenSigned.find(",");
		if (endTokenIndex == string::npos)
		{
			string errorMessage = fmt::format(
				"Wrong token format, no comma and expirationTime present"
				", tokenSigned: {}",
				tokenSigned
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sExpirationTime = tokenSigned.substr(endTokenIndex + 1);
		time_t expirationTime = stoll(sExpirationTime);

		tokenSigned = tokenSigned.substr(0, endTokenIndex);

		string m3u8Suffix(".m3u8");
		string tsSuffix(".ts");
		if (StringUtils::endWith(contentURIToBeVerified, m3u8Suffix))
		{
			{
				size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
				if (endPathIndex != string::npos)
					contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
			}

			string md5Base64 = getSignedMMSPath(contentURIToBeVerified, expirationTime);

			SPDLOG_INFO(
				"Authorization through path (m3u8)"
				", contentURIToBeVerified: {}"
				", expirationTime: {}"
				", tokenSigned: {}"
				", md5Base64: {}",
				contentURIToBeVerified, expirationTime, tokenSigned, md5Base64
			);

			if (md5Base64 != tokenSigned)
			{
				// we still try removing again the last directory to manage the scenario
				// of multi bitrate encoding or multi audio tracks (one director for each audio)
				{
					size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
					if (endPathIndex != string::npos)
						contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
				}

				string md5Base64 = getSignedMMSPath(contentURIToBeVerified, expirationTime);

				SPDLOG_INFO(
					"Authorization through path (m3u8 2)"
					", contentURIToBeVerified: {}"
					", expirationTime: {}"
					", tokenSigned: {}"
					", md5Base64: {}",
					contentURIToBeVerified, expirationTime, tokenSigned, md5Base64
				);

				if (md5Base64 != tokenSigned)
				{
					string errorMessage = fmt::format(
						"Wrong token (m3u8)"
						", md5Base64: {}"
						", tokenSigned: {}",
						md5Base64, tokenSigned
					);
					SPDLOG_WARN(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
		else if (StringUtils::endWith(contentURIToBeVerified, tsSuffix))
		{
			// 2022-11-29: ci sono 3 casi per il download di un .ts:
			//	1. download NON dall'interno di un m3u8
			//	2. download dall'interno di un m3u8 single bitrate
			//	3. download dall'interno di un m3u8 multi bitrate

			{
				// check caso 1.
				string md5Base64 = getSignedMMSPath(contentURIToBeVerified, expirationTime);

				SPDLOG_INFO(
					"Authorization through path"
					", contentURIToBeVerified: {}"
					", expirationTime: {}"
					", tokenSigned: {}"
					", md5Base64: {}",
					contentURIToBeVerified, expirationTime, tokenSigned, md5Base64
				);

				if (md5Base64 != tokenSigned)
				{
					// potremmo essere nel caso 2 o caso 3

					{
						size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
						if (endPathIndex != string::npos)
							contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
					}

					// check caso 2.
					md5Base64 = getSignedMMSPath(contentURIToBeVerified, expirationTime);

					SPDLOG_INFO(
						"Authorization through path (ts 1)"
						", contentURIToBeVerified: {}"
						", expirationTime: {}"
						", tokenSigned: {}"
						", md5Base64: {}",
						contentURIToBeVerified, expirationTime, tokenSigned, md5Base64
					);

					if (md5Base64 != tokenSigned)
					{
						// dovremmo essere nel caso 3

						// we still try removing again the last directory to manage
						// the scenario of multi bitrate encoding
						{
							size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
							if (endPathIndex != string::npos)
								contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
						}

						// check caso 3.
						string md5Base64 = getSignedMMSPath(contentURIToBeVerified, expirationTime);

						SPDLOG_INFO(
							"Authorization through path (ts 2)"
							", contentURIToBeVerified: {}"
							", expirationTime: {}"
							", tokenSigned: {}"
							", md5Base64: {}",
							contentURIToBeVerified, expirationTime, tokenSigned, md5Base64
						);

						if (md5Base64 != tokenSigned)
						{
							// non siamo in nessuno dei 3 casi

							string errorMessage = fmt::format(
								"Wrong token (ts)"
								", md5Base64: {}"
								", tokenSigned: {}",
								md5Base64, tokenSigned
							);
							SPDLOG_WARN(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}
			}
		}
		else
		{
			string md5Base64 = getSignedMMSPath(contentURIToBeVerified, expirationTime);

			SPDLOG_INFO(
				"Authorization through path"
				", contentURIToBeVerified: {}"
				", expirationTime: {}"
				", tokenSigned: {}"
				", md5Base64: {}",
				contentURIToBeVerified, expirationTime, tokenSigned, md5Base64
			);

			if (md5Base64 != tokenSigned)
			{
				string errorMessage = fmt::format(
					"Wrong token"
					", md5Base64: {}"
					", tokenSigned: {}",
					md5Base64, tokenSigned
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
		if (expirationTime < utcNow)
		{
			string errorMessage = fmt::format(
				"Token expired"
				", expirationTime: {}"
				", utcNow: {}",
				expirationTime, utcNow
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("Not authorized");
		SPDLOG_WARN(errorMessage);

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		SPDLOG_WARN(errorMessage);

		throw e;
	}

	return tokenComingFromURL;
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

	SPDLOG_INFO(
		"Authorization through path"
		", contentURI: {}"
		", expirationTime: {}"
		", token: {}"
		", md5Base64: {}",
		contentURI, expirationTime, token, md5Base64
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

time_t MMSDeliveryAuthorization::getReusableExpirationTime(int ttlInSeconds)
{
	time_t utcExpirationTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
	utcExpirationTime += ttlInSeconds;

	// arrotondiamo in eccesso al primo secondo del mese successivo
	// Viene usato questo arrotondamento in modo da avere la stessa url signed per far usare la cache alla CDN
	tm tmDateTime;
	localtime_r(&utcExpirationTime, &tmDateTime);

	tmDateTime.tm_mday = 1;
	// Next month 0=Jan
	if (tmDateTime.tm_mon == 11) // Dec
	{
		tmDateTime.tm_mon = 0;
		tmDateTime.tm_year++;
	}
	else
		tmDateTime.tm_mon++;
	tmDateTime.tm_hour = 0;
	tmDateTime.tm_min = 0;
	tmDateTime.tm_sec = 0;

	// Get the first day/hour/second of the next month
	utcExpirationTime = mktime(&tmDateTime);

	return utcExpirationTime;
}
