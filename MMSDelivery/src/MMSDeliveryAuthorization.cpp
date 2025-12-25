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
#include "Convert.h"
#include "CurlWrapper.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "StringUtils.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <exception>
#include <memory>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <regex>
#include <unordered_map>
#include <unordered_set>

MMSDeliveryAuthorization::MMSDeliveryAuthorization(
	const json &configuration, const shared_ptr<MMSStorage> &mmsStorage, const shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade
)
{
	_configuration = configuration;
	_mmsStorage = mmsStorage;
	_mmsEngineDBFacade = mmsEngineDBFacade;

	_keyPairId = JSONUtils::asString(_configuration["aws"], "keyPairId", "");
	SPDLOG_INFO(
		"Configuration item"
		", aws->keyPairId: {}",
		_keyPairId
	);
	_privateKeyPEMPathName = JSONUtils::asString(_configuration["aws"], "privateKeyPEMPathName", "");
	SPDLOG_INFO(
		"Configuration item"
		", aws->privateKeyPEMPathName: {}",
		_privateKeyPEMPathName
	);
	_vodCloudFrontHostName = _configuration["aws"]["vodCloudFrontHostName"];
	SPDLOG_INFO(
		"Configuration item"
		", aws->vodCloudFrontHostName: {}",
		_vodCloudFrontHostName
	);
	_vodDeliveryCloudFrontHostName = _configuration["aws"]["vodDeliveryCloudFrontHostName"];
	SPDLOG_INFO(
		"Configuration item"
		", aws->vodDeliveryCloudFrontHostName: {}",
		_vodDeliveryCloudFrontHostName
	);
	_vodDeliveryPathCloudFrontHostName = _configuration["aws"]["vodDeliveryPathCloudFrontHostName"];
	SPDLOG_INFO(
		"Configuration item"
		", aws->vodDeliveryPathCloudFrontHostName: {}",
		_vodDeliveryPathCloudFrontHostName
	);

	json api = _configuration["api"];

	_deliveryProtocol = JSONUtils::asString(api["delivery"], "deliveryProtocol", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryProtocol: {}",
		_deliveryProtocol
	);
	_deliveryHost_authorizationThroughParameter = JSONUtils::asString(api["delivery"], "deliveryHost_authorizationThroughParameter", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryHost_authorizationThroughParameter: {}",
		_deliveryHost_authorizationThroughParameter
	);
	_deliveryHost_authorizationThroughPath = JSONUtils::asString(api["delivery"], "deliveryHost_authorizationThroughPath", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryHost_authorizationThroughPath: {}",
		_deliveryHost_authorizationThroughPath
	);
}

