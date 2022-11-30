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
// #include <openssl/md5.h>
#include <openssl/evp.h>


MMSDeliveryAuthorization::MMSDeliveryAuthorization(
	Json::Value configuration,
	shared_ptr<MMSStorage> mmsStorage,
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	shared_ptr<spdlog::logger> logger
)
{
	_configuration = configuration;
	_mmsStorage = mmsStorage;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_logger = logger;

	_keyPairId =  _configuration["aws"].get("keyPairId", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->keyPairId: " + _keyPairId
	);
	_privateKeyPEMPathName =  _configuration["aws"]
		.get("privateKeyPEMPathName", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->privateKeyPEMPathName: " + _privateKeyPEMPathName
	);
	_vodCloudFrontHostNamesRoot = _configuration["aws"]["vodCloudFrontHostNames"];
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->vodCloudFrontHostNames: " + "..."
	);

    Json::Value api = _configuration["api"];

    _deliveryProtocol  = api["delivery"].get("deliveryProtocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->deliveryProtocol: " + _deliveryProtocol
    );
    _deliveryHost_authorizationThroughParameter  = api["delivery"].get("deliveryHost_authorizationThroughParameter", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->deliveryHost_authorizationThroughParameter: " + _deliveryHost_authorizationThroughParameter
    );
    _deliveryHost_authorizationThroughPath  = api["delivery"].get("deliveryHost_authorizationThroughPath", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->deliveryHost_authorizationThroughPath: " + _deliveryHost_authorizationThroughPath 
    );

}
	