pair<string, string> MMSDeliveryAuthorization::createDeliveryAuthorization(
	int64_t userKey, const shared_ptr<Workspace>& requestWorkspace, const string& playerIP,

	int64_t mediaItemKey, const string& uniqueName, int64_t encodingProfileKey, const string& encodingProfileLabel,

	int64_t physicalPathKey,

	int64_t ingestionJobKey, int64_t deliveryCode,

	int ttlInSeconds, int maxRetries, bool reuseAuthIfPresent, bool playerIPToBeAuthorized,
	const string& playerCountry, const string& playerRegion,

	bool save,
	// deliveryType:
	// 1. MMS_URLWithTokenAsParam_DB (ex MMS_Token): delivered by MMS with a Token parameter retrieved by DB.
	// Vengono utilizzati i parametri: ttlInSeconds, maxRetries e reuseAuthIfPresent
	// (i.e.: https://mms-delivery.catramms-cloud.com/MMS_0000/47/000/000/280/18657840_source.mp4?token=14)
	//
	// 2. AWSMMS_URLWithTokenAsParam_DB (ex MMS_Token): delivered by AWS getting data from MMS with a Token parameter retrieved by DB (i.e.:
	// https://d3mvdxwkjkh4kh.cloudfront.net/MMS_0000/47/000/000/280/18657840_source.mp4?token=14)
	//
	// 3. MMS_URLWithTokenAsParam_Signed (ex MMS_Token): delivered by MMS with a Token parameter signed (i.e.:
	// https://mms-delivery.catramms-cloud.com/MMS_0000/47/000/000/280/18657840_source.mp4?token=lprk6uoobhZ2GrkWXWvDJA%3D%3D,1822341600)
	//
	// 4. AWSMMS_URLWithTokenAsParam_Signed (ex MMS_Token): delivered by AWS getting data from MMS with a Token parameter signed (i.e.:
	// https://d3mvdxwkjkh4kh.cloudfront.net/MMS_0000/47/000/000/280/18657840_source.mp4?token=lprk6uoobhZ2GrkWXWvDJA%3D%3D,1822341600)
	//
	// 5. MMS_SignedURL: delivered by MMS with a signed URL without parameter (i.e.:
	// https://mms-delivery-path.catramms-cloud.com/token_lprk6uoobhZ2GrkWXWvDJA==,1822341600/MMS_0000/47/000/000/280/18657840_source.mp4)
	//
	// 6. AWSMMS_SignedURL: delivered by AWS getting data from MMS with a signed URL without parameter (i.e.:
	// https://dl4y0maav2axc.cloudfront.net/token_lprk6uoobhZ2GrkWXWvDJA==,1822341600/MMS_0000/47/000/000/280/18657840_source.mp4)
	//
	// 7. AWSCloudFront_Signed: delivered by AWS CloudFront with a signed URL without parameter (i.e.:
	// https://d3mvdxwkjkh4kh.cloudfront.net//MMS_0000/47/000/000/280/18657840_source.mp4?Expires=3578439566&Signature=hiHYmI3~vu5dEhrI6G5xYNSgou1MpTqgNJI08EBinodNYLiqUWi33s4FNd31jARtKAJ~OSHEOKhLCWcE2JGtnEF~g2vasJdI4XWxNvo4G0Dd2R-4wGF2s5IPdhjj6jTkrJC7FXOnPfIve9vUvNdP~eovr~UCFN5jX7yy25b38qqXe5kUXjDHfj6-DMZmUC-uEGzSQT0SOB0Ihtvh9JaE9iBCIsxnwNIPdafMWZOZh9e1Ls70yIXP597-U9d4w~dFchDs3CasAn4ropikBOW3KrEFBCrBO~vdsFgMDHMAyARpqsoYK7WIxq8D3J369utKjNvD8qpVG9XQM6OH127k8g__&Key-Pair-Id=APKAUYWFOBAADUMU4IGK)
	const string& deliveryType,

	bool warningIfMissingMediaItemKey, bool filteredByStatistic, const string& userId
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
			if (!uniqueName.empty() && mediaItemKey == -1)
			{
				// initialize mediaItemKey

				pair<int64_t, MMSEngineDBFacade::ContentType> mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					requestWorkspace->_workspaceKey, uniqueName, warningIfMissingMediaItemKey,
					// 2022-12-18: assumo il MIK sia nel DB da un po di tempo
					false
				);
				tie(mediaItemKey, ignore) = mediaItemKeyDetails;
			}

			if (encodingProfileKey == -1 && !encodingProfileLabel.empty())
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

			AWSSigner awsSigner;
			string signedPlayURL =
				awsSigner.calculateSignedURL(_vodCloudFrontHostName, deliveryURI, _keyPairId, _privateKeyPEMPathName, expirationTime);

			deliveryURL = signedPlayURL;
		}
		else if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_DB" ||
				 deliveryType == "MMS_URLWithTokenAsParam_Signed" || deliveryType == "AWSMMS_URLWithTokenAsParam_Signed")
		{
			string deliveryHost;
			if (deliveryType == "AWSMMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_Signed")
				deliveryHost = _vodDeliveryCloudFrontHostName;
			else // if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "MMS_URLWithTokenAsParam_Signed")
				deliveryHost = _deliveryHost_authorizationThroughParameter;

			if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_DB")
			{
				int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
					userKey, playerIPToBeAuthorized ? playerIP : "", localPhysicalPathKey, -1, deliveryURI, ttlInSeconds, maxRetries,
					reuseAuthIfPresent
				);

				deliveryURL = std::format("{}://{}{}?token={}", _deliveryProtocol, deliveryHost, deliveryURI, authorizationKey);
			}
			else
			{
				time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

				string uriToBeSigned;
				{
					string m3u8Suffix(".m3u8");
					// if (deliveryURI.size() >= m3u8Suffix.size() &&
					// 	0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
					if (deliveryURI.ends_with(m3u8Suffix))
					{
						size_t endPathIndex = deliveryURI.find_last_of('/');
						if (endPathIndex == string::npos)
							uriToBeSigned = deliveryURI;
						else
							uriToBeSigned = deliveryURI.substr(0, endPathIndex);
					}
					else
						uriToBeSigned = deliveryURI;
				}
				string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

				deliveryURL = std::format(
					"{}://{}{}?token={},{}", _deliveryProtocol, deliveryHost, deliveryURI, CurlWrapper::escape(md5Base64), expirationTime
				);
			}

			if (save && !deliveryFileName.empty())
				deliveryURL.append("&deliveryFileName=").append(deliveryFileName);
		}
		else // if (deliveryType == "MMS_SignedURL" || deliveryType == "AWSMMS_SignedURL")
		{
			time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

			string uriToBeSigned;
			{
				string m3u8Suffix(".m3u8");
				// if (deliveryURI.size() >= m3u8Suffix.size() &&
				// 	0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
				if (deliveryURI.ends_with(m3u8Suffix))
				{
					size_t endPathIndex = deliveryURI.find_last_of('/');
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
			if (deliveryType == "AWSMMS_SignedURL")
				deliveryHost = _vodDeliveryPathCloudFrontHostName;
			else // if (deliveryType == "MMS_SignedURL")
				deliveryHost = _deliveryHost_authorizationThroughPath;

			deliveryURL = std::format("{}://{}/token_{},{}{}", _deliveryProtocol, deliveryHost, md5Base64, expirationTime, deliveryURI);
		}

		if (!filteredByStatistic && !userId.empty())
		{
			try
			{
				_mmsEngineDBFacade->addRequestStatistic(
					requestWorkspace->_workspaceKey, playerIP, userId, localPhysicalPathKey,
					-1, // confStreamKey
					title
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = std::format(
					"mmsEngineDBFacade->addRequestStatistic failed"
					", e.what: {}",
					e.what()
				);
				SPDLOG_ERROR(errorMessage);
			}
		}

		SPDLOG_INFO(
			"createDeliveryAuthorization info"
			", title: {}"
			", deliveryURI: {}"
			", deliveryType: {}"
			", mediaItemKey: {}"
			", physicalPathKey: {}"
			", ttlInSeconds: {}"
			", maxRetries: {}"
			", reuseAuthIfPresent: {}"
			", playerIP: {}"
			", playerIPToBeAuthorized: {}"
			", deliveryURL (authorized): {}",
			title, deliveryURI, deliveryType, mediaItemKey, physicalPathKey, ttlInSeconds, maxRetries, reuseAuthIfPresent, playerIP,
			playerIPToBeAuthorized, deliveryURL
		);
	}
	else
	{
		// create authorization for a live request

		auto [ingestionType, ingestionJobRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeMetadataContent(
			requestWorkspace->_workspaceKey, ingestionJobKey,
			// 2022-12-18: it is a live request, it has time to be present into the slave
			false
		);

		if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy && ingestionType != MMSEngineDBFacade::IngestionType::VODProxy &&
			ingestionType != MMSEngineDBFacade::IngestionType::LiveGrid && ingestionType != MMSEngineDBFacade::IngestionType::LiveRecorder &&
			ingestionType != MMSEngineDBFacade::IngestionType::Countdown)
		{
			string errorMessage = std::format(
				"ingestionJob is not a LiveProxy"
				", ingestionType: {}",
				MMSEngineDBFacade::toString(ingestionType)
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy || ingestionType == MMSEngineDBFacade::IngestionType::VODProxy ||
			ingestionType == MMSEngineDBFacade::IngestionType::Countdown || ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
		{
			string field = "outputs";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage = std::format(
					"A Proxy/Countdown/Recorder without outputs cannot be delivered"
					", ingestionJobKey: {}",
					ingestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json outputsRoot = JSONUtils::asJson(ingestionJobRoot, "outputs", json::array());

			// Option 1: OutputType HLS with deliveryCode
			// Option 2: OutputType RTMP_Channel/CDN_AWS/CDN_CDN77 with playURL
			// tuple<string, int64_t, string> means OutputType, deliveryCode, playURL
			vector<tuple<string, int64_t, string>> outputDeliveryOptions;
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				const json& outputRoot = outputsRoot[outputIndex];

				string outputType;
				int64_t localDeliveryCode = -1;
				string playURL;

				outputType = JSONUtils::asString(outputRoot, "outputType", "HLS_Channel");

				if (outputType == "CDN_CDN77")
				{
					try
					{
						auto [resourceURL, filePath, secureToken] = _mmsEngineDBFacade->cdn77_reservationDetails(ingestionJobKey, outputIndex);

						if (!filePath.empty() && filePath.front() != '/')
							filePath.insert(filePath.begin(), '/');

						if (!secureToken.empty())
							playURL = getSignedCDN77URL(resourceURL, filePath, secureToken, ttlInSeconds, playerIPToBeAuthorized ? playerIP : "");
						else
							playURL = std::format("https://{}{}", resourceURL, filePath);
					}
					catch (DBRecordNotFound &e)
					{
						SPDLOG_ERROR(
							"ingestionJobKey/outputIndex not found failed"
							", ingestionJobKey: {}"
							", outputIndex: {}"
							", exception: {}",
							ingestionJobKey, outputIndex, e.what()
						);
						continue;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"getSignedCDN77URL failed"
							", ingestionJobKey: {}"
							", outputIndex: {}"
							", exception: {}",
							ingestionJobKey, outputIndex, e.what()
						);
						continue;
					}

					if (playURL.empty())
						continue;
				}
				/*
				else if (outputType == "CDN_AWS")
				{
					try
					{
						bool awsSignedURL = JSONUtils::asBool(outputRoot, "awsSignedURL", false);
						playURL = _mmsEngineDBFacade->cdnaws_reservationDetails(ingestionJobKey, outputIndex);

						if (awsSignedURL)
							playURL = getAWSSignedURL(playURL, ttlInSeconds);
					}
					catch (DBRecordNotFound &e)
					{
						SPDLOG_ERROR(
							"ingestionJobKey/outputIndex not found failed"
							", ingestionJobKey: {}"
							", outputIndex: {}"
							", exception: {}",
							ingestionJobKey, outputIndex, e.what()
						);
						continue;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"getSignedCDN77URL failed"
							", ingestionJobKey: {}"
							", outputIndex: {}"
							", exception: {}",
							ingestionJobKey, outputIndex, e.what()
						);
						continue;
					}

					if (playURL.empty())
						continue;
				}
				*/
				else if (outputType == "RTMP_Channel")
				{
					try
					{
						/* playURLDetailsRoot:
						{
							"securityType": "token",
							"cdnName": "medianova", // "aws"
							"playURLProtocol": "https",
							"playURLHostName": "cibortv-live.lg.mncdn.com", // "d1nue3l1x0sz90.cloudfront.net"
							"uri": "/mn-d1/dmax/index.m3u8", // /out/v1/ca8fd629f9204ca38daf18f04187c694/index.m3u8
							"token": "svFTvs7d",
							"medianova":
							{
								"uriEnabled": true,
								"playerIPEnabled": false
							},
							"aws":
							{
								"channelId": "...."
							}
						}
						*/
						json playURLDetailsRoot = _mmsEngineDBFacade->rtmp_reservationDetails(ingestionJobKey, outputIndex);

						string securityType = JSONUtils::asString(playURLDetailsRoot, "securityType", "none");
						string cdnName = JSONUtils::asString(playURLDetailsRoot, "cdnName", "");
						string playURLProtocol = JSONUtils::asString(playURLDetailsRoot, "playURLProtocol", "https");
						string playURLHostName = JSONUtils::asString(playURLDetailsRoot, "playURLHostName", "");
						string uri = JSONUtils::asString(playURLDetailsRoot, "uri", "");
						if (securityType == "token")
						{
							string secureToken = JSONUtils::asString(playURLDetailsRoot, "token", "");
							if (cdnName == "medianova")
							{
								json medianovaRoot = JSONUtils::asJson(playURLDetailsRoot, "medianova", json(nullptr));
								// uriEnabled deve essere consistente con la conf. in Medianova (security->Security Token->URI)
								bool uriEnabled = JSONUtils::asBool(medianovaRoot, "uriEnabled", false);
								// playerIPEnabled deve essere consistente con la conf. in Medianova (se ho capito bene dobbiamo
								// chiedere Medianova di abilitare/disattivare questa opzione)
								bool playerIPEnabled = JSONUtils::asBool(medianovaRoot, "playerIPEnabled", false);

								playURL = getMedianovaSignedTokenURL(
									playURLProtocol, playURLHostName, uri, secureToken, ttlInSeconds, playerIP,
									uriEnabled, playerIPToBeAuthorized && playerIPEnabled
								);
							}
							else if (cdnName == "aws")
							{
								AWSSigner awsSigner;
								playURL = awsSigner.calculateSignedURL(playURLHostName, uri, _keyPairId, _privateKeyPEMPathName, ttlInSeconds);
							}
							else
								SPDLOG_ERROR(
									"cdnName unknown"
									", ingestionJobKey: {}"
									", outputIndex: {}"
									", cdnName: {}",
									ingestionJobKey, outputIndex, cdnName
								);
						}
						else
							playURL = std::format("{}://{}{}", playURLProtocol, playURLHostName, uri);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"rtmp_reservationDetails failed"
							", ingestionJobKey: {}"
							", outputIndex: {}"
							", exception: {}",
							ingestionJobKey, outputIndex, e.what()
						);
						continue;
					}

					if (playURL.empty())
						continue;
				}
				else if (outputType == "SRT_Channel")
				{
					try
					{
						playURL = _mmsEngineDBFacade->srt_reservationDetails(ingestionJobKey, outputIndex);
					}
					catch (DBRecordNotFound &e)
					{
						SPDLOG_ERROR(
							"ingestionJobKey/outputIndex not found failed"
							", ingestionJobKey: {}"
							", outputIndex: {}"
							", exception: {}",
							ingestionJobKey, outputIndex, e.what()
						);
						continue;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"srt_reservationDetails failed"
							", ingestionJobKey: {}"
							", outputIndex: {}"
							", exception: {}",
							ingestionJobKey, outputIndex, e.what()
						);
						continue;
					}

					if (playURL.empty())
						continue;
				}
				else if (outputType == "HLS_Channel")
				{
					localDeliveryCode = JSONUtils::asInt64(outputRoot, "deliveryCode", -1);
				}

				SPDLOG_INFO(
					"createDeliveryAuthorization"
					", ingestionJobKey: {}"
					", outputIndex: {}"
					", outputType: {}"
					", playURL: {}"
					", localDeliveryCode: {}",
					ingestionJobKey, outputIndex, outputType, playURL, localDeliveryCode
				);
				outputDeliveryOptions.emplace_back(outputType, localDeliveryCode, playURL);
			}

			string outputType;
			string playURL;

			if (deliveryCode == -1) // requested delivery code (it is an input)
			{
				if (outputDeliveryOptions.empty())
				{
					string errorMessage = std::format(
						"No outputDeliveryOptions, it cannot be delivered"
						", ingestionJobKey: {}",
						ingestionJobKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				if (outputDeliveryOptions.size() > 1)
				{
					string errorMessage = std::format(
						"Live authorization with several option. Just get the first"
						", ingestionJobKey: {}",
						ingestionJobKey
					);
					SPDLOG_WARN(errorMessage);

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

				for (const tuple<string, int64_t, string>& outputDeliveryOption : outputDeliveryOptions)
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
					string errorMessage = std::format(
						"DeliveryCode received does not exist for the ingestionJob"
						", ingestionJobKey: {}",
						ingestionJobKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			if (outputType == "RTMP_Channel" || outputType == "SRT_Channel" || outputType == "CDN_AWS" || outputType == "CDN_CDN77")
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

				if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_DB" ||
					deliveryType == "MMS_URLWithTokenAsParam_Signed" || deliveryType == "AWSMMS_URLWithTokenAsParam_Signed")
				{
					if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_DB")
					{
						int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
							userKey, playerIPToBeAuthorized ? playerIP : "",
							-1,			  // physicalPathKey,	vod key
							deliveryCode, // live key
							deliveryURI, ttlInSeconds, maxRetries, reuseAuthIfPresent
						);

						deliveryURL = std::format(
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
							if (deliveryURI.ends_with(m3u8Suffix))
							{
								size_t endPathIndex = deliveryURI.find_last_of('/');
								if (endPathIndex == string::npos)
									uriToBeSigned = deliveryURI;
								else
									uriToBeSigned = deliveryURI.substr(0, endPathIndex);
							}
							else
								uriToBeSigned = deliveryURI;
						}
						string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

						deliveryURL = std::format(
							"{}://{}{}?token={},{}", _deliveryProtocol,
							getDeliveryHost(requestWorkspace, playerCountry, playerRegion, _deliveryHost_authorizationThroughParameter), deliveryURI,
							CurlWrapper::escape(md5Base64), expirationTime
						);
					}
				}
				else // if (deliveryType == "MMS_SignedURL" || deliveryType == "AWSMMS_SignedURL")
				{
					time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

					string uriToBeSigned;
					{
						string m3u8Suffix(".m3u8");
						if (deliveryURI.size() >= m3u8Suffix.size() &&
							0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
						{
							size_t endPathIndex = deliveryURI.find_last_of('/');
							if (endPathIndex == string::npos)
								uriToBeSigned = deliveryURI;
							else
								uriToBeSigned = deliveryURI.substr(0, endPathIndex);
						}
						else
							uriToBeSigned = deliveryURI;
					}
					string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

					deliveryURL = _deliveryProtocol + "://" +
								  getDeliveryHost(requestWorkspace, playerCountry, playerRegion, _deliveryHost_authorizationThroughPath) + "/token_" +
								  md5Base64 + "," + to_string(expirationTime) + deliveryURI;
				}
				/*
				else
				{
					string errorMessage = string("wrong deliveryType")
						+ ", deliveryType: " + deliveryType
					;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				*/
			}
			else
			{
				string deliveryURI;
				string liveFileExtension = "mpd";

				tuple<string, string, string> liveDeliveryDetails =
					_mmsStorage->getLiveDeliveryDetails(to_string(deliveryCode), liveFileExtension, requestWorkspace);
				tie(deliveryURI, ignore, deliveryFileName) = liveDeliveryDetails;

				if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_DB" ||
					deliveryType == "MMS_URLWithTokenAsParam_Signed" || deliveryType == "AWSMMS_URLWithTokenAsParam_Signed")
				{
					if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_DB")
					{
						int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
							userKey, playerIPToBeAuthorized ? playerIP : "",
							-1,			  // physicalPathKey,	vod key
							deliveryCode, // live key
							deliveryURI, ttlInSeconds, maxRetries, reuseAuthIfPresent
						);

						deliveryURL = std::format(
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
							if (deliveryURI.ends_with(m3u8Suffix))
							{
								size_t endPathIndex = deliveryURI.find_last_of('/');
								if (endPathIndex == string::npos)
									uriToBeSigned = deliveryURI;
								else
									uriToBeSigned = deliveryURI.substr(0, endPathIndex);
							}
							else
								uriToBeSigned = deliveryURI;
						}
						string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

						deliveryURL = std::format(
							"{}://{}{}?token={},{}", _deliveryProtocol, _deliveryHost_authorizationThroughParameter, deliveryURI,
							CurlWrapper::escape(md5Base64), expirationTime
						);
					}
				}
				else // if (deliveryType == "MMS_SignedURL" || deliveryType == "AWSMMS_SignedURL")
				{
					time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

					string uriToBeSigned;
					{
						string m3u8Suffix(".m3u8");
						if (deliveryURI.size() >= m3u8Suffix.size() &&
							0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
						{
							size_t endPathIndex = deliveryURI.find_last_of('/');
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
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				*/
			}

			try
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage = std::format(
						"{} field missing"
						", ingestionJobKey: {}",
						field, ingestionJobKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string configurationLabel = JSONUtils::asString(ingestionJobRoot, field, "");

				int64_t streamConfKey = _mmsEngineDBFacade->stream_columnAsInt64(requestWorkspace->_workspaceKey, "confKey", -1, configurationLabel);
				/*
				bool warningIfMissing = false;
				tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
					streamDetails = _mmsEngineDBFacade->getStreamDetails(requestWorkspace->_workspaceKey, configurationLabel, warningIfMissing);

				int64_t streamConfKey;
				tie(streamConfKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore) = streamDetails;
				*/

				_mmsEngineDBFacade->addRequestStatistic(
					requestWorkspace->_workspaceKey, playerIP, userId,
					-1, // localPhysicalPathKey,
					streamConfKey, configurationLabel
				);
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					"mmsEngineDBFacade->addRequestStatistic failed"
					", e.what: {}",
					e.what()
				);
			}
		}
		else // if (ingestionType != MMSEngineDBFacade::IngestionType::LiveGrid)
		{
			string field = "DeliveryCode";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage = std::format(
					"A LiveGrid without DeliveryCode cannot be delivered"
					", ingestionJobKey: {}",
					ingestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			deliveryCode = JSONUtils::asInt64(ingestionJobRoot, field, 0);

			string deliveryURI;
			string liveFileExtension = "m3u8";
			tuple<string, string, string> liveDeliveryDetails =
				_mmsStorage->getLiveDeliveryDetails(to_string(deliveryCode), liveFileExtension, requestWorkspace);
			tie(deliveryURI, ignore, deliveryFileName) = liveDeliveryDetails;

			if (deliveryType == "MMS_URLWithTokenAsParam_DB" || deliveryType == "AWSMMS_URLWithTokenAsParam_DB" ||
				deliveryType == "MMS_URLWithTokenAsParam_Signed" || deliveryType == "AWSMMS_URLWithTokenAsParam_Signed")
			{
				int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
					userKey, playerIPToBeAuthorized ? playerIP : "",
					-1, // physicalPathKey,
					deliveryCode, deliveryURI, ttlInSeconds, maxRetries, reuseAuthIfPresent
				);

				deliveryURL = std::format("{}://{}{}?token{}",
					_deliveryProtocol, _deliveryHost_authorizationThroughParameter, deliveryURI, authorizationKey);
			}
			else // if (deliveryType == "MMS_SignedURL" || deliveryType == "AWSMMS_SignedURL")
			{
				time_t expirationTime = getReusableExpirationTime(ttlInSeconds);

				string uriToBeSigned;
				{
					string m3u8Suffix(".m3u8");
					if (deliveryURI.size() >= m3u8Suffix.size() &&
						0 == deliveryURI.compare(deliveryURI.size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
					{
						size_t endPathIndex = deliveryURI.find_last_of('/');
						if (endPathIndex == string::npos)
							uriToBeSigned = deliveryURI;
						else
							uriToBeSigned = deliveryURI.substr(0, endPathIndex);
					}
					else
						uriToBeSigned = deliveryURI;
				}
				string md5Base64 = getSignedMMSPath(uriToBeSigned, expirationTime);

				deliveryURL = std::format("{}://{}/token_{},{}{}", _deliveryProtocol, _deliveryHost_authorizationThroughPath,
					md5Base64, expirationTime, deliveryURI);
			}
			/*
			else
			{
				string errorMessage = string("wrong deliveryType")
					+ ", deliveryType: " + deliveryType
				;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			*/

			SPDLOG_INFO(
				"createDeliveryAuthorization for LiveGrid"
				", ingestionJobKey: {}"
				", deliveryCode: {}"
				", deliveryURL: {}",
				ingestionJobKey, deliveryCode, deliveryURL
			);
		}
	}

	return make_pair(deliveryURL, deliveryFileName);
}

string MMSDeliveryAuthorization::getDeliveryHost(
	const shared_ptr<Workspace>& requestWorkspace, const string& playerCountry, const string& playerRegion, const string& defaultDeliveryHost
)
{
	string deliveryHost = defaultDeliveryHost;
	if (!playerCountry.empty())
	{
		// verifica se abbiamo externalDeliveries per questo specifico playerCountry-playerRegion
		/*
		{"HLS-live": {
		"hostGroups": {
			"group-1": [
				{ "host": "srv-1.cibortvlive.com", "running": true },
				{ "host": "srv-2.cibortvlive.com", "running": true }
			],
			"group-1": [
				{ "host": "srv-1.cibortvlive.com", "running": true },
				{ "host": "srv-2.cibortvlive.com", "running": true }
			],
			"default": [
				{ "host": "srv-3.cibortvlive.com", "running": true },
				{ "host": "srv-4.cibortvlive.com", "running": false }
			]
		},
		"countryMap": {	// each location could be: country["-region"]
			"US-PA": "group-1",
			"CA": "group-1",
			"BR": "group-2",
			"AR": "group-2",
			"IT": "group-1"
		}
		}}
		*/

		json hlsLiveRoot = JSONUtils::asJson(requestWorkspace->_externalDeliveriesRoot, "HLS-live", json());

		json countryMapRoot = JSONUtils::asJson(hlsLiveRoot, "countryMap", json());
		json hostGroupsRoot = JSONUtils::asJson(hlsLiveRoot, "hostGroups", json());

		string externalDeliveriesGroup = "default";
		if (!playerRegion.empty())
			externalDeliveriesGroup = JSONUtils::asString(countryMapRoot, std::format("{}-{}", playerCountry, playerRegion), "default");
		if (externalDeliveriesGroup == "default")
			externalDeliveriesGroup = JSONUtils::asString(countryMapRoot, playerCountry, "default");

		json hostGroupRoot = JSONUtils::asJson(hostGroupsRoot, externalDeliveriesGroup, json::array());
		if (!hostGroupRoot.empty())
		{
			shared_ptr<HostBandwidthTracker> hostBandwidthTracker =
				getHostBandwidthTracker(requestWorkspace->_workspaceKey, externalDeliveriesGroup, hostGroupRoot);
			optional<string> deliveryHostOpt = hostBandwidthTracker->getMinBandwidthHost();
			if (deliveryHostOpt.has_value())
			{
				deliveryHost = deliveryHostOpt.value();
				// aggiungiamo una banda fittizia occupata da questa richiesta in attesa
				// che la banda reala venga aggiornata periodicamente
				// Questo per evitare che tutte le richieste andranno tutte sullo stesso host in attesa che bandwidthUsage si aggiorni
				// 2 sembra troppo poco perchè si crea troppa differenza di banda tra i server (da 266 Mbps fino a 725 Mbps)
				uint64_t bandwidth = 3 * 1000000;
				hostBandwidthTracker->addBandwidth(deliveryHost, bandwidth);
			}
		}
	}

	SPDLOG_INFO(
		"getDeliveryHost"
		", playerCountry: {}"
		", playerRegion: {}"
		", deliveryHost: {}",
		// ", externalDeliveries: {}",
		playerCountry, playerRegion, deliveryHost
		// JSONUtils::toString(requestWorkspace->_externalDeliveriesRoot),
	);

	return deliveryHost;
}

shared_ptr<HostBandwidthTracker> MMSDeliveryAuthorization::getHostBandwidthTracker(int64_t workspaceKey, const string& groupName,
	const json &hostGroupRoot)
{
	lock_guard<mutex> locker(_externalDeliveriesMutex);

	shared_ptr<HostBandwidthTracker> hostBandwidthTracker;

	string key = std::format("{}-{}", workspaceKey, groupName);
	auto it = _externalDeliveriesGroups.find(key);
	if (it == _externalDeliveriesGroups.end())
	{
		hostBandwidthTracker = make_shared<HostBandwidthTracker>();
		_externalDeliveriesGroups.insert(pair<string, shared_ptr<HostBandwidthTracker>>(key, hostBandwidthTracker));
	}
	else
		hostBandwidthTracker = it->second;
	hostBandwidthTracker->updateHosts(hostGroupRoot);

	// externalDeliveries ed il flag running sono recuperati dal DB
	// Se volessimo disabilitare un externalDelivery perchè ad es. bisogna fare manutenzione,
	// è sufficiente mettere il flag running a false

	return hostBandwidthTracker;
}

unordered_map<string, uint64_t> MMSDeliveryAuthorization::getExternalDeliveriesRunningHosts()
{
	unordered_map<string, uint64_t> hostsBandwidth;

	lock_guard<mutex> locker(_externalDeliveriesMutex);

	if (!_externalDeliveriesGroups.empty())
	{
		unordered_set<string> hosts;
		for (const auto &[key, hostBandwidthTracker] : _externalDeliveriesGroups)
			hostBandwidthTracker->addRunningHosts(hosts);

		for (const string& host : hosts)
			hostsBandwidth.insert(make_pair(host, 0));
	}

	return hostsBandwidth;
}

void MMSDeliveryAuthorization::updateExternalDeliveriesBandwidthHosts(const unordered_map<string, uint64_t>& hostsBandwidth)
{

	lock_guard<mutex> locker(_externalDeliveriesMutex);

	for (const auto &hostBandwidthTracker : _externalDeliveriesGroups | views::values)
	{
		for (const auto &[host, hostBandwidth] : hostsBandwidth)
			hostBandwidthTracker->updateBandwidth(host, hostBandwidth);
	}
}

string MMSDeliveryAuthorization::checkDeliveryAuthorizationThroughParameter(const string& contentURI, const string& tokenParameter)
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
				string errorMessage = std::format(
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
				// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
				//	That  because if we have really a + char (%2B into the string), and we do the replace
				//	after unescape, this char will be changed to space and we do not want it
				string plus = "\\+";
				string plusDecoded = " ";
				string firstDecoding = regex_replace(firstPartOfToken, regex(plus), plusDecoded);

				firstPartOfToken = CurlWrapper::unescape(firstDecoding);
			}
			secondPartOfToken = tokenParameter.substr(endOfTokenIndex + separator.length());
		}

		// end with
		string tsExtension(".ts");	   // hls
		string m4sExtension(".m4s");   // dash
		string m3u8Extension(".m3u8"); // m3u8
		if (!secondPartOfToken.empty()  // secondPartOfToken is the cookie
			&& (contentURI.ends_with(tsExtension) || contentURI.ends_with(m4sExtension) || contentURI.ends_with(m3u8Extension)))
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

			if (cookie.empty())
			{
				string errorMessage = std::format(
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
					string errorMessage = std::format(
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
				string errorMessage = std::format(
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

			if (StringUtils::isNumber(firstPartOfToken)) // MMS_URLWithTokenAsParam_DB || AWSMMS_URLWithTokenAsParam_DB
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
					string errorMessage = std::format(
						"Not authorized: token invalid"
						", tokenComingFromURL: {}",
						tokenComingFromURL
					);
					SPDLOG_WARN(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else // MMS_URLWithTokenAsParam_Signed || AWSMMS_URLWithTokenAsParam_Signed
			{
				const string& tokenSigned = firstPartOfToken;
				checkSignedMMSPath(tokenSigned, contentURI);
			}
		}
	}
	catch (exception &e)
	{
		string errorMessage = "Not authorized: exception managing token";
		SPDLOG_WARN(errorMessage);

		throw;
	}

	return tokenComingFromURL;
}

int64_t MMSDeliveryAuthorization::checkDeliveryAuthorizationThroughPath(const string& contentURI)
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
			string errorMessage = std::format(
				"Wrong token format"
				", contentURI: {}",
				contentURI
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		startTokenIndex += tokenLabel.size();

		size_t endTokenIndex = contentURI.find(',', startTokenIndex);
		if (endTokenIndex == string::npos)
		{
			string errorMessage = std::format(
				"Wrong token format"
				", contentURI: {}",
				contentURI
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t endExpirationIndex = contentURI.find('/', endTokenIndex);
		if (endExpirationIndex == string::npos)
		{
			string errorMessage = std::format(
				"Wrong token format"
				", contentURI: {}",
				contentURI
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		string tokenSigned = contentURI.substr(startTokenIndex, endExpirationIndex - startTokenIndex);

		string contentURIToBeVerified;

		size_t endContentURIIndex = contentURI.find('?', endExpirationIndex);
		if (endContentURIIndex == string::npos)
			contentURIToBeVerified = contentURI.substr(endExpirationIndex);
		else
			contentURIToBeVerified = contentURI.substr(endExpirationIndex, endContentURIIndex - endExpirationIndex);

		return checkSignedMMSPath(tokenSigned, contentURIToBeVerified);
	}
	catch (exception &e)
	{
		string errorMessage = "Not authorized: exception managing token";
		SPDLOG_WARN(errorMessage);

		throw;
	}

	return tokenComingFromURL;
}

string MMSDeliveryAuthorization::checkDeliveryAuthorizationOfAManifest(bool secondaryManifest, const string& token, const string& cookie,
	const string& contentURI)
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
				// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
				//	That  because if we have really a + char (%2B into the string), and we do the replace
				//	after unescape, this char will be changed to space and we do not want it
				string plus = "\\+";
				string plusDecoded = " ";
				string firstDecoding = regex_replace(tokenIt->second, regex(plus), plusDecoded);

				tokenParameter = CurlWrapper::unescape(firstDecoding);
			}
			tokenParameter = std::format("{}---{}", tokenParameter, cookie);
			*/
			string tokenParameter = std::format("{}---{}", token, cookie);
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

			if (cookie.empty())
			{
				if (StringUtils::isNumber(token)) // MMS_URLWithTokenAsParam_DB || AWSMMS_URLWithTokenAsParam_DB
				{
					if (!_mmsEngineDBFacade->checkDeliveryAuthorization(stoll(token), contentURI))
					{
						string errorMessage = std::format(
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
						// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
						//	That  because if we have really a + char (%2B into the string), and we do the replace
						//	after unescape, this char will be changed to space and we do not want it
						string plus = "\\+";
						string plusDecoded = " ";
						string firstDecoding = regex_replace(token, regex(plus), plusDecoded);

						tokenComingFromURL = CurlWrapper::unescape(firstDecoding);
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
					string errorMessage = std::format(
						"cookie invalid, let's check the token"
						", sTokenComingFromCookie: {}"
						", tokenComingFromURL: {}",
						sTokenComingFromCookie, tokenComingFromURL
					);
					SPDLOG_INFO(errorMessage);

					if (StringUtils::isNumber(tokenComingFromURL)) // MMS_URLWithTokenAsParam_DB || AWSMMS_URLWithTokenAsParam_DB
					{
						if (!_mmsEngineDBFacade->checkDeliveryAuthorization(stoll(tokenComingFromURL), contentURI))
						{
							string errorMessage = std::format(
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
	catch (exception &e)
	{
		string errorMessage = "Not authorized: exception managing token";
		SPDLOG_WARN(errorMessage);

		throw;
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

		size_t endTokenIndex = tokenSigned.find(',');
		if (endTokenIndex == string::npos)
		{
			string errorMessage = std::format(
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
		if (contentURIToBeVerified.ends_with(m3u8Suffix))
		{
			{
				size_t endPathIndex = contentURIToBeVerified.find_last_of('/');
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
					size_t endPathIndex = contentURIToBeVerified.find_last_of('/');
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
					string errorMessage = std::format(
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
		else if (contentURIToBeVerified.ends_with(tsSuffix))
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
						size_t endPathIndex = contentURIToBeVerified.find_last_of('/');
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
							size_t endPathIndex = contentURIToBeVerified.find_last_of('/');
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

							string errorMessage = std::format(
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
				string errorMessage = std::format(
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
			string errorMessage = std::format(
				"Token expired"
				", expirationTime: {}"
				", utcNow: {}",
				expirationTime, utcNow
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = "Not authorized: exception managing token";
		SPDLOG_WARN(errorMessage);

		throw;
	}

	return tokenComingFromURL;
}

string MMSDeliveryAuthorization::getSignedMMSPath(const string& contentURI, time_t expirationTime)
{
	string token = std::format("{}{}", expirationTime, contentURI);
	string md5Base64;
	{
		// unsigned char digest[MD5_DIGEST_LENGTH];
		// MD5((unsigned char*) token.c_str(), token.size(), digest);
		// md5Base64 = Convert::base64_encode(digest, MD5_DIGEST_LENGTH);
		{
			unsigned int md5_digest_len = EVP_MD_size(EVP_md5());

			// MD5_Init
			EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
			EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

			// MD5_Update
			EVP_DigestUpdate(mdctx, (unsigned char *)token.c_str(), token.size());

			// MD5_Final
			auto *md5_digest = static_cast<unsigned char *>(OPENSSL_malloc(md5_digest_len));
			EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);

			md5Base64 = Convert::base64_encode(md5_digest, md5_digest_len);

			OPENSSL_free(md5_digest);

			EVP_MD_CTX_free(mdctx);
		}

		ranges::transform(
			md5Base64, md5Base64.begin(),
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
	const string& resourceURL, // i.e.: 1234456789.rsc.cdn77.org
	const string& filePath,	// /file/playlist/d.m3u8
	const string& secureToken, long expirationInSeconds, string playerIP
)
{
	SPDLOG_INFO(
		"getSignedCDN77URL"
		", resourceURL: {}"
		", filePath: {}"
		", secureToken: {}"
		", expirationInSeconds: {}"
		", playerIP: {}",
		resourceURL, filePath, secureToken, expirationInSeconds, playerIP
	);

	try
	{
		//  It's smart to set the expiration time as current time plus 5 minutes { time() + 300}.
		//  This way the link will be available only for the time needed to start the download.

		long expiryTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now()) + expirationInSeconds;

		string signedURL;
		{
			string strippedPath;
			{
				// because of hls/dash, anything included after the last slash (e.g. playlist/{chunk}) shouldn't be part of the path string,
				// for which we generate the secure token. Because of that, everything included after the last slash is stripped.
				// $strippedPath = substr($filePath, 0, strrpos($filePath, '/'));
				size_t fileNameStart = filePath.find_last_of('/');
				if (fileNameStart == string::npos)
				{
					string errorMessage = std::format(
						"filePath format is wrong"
						", filePath: {}",
						filePath
					);
					SPDLOG_ERROR(errorMessage);

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
			string hashStr =
				playerIP.empty() ? std::format("{}{}", strippedPath, secureToken) : std::format("{}{} {}", strippedPath, playerIP, secureToken);

			// if ($expiryTimestamp) {
			// 	$hashStr = $expiryTimestamp . $hashStr;
			// 	$expiryTimestamp = ',' . $expiryTimestamp;
			// }
			hashStr = to_string(expiryTimestamp) + hashStr;
			string sExpiryTimestamp = std::format(",{}", expiryTimestamp);

			SPDLOG_INFO(
				"getSignedCDN77URL"
				", strippedPath: {}"
				", hashStr: {}"
				", sExpiryTimestamp: {}",
				strippedPath, hashStr, sExpiryTimestamp
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
					md5_digest = static_cast<unsigned char *>(OPENSSL_malloc(md5_digest_len));
					EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);

					/*
					{
						string md5 = string((char *) md5_digest, md5_digest_len);

						info(__FILEREF__ + "getSignedCDN77URL"
							+ ", hashStr: " + hashStr
							+ ", md5_digest_len: " + to_string(md5_digest_len)
							+ ", md5: " + md5
						);
					}
					*/

					md5Base64 = Encrypt::binaryToBase64(md5_digest, md5_digest_len);
					/*
					{
						BIO *bio, *b64;
						BUF_MEM *bufferPtr;

						SPDLOG_DEBUG("BIO_new...");
						b64 = BIO_new(BIO_f_base64());
						// By default there must be a newline at the end of input.
						// Next flag remove new line at the end
						BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
						bio = BIO_new(BIO_s_mem());

						SPDLOG_DEBUG("BIO_push...");
						bio = BIO_push(b64, bio);

						SPDLOG_DEBUG("BIO_write...");
						BIO_write(bio, md5_digest, md5_digest_len);
						BIO_flush(bio);
						BIO_get_mem_ptr(bio, &bufferPtr);

						SPDLOG_DEBUG("BIO_set_close...");
						BIO_set_close(bio, BIO_NOCLOSE);
						SPDLOG_DEBUG("BIO_free_all...");
						BIO_free_all(bio);
						// info(__FILEREF__ + "BIO_free...");
						// BIO_free(b64);	// useless because of BIO_free_all

						SPDLOG_DEBUG("base64Text set...");
						// char* base64Text=(*bufferPtr).data;

						SPDLOG_INFO(
							"getSignedCDN77URL"
							", (*bufferPtr).length: {}"
							", (*bufferPtr).data: {}",
							bufferPtr->length, bufferPtr->data
						);

						md5Base64 = string(bufferPtr->data, bufferPtr->length);

						BUF_MEM_free(bufferPtr);
					}
					*/

					OPENSSL_free(md5_digest);

					EVP_MD_CTX_free(mdctx);
				}

				SPDLOG_INFO(
					"getSignedCDN77URL"
					", md5Base64: {}",
					md5Base64
				);

				// $invalidChars = ['+','/'];
				// $validChars = ['-','_'];
				ranges::transform(
					md5Base64, md5Base64.begin(),
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

			signedURL = std::format("https://{}/{}{}{}", resourceURL, md5Base64, sExpiryTimestamp, filePath);
		}

		SPDLOG_INFO(
			"end getSignedCDN77URL"
			", resourceURL: {}"
			", filePath: {}"
			", secureToken: {}"
			", expirationInSeconds: {}"
			", playerIP: {}"
			", signedURL: {}",
			resourceURL, filePath, secureToken, expirationInSeconds, playerIP, signedURL
		);

		return signedURL;
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"getSignedCDN77URL failed"
			", e.what(): {}",
			e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

string MMSDeliveryAuthorization::getMedianovaSignedTokenURL(
	const string& playURLProtocol, // i.e.: https
	const string& playURLHostname, // i.e.: origin-l-backup.glb.mncdn.com
	const string& uri,	// /mn-m1/cnl52/index.m3u8
	const string& secureToken, // i.e.: svFTvs7d
	long expirationInSeconds,  // 3600
	const string& playerIP,
	const bool uriEnabled,
	const bool playerIPEnabled
)
{
	SPDLOG_INFO(
		"https://test-cibortv-live.lg.mncdn.com/mn-m1/cnl52/index.m3u8?st=UKs6348dQptKUc8ShW-qdA&e=1829660036"
		", playURLProtocol: {}"
		", playURLHostname: {}"
		", uri: {}"
		", secureToken: {}"
		", expirationInSeconds: {}"
		", playerIP: {}"
		", uriEnabled: {}"
		", playerIPEnabled: {}",
		playURLProtocol, playURLHostname, uri, secureToken, expirationInSeconds, playerIP, uriEnabled, playerIPEnabled
	);

	try
	{
		//  It's smart to set the expiration time as current time plus 5 minutes { time() + 300}.
		//  This way the link will be available only for the time needed to start the download.

		long expiryTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now()) + expirationInSeconds;

		optional<string> newUri = nullopt;
		if (uriEnabled)
			newUri = StringUtils::uriPathPrefix(uri) + "/";

		// The order is important, fields: expiryTimestamp, secureToken, [playerIP], [newUri], [any query parameters]
		// If userID is added, the signed URL should have also the 'userid' parameter (&userid=$user_id)
		string toSign = std::format("{} {}{}{}", expiryTimestamp, secureToken,
			playerIPEnabled ? std::format(" {}", playerIP) : "",
			newUri ? std::format(" {}", *newUri) : "");
		SPDLOG_INFO(
			"getMedianovaSignedTokenURL"
			", toSign: {}",
			toSign
		);

		unsigned int len;
		const auto md5Digest = Encrypt::md5(toSign, len);
		SPDLOG_INFO(
			"getMedianovaSignedTokenURL digest"
			"len: {}", len
		);

		string md5Base64 = Encrypt::binaryToBase64(md5Digest.data(), len);
		SPDLOG_INFO(
			"getMedianovaSignedTokenURL"
			", md5Base64: {}",
			md5Base64
		);

		md5Base64 = StringUtils::replaceAll(md5Base64, "=", "");
		ranges::transform(
			md5Base64, md5Base64.begin(),
			[](unsigned char c)
			{
				if (c == '+')
					return '-';
				if (c == '/')
					return '_';
				return static_cast<char>(c);
			}
		);

		string signedURL = std::format("{}://{}{}?st={}&e={}", playURLProtocol, playURLHostname, uri, md5Base64, expiryTimestamp);

		SPDLOG_INFO(
			"end getMedianovaSignedTokenURL"
			", playURLProtocol: {}"
			", playURLHostname: {}"
			", uri: {}"
			", secureToken: {}"
			", expirationInSeconds: {}"
			", playerIP: {}"
			", uriEnabled: {}"
			", playerIPEnabled: {}"
			", signedURL: {}",
			playURLProtocol, playURLHostname, uri, secureToken, expirationInSeconds, playerIP, uriEnabled, playerIPEnabled, signedURL
		);

		return signedURL;
	}
	catch (exception &e)
	{
		const string errorMessage = std::format(
			"getMedianovaSignedTokenURL failed"
			", e.what(): {}",
			e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}

/*
string MMSDeliveryAuthorization::getAWSSignedURL(const string& playURL, int expirationInSeconds)
{
	string signedPlayURL;

	// string mmsGUIURL;
	// ostringstream response;
	// bool responseInitialized = false;
	try
	{
		// playURL is like:
		// https://d1nue3l1x0sz90.cloudfront.net/out/v1/ca8fd629f9204ca38daf18f04187c694/index.m3u8
		string prefix("https://");
		if (!(playURL.starts_with(prefix) && playURL.find('/', prefix.size()) != string::npos))
		{
			string errorMessage = std::format(
				"awsSignedURL. playURL wrong format"
				", playURL: {}",
				playURL
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		size_t uriStartIndex = playURL.find('/', prefix.size());
		string cloudFrontHostName = playURL.substr(prefix.size(), uriStartIndex - prefix.size());
		string uriPath = playURL.substr(uriStartIndex + 1);

		AWSSigner awsSigner;
		string signedPlayURL = awsSigner.calculateSignedURL(cloudFrontHostName, uriPath, _keyPairId, _privateKeyPEMPathName, expirationInSeconds);

		if (signedPlayURL.empty())
		{
			string errorMessage = std::format(
				"awsSignedURL. no signedPlayURL found"
				", signedPlayURL: {}",
				signedPlayURL
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception& e)
	{
		SPDLOG_ERROR(
			"awsSigner failed"
			", exception: {}",
			e.what()
		);

		throw;
	}

	return signedPlayURL;
}
*/

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