pair<string, string> MMSDeliveryAuthorization::createDeliveryAuthorization(
	int64_t userKey,
	shared_ptr<Workspace> requestWorkspace,
	string clientIPAddress,

	int64_t mediaItemKey,
	string uniqueName,
	int64_t encodingProfileKey,
	string encodingProfileLabel,

	int64_t physicalPathKey,

	int64_t ingestionJobKey,
	int64_t deliveryCode,

	int ttlInSeconds,
	int maxRetries,

	bool save,
	// deliveryType:
	// MMS_Token: delivery by MMS with a Token
	// MMS_SignedToken: delivery by MMS with a signed URL
	// AWSCloudFront: delivery by AWS CloudFront without a signed URL
	// AWSCloudFront_Signed: delivery by AWS CloudFront with a signed URL
	string deliveryType,

	bool warningIfMissingMediaItemKey,
	bool filteredByStatistic,
	string userId
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

			tie(title, mmsPartitionNumber, deliveryFileName, deliveryURI)
				= deliveryFileNameAndDeliveryURI;
		}
		else
		{
			if (uniqueName != "" && mediaItemKey == -1)
			{
				// initialize mediaItemKey

				pair<int64_t, MMSEngineDBFacade::ContentType> mediaItemKeyDetails =
					_mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
						requestWorkspace->_workspaceKey,
						uniqueName, warningIfMissingMediaItemKey);
				tie(mediaItemKey, ignore) = mediaItemKeyDetails;
			}

			if (encodingProfileKey == -1 && encodingProfileLabel != "")
			{
				// initialize encodingProfileKey

				MMSEngineDBFacade::ContentType contentType;

				tuple<MMSEngineDBFacade::ContentType, string, string, string,
					int64_t, int64_t> mediaItemDetails;
				mediaItemDetails = _mmsEngineDBFacade->getMediaItemKeyDetails(
					requestWorkspace->_workspaceKey, mediaItemKey,
					false	// warningIfMissing
				);
				tie(contentType, ignore, ignore, ignore, ignore, ignore) =
					mediaItemDetails;

				encodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
					requestWorkspace->_workspaceKey,
					contentType,
					encodingProfileLabel,
					true	// contentTypeToBeUsed
				);
			}

			tuple<string, int, int64_t, string, string> vodDeliveryURIDetails =
				_mmsStorage->getVODDeliveryURI(mediaItemKey,
					encodingProfileKey, save, requestWorkspace);
			tie(title, mmsPartitionNumber, localPhysicalPathKey, deliveryFileName, deliveryURI) =
				vodDeliveryURIDetails;
		}

		if (deliveryType == "AWSCloudFront_Signed")
		{
			time_t expirationTime = chrono::system_clock::to_time_t(
				chrono::system_clock::now());
			expirationTime += ttlInSeconds;

			// deliverURI: /MMS_0000/2/.....
			size_t beginURIIndex = deliveryURI.find("/", 1);
			if (beginURIIndex == string::npos)
			{
				string errorMessage = string("wrong deliveryURI")
					+ ", deliveryURI: " + deliveryURI
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string uriPath = deliveryURI.substr(beginURIIndex + 1);

			if (mmsPartitionNumber >= _vodCloudFrontHostNamesRoot.size())
			{
				string errorMessage = string("no CloudFrontHostName available")
					+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
					+ ", _vodCloudFrontHostNamesRoot.size: "
						+ to_string(_vodCloudFrontHostNamesRoot.size())
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string cloudFrontHostName
				= (_vodCloudFrontHostNamesRoot[mmsPartitionNumber]).asString();

			AWSSigner awsSigner(_logger);
			string signedPlayURL = awsSigner.calculateSignedURL(
				cloudFrontHostName,
				uriPath,
				_keyPairId,
				_privateKeyPEMPathName,
				expirationTime
			);

			deliveryURL = signedPlayURL;
		}
		else if (deliveryType == "AWSCloudFront")
		{
			// deliverURI: /MMS_0000/2/.....
			size_t beginURIIndex = deliveryURI.find("/", 1);
			if (beginURIIndex == string::npos)
			{
				string errorMessage = string("wrong deliveryURI")
					+ ", deliveryURI: " + deliveryURI
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string uriPath = deliveryURI.substr(beginURIIndex);

			if (mmsPartitionNumber >= _vodCloudFrontHostNamesRoot.size())
			{
				string errorMessage = string("no CloudFrontHostName available")
					+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
					+ ", _vodCloudFrontHostNamesRoot.size: "
						+ to_string(_vodCloudFrontHostNamesRoot.size())
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string cloudFrontHostName
				= (_vodCloudFrontHostNamesRoot[mmsPartitionNumber]).asString();

			deliveryURL = "https://" + cloudFrontHostName + uriPath;
		}
		else if (deliveryType == "MMS_Token")
		{
			int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
				userKey,
				clientIPAddress,
				localPhysicalPathKey,
				-1,
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

			if (save && deliveryFileName != "")
				deliveryURL.append("&deliveryFileName=").append(deliveryFileName);
		}
		else // if (deliveryType == "MMS_SignedToken")
		{
			time_t expirationTime = chrono::system_clock::to_time_t(
				chrono::system_clock::now());
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
			string md5Base64 = getSignedPath(uriToBeSigned, expirationTime);

			deliveryURL = 
				_deliveryProtocol
				+ "://" 
				+ _deliveryHost_authorizationThroughPath
				+ "/token_" + md5Base64 + "," + to_string(expirationTime)
				+ deliveryURI
			;
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
					requestWorkspace->_workspaceKey,
					userId,
					localPhysicalPathKey,
					-1,	// confStreamKey
					title
				);
			}
			catch(runtime_error e)
			{
				string errorMessage = string("mmsEngineDBFacade->addRequestStatistic failed")
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
		}

		_logger->info(__FILEREF__ + "createDeliveryAuthorization info"
			+ ", title: " + title
			+ ", deliveryURI: " + deliveryURI
			+ ", deliveryType: " + deliveryType
			+ ", deliveryURL (authorized): " + deliveryURL
		);
	}
	else
	{
		// create authorization for a live request

		tuple<string, MMSEngineDBFacade::IngestionType,
			MMSEngineDBFacade::IngestionStatus, string, string>
			ingestionJobDetails = _mmsEngineDBFacade->getIngestionJobDetails(
					requestWorkspace->_workspaceKey, ingestionJobKey);
		MMSEngineDBFacade::IngestionType ingestionType;
		string metaDataContent;
		tie(ignore, ingestionType, ignore, metaDataContent, ignore) = ingestionJobDetails;

		if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy
			&& ingestionType != MMSEngineDBFacade::IngestionType::VODProxy
			&& ingestionType != MMSEngineDBFacade::IngestionType::LiveGrid
			&& ingestionType != MMSEngineDBFacade::IngestionType::LiveRecorder
			&& ingestionType != MMSEngineDBFacade::IngestionType::Countdown
		)
		{
			string errorMessage = string("ingestionJob is not a LiveProxy")
				+ ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		Json::Value ingestionJobRoot = JSONUtils::toJson(-1, -1, metaDataContent);

		if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy
			|| ingestionType == MMSEngineDBFacade::IngestionType::VODProxy
			|| ingestionType == MMSEngineDBFacade::IngestionType::Countdown)
		{
			string field = "Outputs";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage =
					string("A Live-Proxy without Outputs cannot be delivered")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value outputsRoot = ingestionJobRoot[field];

			// Option 1: OutputType HLS with deliveryCode
			// Option 2: OutputType RTMP_Stream/AWS_CHANNEL with playURL
			// tuple<string, int64_t, string> means OutputType, deliveryCode, playURL
			vector<tuple<string, int64_t, string>> outputDeliveryOptions;
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType;
				int64_t localDeliveryCode = -1;
				string playURL;

				field = "OutputType";
				if (!JSONUtils::isMetadataPresent(outputRoot, field))
					outputType = "HLS";
				else
					outputType = outputRoot.get(field, "HLS").asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					field = "DeliveryCode";
					if (JSONUtils::isMetadataPresent(outputRoot, field))
						localDeliveryCode = outputRoot.get(field, 0).asInt64();
				}
				else if (outputType == "RTMP_Stream"
					|| outputType == "AWS_CHANNEL")
				{
					field = "PlayUrl";
					playURL = outputRoot.get(field, "").asString();
					if (playURL == "")
						continue;
				}

				outputDeliveryOptions.push_back(
					make_tuple(outputType, localDeliveryCode, playURL));
			}

			string outputType;
			string playURL;

			if (deliveryCode == -1)	// requested delivery code (it is an input)
			{
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
					string errorMessage =
						string("Live authorization with several option. Just get the first")
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
			else
			{
				bool deliveryCodeFound = false;

				for (tuple<string, int64_t, string> outputDeliveryOption:
					outputDeliveryOptions)
				{
					int64_t localDeliveryCode;
					tie(outputType, localDeliveryCode, playURL) = outputDeliveryOption;

					if (outputType == "HLS" && localDeliveryCode == deliveryCode)
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

			if (outputType == "RTMP_Stream"
				|| outputType == "AWS_CHANNEL")
			{
				deliveryURL = playURL;
			}
			else
			{
				string deliveryURI;
				string liveFileExtension;
				if (outputType == "HLS")
					liveFileExtension = "m3u8";
				else
					liveFileExtension = "mpd";

				tuple<string, string, string> liveDeliveryDetails
					= _mmsStorage->getLiveDeliveryDetails(
					to_string(deliveryCode),
					liveFileExtension, requestWorkspace);
				tie(deliveryURI, ignore, deliveryFileName) =
					liveDeliveryDetails;

				if (deliveryType == "MMS_Token")
				{
					int64_t authorizationKey =
						_mmsEngineDBFacade->createDeliveryAuthorization(
						userKey,
						clientIPAddress,
						-1,	// physicalPathKey,	vod key
						deliveryCode,		// live key
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
					time_t expirationTime = chrono::system_clock::to_time_t(
						chrono::system_clock::now());
					expirationTime += ttlInSeconds;

					string uriToBeSigned;
					{
						string m3u8Suffix(".m3u8");
						if (deliveryURI.size() >= m3u8Suffix.size()
							&& 0 == deliveryURI.compare(deliveryURI.size()
								- m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
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
					string md5Base64 = getSignedPath(uriToBeSigned, expirationTime);

					deliveryURL = 
						_deliveryProtocol
						+ "://" 
						+ _deliveryHost_authorizationThroughPath
						+ "/token_" + md5Base64 + "," + to_string(expirationTime)
						+ deliveryURI
					;
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
				field = "ConfigurationLabel";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage =
						field + " field missing"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				string configurationLabel = ingestionJobRoot.get(field, "").asString();

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
			catch(runtime_error e)
			{
				string errorMessage = string("mmsEngineDBFacade->addRequestStatistic failed")
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
		}
		else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
		{
			// outputType, localDeliveryCode, playURL
			vector<tuple<string, int64_t, string>> outputDeliveryOptions;

			string field = "Outputs";
			if (JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				Json::Value outputsRoot = ingestionJobRoot[field];

				// Option 1: OutputType HLS with deliveryCode
				// Option 2: OutputType RTMP_Stream/AWS_CHANNEL with playURL
				// tuple<string, int64_t, string> means OutputType, deliveryCode, playURL
				for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
				{
					Json::Value outputRoot = outputsRoot[outputIndex];

					string outputType;
					int64_t localDeliveryCode = -1;
					string playURL;

					field = "OutputType";
					if (!JSONUtils::isMetadataPresent(outputRoot, field))
						outputType = "HLS";
					else
						outputType = outputRoot.get(field, "HLS").asString();

					if (outputType == "HLS" || outputType == "DASH")
					{
						field = "DeliveryCode";
						if (JSONUtils::isMetadataPresent(outputRoot, field))
							localDeliveryCode = outputRoot.get(field, 0).asInt64();
					}
					else if (outputType == "RTMP_Stream"
						|| outputType == "AWS_CHANNEL")
					{
						field = "PlayUrl";
						playURL = outputRoot.get(field, "").asString();
						if (playURL == "")
							continue;
					}

					outputDeliveryOptions.push_back(make_tuple(outputType, localDeliveryCode, playURL));
				}
			}

			string outputType;
			string playURL;

			bool monitorHLS = false;
			bool liveRecorderVirtualVOD = false;
			
			field = "MonitorHLS";
			if (JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				monitorHLS = true;

			field = "LiveRecorderVirtualVOD";
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

					if (outputType == "HLS" && localDeliveryCode == deliveryCode)
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
				outputType = "HLS";
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
				outputType = "HLS";
			}

			if (outputType == "RTMP_Stream"
				|| outputType == "AWS_CHANNEL")
			{
				deliveryURL = playURL;
			}
			else
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
					string md5Base64 = getSignedPath(uriToBeSigned, expirationTime);

					deliveryURL = 
						_deliveryProtocol
						+ "://" 
						+ _deliveryHost_authorizationThroughPath
						+ "/token_" + md5Base64 + "," + to_string(expirationTime)
						+ deliveryURI
					;
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
				field = "ConfigurationLabel";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage =
						field + " field missing"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				string configurationLabel = ingestionJobRoot.get(field, "").asString();

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
			catch(runtime_error e)
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
		else // if (ingestionType != MMSEngineDBFacade::IngestionType::LiveGrid)
		{
			string field = "DeliveryCode";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage = string("A LiveGrid without DeliveryCode cannot be delivered")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			deliveryCode = JSONUtils::asInt64(ingestionJobRoot, field, 0);

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
				int64_t authorizationKey
					= _mmsEngineDBFacade->createDeliveryAuthorization(
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
				string md5Base64 = getSignedPath(uriToBeSigned, expirationTime);

				deliveryURL = 
					_deliveryProtocol
					+ "://" 
					+ _deliveryHost_authorizationThroughPath
					+ "/token_" + md5Base64 + "," + to_string(expirationTime)
					+ deliveryURI
				;
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

			_logger->info(__FILEREF__ + "createDeliveryAuthorization for LiveGrid"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", deliveryCode: " + to_string(deliveryCode)
				+ ", deliveryURL: " + deliveryURL
			);
		}
	}

	return make_pair(deliveryURL, deliveryFileName);
}

string MMSDeliveryAuthorization::getSignedPath(string contentURI, time_t expirationTime)
{
	string token = to_string(expirationTime) + contentURI;
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
			EVP_DigestUpdate(mdctx, (unsigned char*) token.c_str(), token.size());

			// MD5_Final
			md5_digest = (unsigned char *)OPENSSL_malloc(md5_digest_len);
			EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);

			md5Base64 = Convert::base64_encode(md5_digest, md5_digest_len);

			OPENSSL_free(md5_digest);

			EVP_MD_CTX_free(mdctx);
		}

		transform(md5Base64.begin(), md5Base64.end(), md5Base64.begin(),
			[](unsigned char c){
				if (c == '+')
					return '-';
				else if (c == '/')
					return '_';
				else
					return (char) c;
			}
		);
	}

	_logger->info(__FILEREF__ + "Authorization through path"
		+ ", contentURI: " + contentURI
		+ ", expirationTime: " + to_string(expirationTime)
		+ ", token: " + token
		+ ", md5Base64: " + md5Base64
	);


	return md5Base64;
}

