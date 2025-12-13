/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.cpp
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#include "Convert.h"
#include "CurlWrapper.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "LdapWrapper.h"
#include "StringUtils.h"
#include "System.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"
#include <format>
#include <fstream>
#include <openssl/evp.h>
#include <regex>
#include <sstream>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <unordered_map>
#include <utility>

#include "API.h"

API::API(
	const bool noFileSystemAccess, const json& configurationRoot, const shared_ptr<MMSEngineDBFacade>& mmsEngineDBFacade, const shared_ptr<MMSStorage>& mmsStorage,
	const shared_ptr<MMSDeliveryAuthorization>& mmsDeliveryAuthorization, mutex *fcgiAcceptMutex, FileUploadProgressData *fileUploadProgressData,
	const shared_ptr<atomic<uint64_t>>& avgBandwidthUsage
)
	: FastCGIAPI(configurationRoot, fcgiAcceptMutex), _mmsEngineDBFacade(mmsEngineDBFacade), _noFileSystemAccess(noFileSystemAccess),
	  _mmsStorage(mmsStorage), _mmsDeliveryAuthorization(mmsDeliveryAuthorization), _bandwidthStats()
{
	_configurationRoot = configurationRoot;
	_avgBandwidthUsage = avgBandwidthUsage;

	loadConfiguration(configurationRoot, fileUploadProgressData);

	registerHandler<API>("status", &API::status);
	registerHandler<API>("avgBandwidthUsage", &API::avgBandwidthUsage);
	registerHandler<API>("binaryAuthorization", &API::binaryAuthorization);
	registerHandler<API>("deliveryAuthorizationThroughParameter", &API::deliveryAuthorizationThroughParameter);
	registerHandler<API>("deliveryAuthorizationThroughPath", &API::deliveryAuthorizationThroughPath);
	registerHandler<API>("manageHTTPStreamingManifest_authorizationThroughParameter", &API::manageHTTPStreamingManifest_authorizationThroughParameter);

	registerHandler<API>("login", &API::login);
	registerHandler<API>("registerUser", &API::registerUser);
	registerHandler<API>("updateUser", &API::updateUser);
	registerHandler<API>("createTokenToResetPassword", &API::createTokenToResetPassword);
	registerHandler<API>("resetPassword", &API::resetPassword);
	registerHandler<API>("updateWorkspace", &API::updateWorkspace);
	registerHandler<API>("setWorkspaceAsDefault", &API::setWorkspaceAsDefault);
	registerHandler<API>("createWorkspace", &API::createWorkspace);
	registerHandler<API>("deleteWorkspace", &API::deleteWorkspace);
	registerHandler<API>("unshareWorkspace", &API::unshareWorkspace);
	registerHandler<API>("workspaceUsage", &API::workspaceUsage);
	registerHandler<API>("shareWorkspace", &API::shareWorkspace_);
	registerHandler<API>("workspaceList", &API::workspaceList);
	registerHandler<API>("addInvoice", &API::addInvoice);
	registerHandler<API>("invoiceList", &API::invoiceList);
	registerHandler<API>("confirmRegistration", &API::confirmRegistration);
	registerHandler<API>("addEncoder", &API::addEncoder);
	registerHandler<API>("removeEncoder", &API::removeEncoder);
	registerHandler<API>("modifyEncoder", &API::modifyEncoder);
	registerHandler<API>("encoderList", &API::encoderList);
	registerHandler<API>("encodersPoolList", &API::encodersPoolList);
	registerHandler<API>("addEncodersPool", &API::addEncodersPool);
	registerHandler<API>("modifyEncodersPool", &API::modifyEncodersPool);
	registerHandler<API>("removeEncodersPool", &API::removeEncodersPool);
	registerHandler<API>("addAssociationWorkspaceEncoder", &API::addAssociationWorkspaceEncoder);
	registerHandler<API>("removeAssociationWorkspaceEncoder", &API::removeAssociationWorkspaceEncoder);
	registerHandler<API>("createDeliveryAuthorization", &API::createDeliveryAuthorization);
	registerHandler<API>("createBulkOfDeliveryAuthorization", &API::createBulkOfDeliveryAuthorization);
	registerHandler<API>("ingestion", &API::ingestion);
	registerHandler<API>("ingestionRootsStatus", &API::ingestionRootsStatus);
	registerHandler<API>("ingestionRootMetaDataContent", &API::ingestionRootMetaDataContent);
	registerHandler<API>("ingestionJobsStatus", &API::ingestionJobsStatus);
	registerHandler<API>("cancelIngestionJob", &API::cancelIngestionJob);
	registerHandler<API>("updateIngestionJob", &API::updateIngestionJob);
	registerHandler<API>("ingestionJobSwitchToEncoder", &API::ingestionJobSwitchToEncoder);
	registerHandler<API>("encodingJobsStatus", &API::encodingJobsStatus);
	registerHandler<API>("encodingJobPriority", &API::encodingJobPriority);
	registerHandler<API>("killOrCancelEncodingJob", &API::killOrCancelEncodingJob);
	registerHandler<API>("changeLiveProxyPlaylist", &API::changeLiveProxyPlaylist);
	registerHandler<API>("changeLiveProxyOverlayText", &API::changeLiveProxyOverlayText);
	registerHandler<API>("mediaItemsList", &API::mediaItemsList);
	registerHandler<API>("updateMediaItem", &API::updateMediaItem);
	registerHandler<API>("updatePhysicalPath", &API::updatePhysicalPath);
	registerHandler<API>("tagsList", &API::tagsList);
	registerHandler<API>("uploadedBinary", &API::uploadedBinary);
	registerHandler<API>("addUpdateEncodingProfilesSet", &API::addUpdateEncodingProfilesSet);
	registerHandler<API>("encodingProfilesSetsList", &API::encodingProfilesSetsList);
	registerHandler<API>("addEncodingProfile", &API::addEncodingProfile);
	registerHandler<API>("removeEncodingProfile", &API::removeEncodingProfile);
	registerHandler<API>("removeEncodingProfilesSet", &API::removeEncodingProfilesSet);
	registerHandler<API>("encodingProfilesList", &API::encodingProfilesList);
	registerHandler<API>("workflowsAsLibraryList", &API::workflowsAsLibraryList);
	registerHandler<API>("workflowAsLibraryContent", &API::workflowAsLibraryContent);
	registerHandler<API>("saveWorkflowAsLibrary", &API::saveWorkflowAsLibrary);
	registerHandler<API>("removeWorkflowAsLibrary", &API::removeWorkflowAsLibrary);
	registerHandler<API>("mmsSupport", &API::mmsSupport);
	registerHandler<API>("addYouTubeConf", &API::addYouTubeConf);
	registerHandler<API>("modifyYouTubeConf", &API::modifyYouTubeConf);
	registerHandler<API>("removeYouTubeConf", &API::removeYouTubeConf);
	registerHandler<API>("youTubeConfList", &API::youTubeConfList);
	registerHandler<API>("addFacebookConf", &API::addFacebookConf);
	registerHandler<API>("modifyFacebookConf", &API::modifyFacebookConf);
	registerHandler<API>("removeFacebookConf", &API::removeFacebookConf);
	registerHandler<API>("facebookConfList", &API::facebookConfList);
	registerHandler<API>("addTwitchConf", &API::addTwitchConf);
	registerHandler<API>("modifyTwitchConf", &API::modifyTwitchConf);
	registerHandler<API>("removeTwitchConf", &API::removeTwitchConf);
	registerHandler<API>("twitchConfList", &API::twitchConfList);
	registerHandler<API>("addStream", &API::addStream);
	registerHandler<API>("modifyStream", &API::modifyStream);
	registerHandler<API>("removeStream", &API::removeStream);
	registerHandler<API>("streamList", &API::streamList);
	registerHandler<API>("streamFreePushEncoderPort", &API::streamFreePushEncoderPort);
	registerHandler<API>("addSourceTVStream", &API::addSourceTVStream);
	registerHandler<API>("modifySourceTVStream", &API::modifySourceTVStream);
	registerHandler<API>("removeSourceTVStream", &API::removeSourceTVStream);
	registerHandler<API>("sourceTVStreamList", &API::sourceTVStreamList);
	registerHandler<API>("addAWSChannelConf", &API::addAWSChannelConf);
	registerHandler<API>("modifyAWSChannelConf", &API::modifyAWSChannelConf);
	registerHandler<API>("removeAWSChannelConf", &API::removeAWSChannelConf);
	registerHandler<API>("awsChannelConfList", &API::awsChannelConfList);
	registerHandler<API>("addCDN77ChannelConf", &API::addCDN77ChannelConf);
	registerHandler<API>("modifyCDN77ChannelConf", &API::modifyCDN77ChannelConf);
	registerHandler<API>("removeCDN77ChannelConf", &API::removeCDN77ChannelConf);
	registerHandler<API>("cdn77ChannelConfList", &API::cdn77ChannelConfList);
	registerHandler<API>("addRTMPChannelConf", &API::addRTMPChannelConf);
	registerHandler<API>("modifyRTMPChannelConf", &API::modifyRTMPChannelConf);
	registerHandler<API>("removeRTMPChannelConf", &API::removeRTMPChannelConf);
	registerHandler<API>("rtmpChannelConfList", &API::rtmpChannelConfList);
	registerHandler<API>("addSRTChannelConf", &API::addSRTChannelConf);
	registerHandler<API>("modifySRTChannelConf", &API::modifySRTChannelConf);
	registerHandler<API>("removeSRTChannelConf", &API::removeSRTChannelConf);
	registerHandler<API>("srtChannelConfList", &API::srtChannelConfList);
	registerHandler<API>("addHLSChannelConf", &API::addHLSChannelConf);
	registerHandler<API>("modifyHLSChannelConf", &API::modifyHLSChannelConf);
	registerHandler<API>("removeHLSChannelConf", &API::removeHLSChannelConf);
	registerHandler<API>("hlsChannelConfList", &API::hlsChannelConfList);
	registerHandler<API>("addFTPConf", &API::addFTPConf);
	registerHandler<API>("modifyFTPConf", &API::modifyFTPConf);
	registerHandler<API>("removeFTPConf", &API::removeFTPConf);
	registerHandler<API>("ftpConfList", &API::ftpConfList);
	registerHandler<API>("addEMailConf", &API::addEMailConf);
	registerHandler<API>("modifyEMailConf", &API::modifyEMailConf);
	registerHandler<API>("removeEMailConf", &API::removeEMailConf);
	registerHandler<API>("emailConfList", &API::emailConfList);
	registerHandler<API>("loginStatisticList", &API::loginStatisticList);
	registerHandler<API>("addRequestStatistic", &API::addRequestStatistic);
	registerHandler<API>("requestStatisticList", &API::requestStatisticList);
	registerHandler<API>("requestStatisticPerContentList", &API::requestStatisticPerContentList);
	registerHandler<API>("requestStatisticPerUserList", &API::requestStatisticPerUserList);
	registerHandler<API>("requestStatisticPerMonthList", &API::requestStatisticPerMonthList);
	registerHandler<API>("requestStatisticPerDayList", &API::requestStatisticPerDayList);
	registerHandler<API>("requestStatisticPerHourList", &API::requestStatisticPerHourList);
	registerHandler<API>("requestStatisticPerCountryList", &API::requestStatisticPerCountryList);
}

API::~API() = default;

void API::manageRequestAndResponse(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI, const string_view& requestMethod,
	const string_view& requestBody, bool responseBodyCompressed, unsigned long contentLength,
	const unordered_map<string, string> &requestDetails, const unordered_map<string, string>& queryParameters
)
{
	bool basicAuthenticationPresent = authorizationDetails != nullptr;
	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	if (basicAuthenticationPresent)
	{
		SPDLOG_INFO(
			"Received manageRequestAndResponse"
			", requestURI: {}"
			", requestMethod: {}"
			", contentLength: {}"
			", userKey: {}"
			", workspace->_name: {}"
			", requestBody: {}"
			", admin: {}"
			", createRemoveWorkspace: {}"
			", ingestWorkflow: {}"
			", createProfiles: {}"
			", deliveryAuthorization: {}"
			", shareWorkspace: {}"
			", editMedia: {}"
			", editConfiguration: {}"
			", killEncoding: {}"
			", cancelIngestionJob: {}"
			", editEncodersPool: {}"
			", applicationRecorder: {}"
			", createRemoveLiveChannel: {}",
			requestURI, requestMethod, contentLength, apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_name,
			requestBody, apiAuthorizationDetails->admin, apiAuthorizationDetails->canCreateRemoveWorkspace,
			apiAuthorizationDetails->canIngestWorkflow, apiAuthorizationDetails->canCreateProfiles,
			apiAuthorizationDetails->canDeliveryAuthorization, apiAuthorizationDetails->canShareWorkspace,
			apiAuthorizationDetails->canEditMedia, apiAuthorizationDetails->canEditConfiguration,
			apiAuthorizationDetails->canKillEncoding, apiAuthorizationDetails->canCancelIngestionJob,
			apiAuthorizationDetails->canEditEncodersPool, apiAuthorizationDetails->canApplicationRecorder,
			apiAuthorizationDetails->canCreateRemoveLiveChannel
		);
	}

	if (!basicAuthenticationPresent)
	{
		SPDLOG_INFO(
			"Received manageRequestAndResponse"
			", requestURI: {}"
			", requestMethod: {}"
			", contentLength: {}",
			requestURI, requestMethod, contentLength
		);
	}

	try
	{
		handleRequest(sThreadId, requestIdentifier, request, authorizationDetails, requestURI, requestMethod, requestBody,
			responseBodyCompressed, requestDetails, queryParameters, true);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"manage request failed"
			", requestBody: {}"
			", e.what(): {}",
			requestBody, e.what()
		);

		int htmlResponseCode = 500;
		string errorMessage;
		if (dynamic_cast<HTTPError*>(&e))
		{
			htmlResponseCode = dynamic_cast<HTTPError*>(&e)->httpErrorCode;
			errorMessage = e.what();
		}
		else
			errorMessage = getHtmlStandardMessage(htmlResponseCode);

		SPDLOG_ERROR(errorMessage);

		sendError(request, htmlResponseCode, errorMessage);

		throw;
	}
}

void API::mmsSupport(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "mmsSupport";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		string userEmailAddress;
		string subject;
		string text;

		json metadataRoot = JSONUtils::toJson(requestBody);

		vector<string> mandatoryFields = {"UserEmailAddress", "Subject", "Text"};
		for (string field : mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = std::format(
					"Json field is not present or it is null"
					", Json field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw HTTPError(400);
			}
		}

		userEmailAddress = JSONUtils::asString(metadataRoot, "UserEmailAddress", "");
		subject = JSONUtils::asString(metadataRoot, "Subject", "");
		text = JSONUtils::asString(metadataRoot, "Text", "");

		{
			shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

			vector<string> emailBody;
			emailBody.push_back(std::format("<p>UserKey: {}</p>", apiAuthorizationDetails->userKey));
			emailBody.push_back(std::format("<p>WorkspaceKey: {}</p>", apiAuthorizationDetails->workspace->_workspaceKey));
			emailBody.push_back(std::format("<p>APIKey: {}</p>", apiAuthorizationDetails->password));
			emailBody.push_back("<p></p>");
			emailBody.push_back(std::format("<p>From: {}</p>", userEmailAddress));
			emailBody.push_back("<p></p>");
			emailBody.push_back(std::format("<p>{}</p>", text));

			string tosCommaSeparated = "support@catramms-cloud.com";
			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw;
	}
}

void API::status(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "status";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		json statusRoot;

		statusRoot["status"] = "API server up and running";
		// statusRoot["version-api"] = version;

		string sJson = JSONUtils::toString(statusRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, sJson);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw;
	}
}

void API::avgBandwidthUsage(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "avgBandwidthUsage";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		json statusRoot;

		statusRoot["avgBandwidthUsage"] = _avgBandwidthUsage->load(memory_order_relaxed);

		sendSuccess(
			sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, JSONUtils::toString(statusRoot)
		);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw;
	}
}

void API::manageHTTPStreamingManifest_authorizationThroughParameter(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "manageHTTPStreamingManifest_authorizationThroughParameter";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		if (_noFileSystemAccess)
		{
			string errorMessage = std::format(
				"no rights to execute this method"
				", _noFileSystemAccess: {}",
				_noFileSystemAccess
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		auto tokenIt = queryParameters.find("token");
		if (tokenIt == queryParameters.end())
		{
			string errorMessage = string("Not authorized: token parameter not present");
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		// we could have:
		//		- master manifest, token parameter: <token>--- (es: token=9163 oppure ic_vOSatb6TWp4ania5kaQ%3D%3D,1717958161)
		//			es: /MMS_0000/1/001/472/152/8063642_2/8063642_1653439.m3u8?token=9163
		//			es: /MMS_0000/1/001/470/566/8055007_2/8055007_1652158.m3u8?token=ic_vOSatb6TWp4ania5kaQ%3D%3D,1717958161
		//		- secondary manifest (that has to be treated as a .ts delivery), token parameter:
		//			<encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
		//			es:
		/// MMS_0000/1/001/472/152/8063642_2/360p/8063642_1653439.m3u8?token=Nw2npoRhfMLZC-GiRuZHpI~jGKBRA-NE-OARj~o68En4XFUriOSuXqexke21OTVd
		bool secondaryManifest;
		string tokenComingFromURL;

		bool isNumber = StringUtils::isNumber(tokenIt->second);
		if (isNumber || tokenIt->second.find(",") != string::npos)
		{
			secondaryManifest = false;
			// tokenComingFromURL = stoll(tokenIt->second);
			tokenComingFromURL = tokenIt->second;
		}
		else
		{
			secondaryManifest = true;
			// tokenComingFromURL will be initialized in the next statement
		}
		SPDLOG_INFO(
			"manageHTTPStreamingManifest"
			", analizing the token {}"
			", isNumber: {}"
			", tokenIt->second: {}"
			", secondaryManifest: {}",
			tokenIt->second, isNumber, tokenIt->second, secondaryManifest
		);

		string contentURI;
		{
			size_t endOfURIIndex = requestURI.find_last_of('?');
			if (endOfURIIndex == string::npos)
			{
				string errorMessage = std::format(
					"Wrong URI format"
					", requestURI: {}",
					requestURI
				);
				SPDLOG_INFO(errorMessage);

				throw runtime_error(errorMessage);
			}
			contentURI = requestURI.substr(0, endOfURIIndex);
		}

		if (secondaryManifest)
		{
			auto cookieIt = queryParameters.find("cookie");
			if (cookieIt == queryParameters.end())
			{
				string errorMessage = string("The 'cookie' parameter is not found");
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string cookie = cookieIt->second;

			string token = tokenIt->second;

			tokenComingFromURL = _mmsDeliveryAuthorization->checkDeliveryAuthorizationOfAManifest(secondaryManifest, token, cookie, contentURI);

			/*
			string tokenParameter = std::format("{}---{}", tokenIt->second, cookie);
			SPDLOG_INFO(
				"Calling checkDeliveryAuthorizationThroughParameter"
				", contentURI: {}"
				", tokenParameter: {}",
				contentURI, tokenParameter
			);
			tokenComingFromURL = _mmsDeliveryAuthorization->checkDeliveryAuthorizationThroughParameter(contentURI, tokenParameter);
			*/
		}
		else
		{
			// cookie parameter is added inside catramms.nginx
			string mmsInfoCookie;
			auto cookieIt = queryParameters.find("cookie");
			if (cookieIt != queryParameters.end())
				mmsInfoCookie = cookieIt->second;

			tokenComingFromURL = _mmsDeliveryAuthorization->checkDeliveryAuthorizationOfAManifest(
				secondaryManifest, tokenComingFromURL, mmsInfoCookie, contentURI
			);
		}

		// manifest authorized

		{
			string contentType;

			string m3u8Extension(".m3u8");
			if (contentURI.ends_with(m3u8Extension))
				contentType = "Content-type: application/x-mpegURL";
			else // dash
				contentType = "Content-type: application/dash+xml";
			string cookieName = "mmsInfo";

			string responseBody;
			{
				fs::path manifestPathFileName = _mmsStorage->getMMSRootRepository() / contentURI.substr(1);

				SPDLOG_INFO(
					"Reading manifest file"
					", manifestPathFileName: {}",
					manifestPathFileName.string()
				);

				if (!fs::exists(manifestPathFileName))
				{
					string errorMessage = std::format(
						"manifest file not existing"
						", manifestPathFileName: {}",
						manifestPathFileName.string()
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				if (contentURI.ends_with(m3u8Extension))
				{
					std::ifstream manifestFile;

					manifestFile.open(manifestPathFileName.string(), ios::in);
					if (!manifestFile.is_open())
					{
						string errorMessage = std::format(
							"Not authorized: manifest file not opened"
							", manifestPathFileName: {}",
							manifestPathFileName.string()
						);
						SPDLOG_INFO(errorMessage);

						throw runtime_error(errorMessage);
					}

					string manifestLine;
					string tsExtension = ".ts";
					string m3u8Extension = ".m3u8";
					string m3u8ExtXMedia = "#EXT-X-MEDIA";
					string endLine = "\n";
					while (getline(manifestFile, manifestLine))
					{
						if (manifestLine[0] != '#' && manifestLine.ends_with(tsExtension))
						{
							/*
							SPDLOG_INFO(__FILEREF__ + "Creation token parameter for ts"
								+ ", manifestLine: " + manifestLine
								+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
							);
							*/
							string auth = Encrypt::opensslEncrypt(manifestLine + "+++" + tokenComingFromURL);
							responseBody += (manifestLine + "?token=" + auth + endLine);
						}
						else if (manifestLine[0] != '#' && manifestLine.ends_with(m3u8Extension))
						{
							// scenario where we have several .m3u8 manifest files
							/*
							SPDLOG_INFO(__FILEREF__ + "Creation token parameter for m3u8"
								+ ", manifestLine: " + manifestLine
								+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
							);
							*/
							string auth = Encrypt::opensslEncrypt(std::format("{}+++{}", manifestLine, tokenComingFromURL));
							responseBody += std::format("{}?token={}{}", manifestLine, auth, endLine);
						}
						else if (manifestLine.starts_with(m3u8ExtXMedia))
						{
							// #EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES,
							// DEFAULT=YES,URI="eng/1247999_384641.m3u8"
							string temp = "URI=\"";
							size_t uriStartIndex = manifestLine.find(temp);
							if (uriStartIndex != string::npos)
							{
								uriStartIndex += temp.size();
								size_t uriEndIndex = uriStartIndex;
								while (manifestLine[uriEndIndex] != '\"' && uriEndIndex < manifestLine.size())
									uriEndIndex++;
								if (manifestLine[uriEndIndex] == '\"')
								{
									string uri = manifestLine.substr(uriStartIndex, uriEndIndex - uriStartIndex);
									/*
									SPDLOG_INFO(__FILEREF__ + "Creation token parameter for m3u8"
										+ ", uri: " + uri
										+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
									);
									*/
									string auth = Encrypt::opensslEncrypt(uri + "+++" + tokenComingFromURL);
									string tokenParameter = string("?token=") + auth;

									manifestLine.insert(uriEndIndex, tokenParameter);
								}
							}

							responseBody += (manifestLine + endLine);
						}
						else
						{
							responseBody += (manifestLine + endLine);
						}
					}
					manifestFile.close();
				}
				else // dash
				{
#if defined(LIBXML_TREE_ENABLED) && defined(LIBXML_OUTPUT_ENABLED) && defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED)
					SPDLOG_INFO("libxml define OK");
#else
					SPDLOG_INFO("libxml define KO");
#endif

					/*
					<?xml version="1.0" encoding="utf-8"?>
					<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xmlns="urn:mpeg:dash:schema:mpd:2011"
							xmlns:xlink="http://www.w3.org/1999/xlink"
							xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011
					http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd"
							profiles="urn:mpeg:dash:profile:isoff-live:2011"
							type="dynamic"
							minimumUpdatePeriod="PT10S"
							suggestedPresentationDelay="PT10S"
							availabilityStartTime="2020-02-03T15:11:56Z"
							publishTime="2020-02-04T08:54:57Z"
							timeShiftBufferDepth="PT1M0.0S"
							minBufferTime="PT20.0S">
							<ProgramInformation>
							</ProgramInformation>
							<Period id="0" start="PT0.0S">
									<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true">
											<Representation id="0" mimeType="video/mp4" codecs="avc1.640029" bandwidth="1494920" width="1024"
					height="576" frameRate="25/1"> <SegmentTemplate timescale="12800" initialization="init-stream$RepresentationID$.m4s"
					media="chunk-stream$RepresentationID$-$Number%05d$.m4s" startNumber="6373"> <SegmentTimeline> <S t="815616000" d="128000"
					r="5" />
															</SegmentTimeline>
													</SegmentTemplate>
											</Representation>
									</AdaptationSet>
									<AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true">
											<Representation id="1" mimeType="audio/mp4" codecs="mp4a.40.5" bandwidth="95545"
					audioSamplingRate="48000"> <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011"
					value="2" /> <SegmentTemplate timescale="48000" initialization="init-stream$RepresentationID$.m4s"
					media="chunk-stream$RepresentationID$-$Number%05d$.m4s" startNumber="6373"> <SegmentTimeline> <S t="3058557246" d="479232" />
																	<S d="481280" />
																	<S d="479232" r="1" />
																	<S d="481280" />
																	<S d="479232" />
															</SegmentTimeline>
													</SegmentTemplate>
											</Representation>
									</AdaptationSet>
							</Period>
					</MPD>
					*/
					xmlDocPtr doc = xmlParseFile(manifestPathFileName.string().c_str());
					if (doc == nullptr)
					{
						string errorMessage = std::format(
							"xmlParseFile failed"
							", manifestPathFileName: {}",
							manifestPathFileName.string()
						);
						SPDLOG_INFO(errorMessage);

						throw runtime_error(errorMessage);
					}

					// xmlNode* rootElement = xmlDocGetRootElement(doc);

					/* Create xpath evaluation context */
					xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
					if (xpathCtx == nullptr)
					{
						xmlFreeDoc(doc);

						string errorMessage = std::format(
							"xmlXPathNewContext failed"
							", manifestPathFileName: {}",
							manifestPathFileName.string()
						);
						SPDLOG_INFO(errorMessage);

						throw runtime_error(errorMessage);
					}

					if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "xmlns", BAD_CAST "urn:mpeg:dash:schema:mpd:2011") != 0)
					{
						xmlXPathFreeContext(xpathCtx);
						xmlFreeDoc(doc);

						string errorMessage = std::format(
							"xmlXPathRegisterNs xmlns:xsi"
							", manifestPathFileName: {}",
							manifestPathFileName.string()
						);
						SPDLOG_INFO(errorMessage);

						throw runtime_error(errorMessage);
					}
					/*
					if(xmlXPathRegisterNs(xpathCtx,
						BAD_CAST "xmlns:xlink",
						BAD_CAST "http://www.w3.org/1999/xlink") != 0)
					{
						xmlXPathFreeContext(xpathCtx);
						xmlFreeDoc(doc);

						string errorMessage = string("xmlXPathRegisterNs xmlns:xlink")
							+ ", manifestPathFileName: " + manifestPathFileName.string()
							;
						SPDLOG_INFO(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
					if(xmlXPathRegisterNs(xpathCtx,
						BAD_CAST "xsi:schemaLocation",
						BAD_CAST "http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd") != 0)
					{
						xmlXPathFreeContext(xpathCtx);
						xmlFreeDoc(doc);

						string errorMessage = string("xmlXPathRegisterNs xsi:schemaLocation")
							+ ", manifestPathFileName: " + manifestPathFileName.string()
							;
						SPDLOG_INFO(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
					*/

					// Evaluate xpath expression
					const char *xpathExpr = "//xmlns:Period/xmlns:AdaptationSet/xmlns:Representation/xmlns:SegmentTemplate";
					xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST xpathExpr, xpathCtx);
					if (xpathObj == nullptr)
					{
						xmlXPathFreeContext(xpathCtx);
						xmlFreeDoc(doc);

						string errorMessage = std::format(
							"xmlXPathEvalExpression failed"
							", manifestPathFileName: {}",
							manifestPathFileName.string()
						);
						SPDLOG_INFO(errorMessage);

						throw runtime_error(errorMessage);
					}

					xmlNodeSetPtr nodes = xpathObj->nodesetval;
					SPDLOG_INFO(
						"processing mpd manifest file"
						", manifestPathFileName: {}"
						", nodesNumber: {}",
						manifestPathFileName.string(), nodes->nodeNr
					);
					for (int nodeIndex = 0; nodeIndex < nodes->nodeNr; nodeIndex++)
					{
						if (nodes->nodeTab[nodeIndex] == nullptr)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = std::format(
								"nodes->nodeTab[nodeIndex] is null"
								", manifestPathFileName: {}"
								", nodeIndex: {}",
								manifestPathFileName.string(), nodeIndex
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}

						const char *mediaAttributeName = "media";
						const char *initializationAttributeName = "initialization";
						xmlChar *mediaValue = xmlGetProp(nodes->nodeTab[nodeIndex], BAD_CAST mediaAttributeName);
						xmlChar *initializationValue = xmlGetProp(nodes->nodeTab[nodeIndex], BAD_CAST initializationAttributeName);
						if (mediaValue == (xmlChar *)nullptr || initializationValue == (xmlChar *)nullptr)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = std::format(
								"xmlGetProp failed"
								", manifestPathFileName: {}",
								manifestPathFileName.string()
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}

						string auth = Encrypt::opensslEncrypt(string((char *)mediaValue) + "+++" + tokenComingFromURL);
						string newMediaAttributeValue = string((char *)mediaValue) + "?token=" + auth;
						// xmlAttrPtr
						xmlSetProp(nodes->nodeTab[nodeIndex], BAD_CAST mediaAttributeName, BAD_CAST newMediaAttributeValue.c_str());

						string newInitializationAttributeValue = string((char *)initializationValue) + "?token=" + auth;
						// xmlAttrPtr
						xmlSetProp(
							nodes->nodeTab[nodeIndex], BAD_CAST initializationAttributeName, BAD_CAST newInitializationAttributeValue.c_str()
						);

						// const char *value = "ssss";
						// xmlNodeSetContent(nodes->nodeTab[nodeIndex], BAD_CAST value);

						/*
						 * All the elements returned by an XPath query are pointers to
						 * elements from the tree *except* namespace nodes where the XPath
						 * semantic is different from the implementation in libxml2 tree.
						 * As a result when a returned node set is freed when
						 * xmlXPathFreeObject() is called, that routine must check the
						 * element type. But node from the returned set may have been removed
						 * by xmlNodeSetContent() resulting in access to freed data.
						 * This can be exercised by running
						 *       valgrind xpath2 test3.xml '//discarded' discarded
						 * There is 2 ways around it:
						 *   - make a copy of the pointers to the nodes from the result set
						 *     then call xmlXPathFreeObject() and then modify the nodes
						 * or
						 *   - remove the reference to the modified nodes from the node set
						 *     as they are processed, if they are not namespace nodes.
						 */
						// if (nodes->nodeTab[nodeIndex]->type != XML_NAMESPACE_DECL)
						// 	nodes->nodeTab[nodeIndex] = NULL;
					}

					/* Cleanup of XPath data */
					xmlXPathFreeObject(xpathObj);
					xmlXPathFreeContext(xpathCtx);

					/* dump the resulting document */
					{
						xmlChar *xmlbuff;
						int buffersize;
						xmlDocDumpFormatMemoryEnc(doc, &xmlbuff, &buffersize, "UTF-8", 1);
						SPDLOG_INFO(
							"dumping mpd manifest file"
							", manifestPathFileName: {}"
							", buffersize: {}",
							manifestPathFileName.string(), buffersize
						);

						responseBody = (char *)xmlbuff;

						xmlFree(xmlbuff);
						// xmlDocDump(stdout, doc);
					}

					/* free the document */
					xmlFreeDoc(doc);

					/*
					std::ifstream manifestFile(manifestPathFileName);
					std::stringstream buffer;
					buffer << manifestFile.rdbuf();

					responseBody = buffer.str();
					*/
				}
			}

			string cookieValue = Encrypt::opensslEncrypt(tokenComingFromURL);
			string cookiePath;
			{
				size_t cookiePathIndex = contentURI.find_last_of("/");
				if (cookiePathIndex == string::npos)
				{
					string errorMessage = std::format(
						"Wrong URI format"
						", contentURI: {}",
						contentURI
					);
					SPDLOG_INFO(errorMessage);

					throw runtime_error(errorMessage);
				}
				cookiePath = contentURI.substr(0, cookiePathIndex);
			}

			bool enableCorsGETHeader = true;
			string originHeader;
			{
				auto originIt = requestDetails.find("HTTP_ORIGIN");
				if (originIt != requestDetails.end())
					originHeader = originIt->second;
			}
			if (secondaryManifest)
				sendSuccess(
					sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody, contentType, "",
					"", "", enableCorsGETHeader, originHeader
				);
			else
				sendSuccess(
					sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody, contentType,
					cookieName, cookieValue, cookiePath, enableCorsGETHeader, originHeader
				);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw HTTPError(403);
	}
}

void API::sendError(FCGX_Request &request, int htmlResponseCode, const string_view& errorMessage)
{
	json responseBodyRoot;
	responseBodyRoot["status"] = to_string(htmlResponseCode);
	responseBodyRoot["error"] = errorMessage;

	FastCGIAPI::sendError(request, htmlResponseCode, JSONUtils::toString(responseBodyRoot));
}

void API::stopBandwidthUsageThread()
{
	_bandwidthUsageThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_bandwidthUsagePeriodInSeconds));
}

void API::bandwidthUsageThread()
{
	while (!_bandwidthUsageThreadShutdown)
	{
		// non serve lo sleep perchè lo sleep è già all'interno di System::getBandwidthInBytes
		// this_thread::sleep_for(chrono::seconds(_bandwidthUsagePeriodInSeconds));

		// aggiorniamo la banda usata da questo server. Ci server per rispondere alla API .../bandwidthUsage
		double avgBandwidthUsage = 0;
		try
		{
			// impieghera' 15 secs
			// Ritorna la banda media secondo i parametri specificati ed anche i picchi
			map<string, pair<uint64_t, uint64_t>> peakInBytes;
			map<string, pair<uint64_t, uint64_t>> avgBandwidth = System::getAvgAndPeakBandwidthInBytes(peakInBytes, 2, 5);

			bool deliveryExternalNetworkInterfaceFound = false;
			for (const auto &[iface, stats] : avgBandwidth)
			{
				auto [rx, tx] = stats;
				SPDLOG_INFO(
					"bandwidthUsageThread, avgBandwidthInMbps"
					", iface: {}"
					", rx: {} ({}Mbps)"
					", tx: {} ({}Mbps)",
					iface, rx, static_cast<uint32_t>((rx * 8) / 1000000), tx, static_cast<uint32_t>((tx * 8) / 1000000)
				);
				if (_deliveryExternalNetworkInterface == iface)
				{
					avgBandwidthUsage = tx;
					deliveryExternalNetworkInterfaceFound = true;
					// break; commentato in modo da avere sempre il log della banda usata da tutte le reti (public e internal)
				}
			}
			if (!deliveryExternalNetworkInterfaceFound)
				SPDLOG_WARN(
					"bandwidthUsageThread, getAvgAndPeakBandwidthInBytes"
					", deliveryExternalNetworkInterface not found"
					", _deliveryExternalNetworkInterface: {}",
					_deliveryExternalNetworkInterface
				);
			else
				_avgBandwidthUsage->store(avgBandwidthUsage, memory_order_relaxed);
			SPDLOG_INFO(
				"bandwidthUsageThread, avgBandwidthInMbps"
				", avgBandwidthUsage: @{}@Mbps",
				static_cast<uint32_t>((avgBandwidthUsage * 8) / 1000000)
			);

			// loggo il picco
			for (const auto &[iface, stats] : peakInBytes)
			{
				if (_deliveryExternalNetworkInterface == iface)
				{
					auto [peakRx, peakTx] = stats;
					// messaggio usato da servicesStatusLibrary::mms_delivery_check_bandwidth_usage
					SPDLOG_INFO(
						"bandwidthUsageThread, peakBandwidthInMbps"
						", iface: {}"
						", peakTx: @{}@Mbps",
						iface, static_cast<uint32_t>((peakTx * 8) / 1000000)
					);
					break;
				}
			}
		}
		catch (exception& e)
		{
			SPDLOG_ERROR(
				"System::getBandwidthInMbps failed"
				", exception: {}",
				e.what()
			);
		}

		// aggiorniamo le bande usate da _externalDeliveriesGroups in modo che getMinBandwidthHost possa funzionare bene
		try
		{
			unordered_map<string, uint64_t> runningHostsBandwidth = _mmsDeliveryAuthorization->getExternalDeliveriesRunningHosts();

			SPDLOG_INFO(
				"bandwidthUsageThread, avgBandwidthInMbps"
				", runningHostsBandwidth.size: {}",
				runningHostsBandwidth.size()
			);

			if (!runningHostsBandwidth.empty())
			{
				for (auto &[runningHost, bandwidth] : runningHostsBandwidth)
				{
					try
					{
						string bandwidthUsageURL =
							std::format("{}://{}:{}/catramms/{}/avgBandwidthUsage", _apiProtocol, runningHost, _apiPort, _apiVersion);
						constexpr int bandwidthUsageTimeoutInSeconds = 2;
						json bandwidthUsageRoot = CurlWrapper::httpGetJson(bandwidthUsageURL, bandwidthUsageTimeoutInSeconds);

						bandwidth = JSONUtils::asUint64(bandwidthUsageRoot, "avgBandwidthUsage");
					}
					catch (exception &e)
					{
						// se una culr fallisce comunque andiamo avanti
						SPDLOG_ERROR(
							"bandwidthUsage failed"
							", exception: {}",
							e.what()
						);
					}
				}

				_mmsDeliveryAuthorization->updateExternalDeliveriesBandwidthHosts(runningHostsBandwidth);
			}
		}
		catch (exception& e)
		{
			SPDLOG_ERROR(
				"System::getBandwidthInMbps failed"
				", exception: {}",
				e.what()
			);
		}

		// inizializziamo la struttura BandwidthStats
		try
		{
			// addSample logs when a new day is started
			_bandwidthStats.addSample(avgBandwidthUsage, chrono::system_clock::now());
		}
		catch (exception& e)
		{
			SPDLOG_ERROR(
				"System::getBandwidthInMbps failed"
				", exception: {}",
				e.what()
			);
		}
	}
}

shared_ptr<FastCGIAPI::AuthorizationDetails> API::checkAuthorization(const string_view& sThreadId, const string_view& userName, const string_view& password)
{
	auto apiAuthorizationDetails = make_shared<APIAuthorizationDetails>();
	try
	{
		const tuple<int64_t, shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>
			userKeyWorkspaceAndFlags = _mmsEngineDBFacade->checkAPIKey(
				password,
				// 2022-12-18: controllo della apikey, non vedo motivi per mettere true
			false
			);

		apiAuthorizationDetails->userName = userName;
		apiAuthorizationDetails->password = password;
		apiAuthorizationDetails->userKey = get<0>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->workspace = get<1>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->admin = get<2>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canCreateRemoveWorkspace = get<3>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canIngestWorkflow = get<4>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canCreateProfiles = get<5>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canDeliveryAuthorization = get<6>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canShareWorkspace = get<7>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canEditMedia = get<8>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canEditConfiguration = get<9>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canKillEncoding = get<10>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canCancelIngestionJob = get<11>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canEditEncodersPool = get<12>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canApplicationRecorder = get<13>(userKeyWorkspaceAndFlags);
		apiAuthorizationDetails->canCreateRemoveLiveChannel = get<14>(userKeyWorkspaceAndFlags);
		return apiAuthorizationDetails;
	}
	catch (exception &e)
	{
		SPDLOG_INFO(
			"_mmsEngineDBFacade->checkAPIKey failed"
			", _requestIdentifier: {}"
			", threadId: {}"
			", apiKey: {}",
			_requestIdentifier, sThreadId, password
		);

		throw HTTPError(401);
	}

	if (apiAuthorizationDetails->userKey != StringUtils::toInt64(userName))
	{
		SPDLOG_INFO(
			"Username of the basic authorization (UserKey) is not the same UserKey the apiKey is referring"
			", _requestIdentifier: {}"
			", threadId: {}"
			", username of basic authorization (userKey): {}"
			", userKey associated to the APIKey: {}"
			", apiKey: {}",
			_requestIdentifier, sThreadId, userName, apiAuthorizationDetails->userKey, password
		);

		throw HTTPError(401);
	}
}

bool API::basicAuthenticationRequired(const string &requestURI, const unordered_map<string, string> &queryParameters)
{
	bool basicAuthenticationRequired = true;

	const string method = getMapParameter(queryParameters, "x-api-method", string(), false);
	if (method.empty())
	{
		SPDLOG_ERROR("The 'x-api-method' parameter is not found");

		return basicAuthenticationRequired;
	}

	if (method == "registerUser" || method == "confirmRegistration" || method == "createTokenToResetPassword" || method == "resetPassword" ||
		method == "login" || method == "manageHTTPStreamingManifest_authorizationThroughParameter" ||
		method == "deliveryAuthorizationThroughParameter" || method == "deliveryAuthorizationThroughPath" || method == "avgBandwidthUsage" ||
		method == "status" // often used as healthy check
	)
		basicAuthenticationRequired = false;

	// This is the authorization asked when the deliveryURL is received by nginx
	// Here the token is checked and it is not needed any basic authorization
	if (requestURI == "/catramms/delivery/authorization")
		basicAuthenticationRequired = false;

	return basicAuthenticationRequired;
}

void API::loadConfiguration(json configurationRoot, FileUploadProgressData *fileUploadProgressData)
{
	string encodingPriority = JSONUtils::asString(configurationRoot["api"]["workspaceDefaults"], "encodingPriority", "low");
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->encodingPriority: {}",
		encodingPriority
	);
	try
	{
		{
			fs::path versionPathFileName = "/opt/catramms/CatraMMS/version.txt";
			if (fs::exists(versionPathFileName) && fs::is_regular_file(versionPathFileName))
			{
				ifstream f(versionPathFileName);
				stringstream buffer;
				buffer << f.rdbuf();
				_mmsVersion = buffer.str();
			}
		}

		_encodingPriorityWorkspaceDefaultValue =
			MMSEngineDBFacade::toEncodingPriority(encodingPriority); // it generate an exception in case of wrong string
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"Configuration item is wrong. 'low' encoding priority is set"
			", api->encodingPriorityWorkspaceDefaultValue: {}",
			encodingPriority
		);

		_encodingPriorityWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;
	}

	_maxPageSize = JSONUtils::asInt(configurationRoot["postgres"], "maxPageSize", 5);
	SPDLOG_INFO(
		"Configuration item"
		", postgres->maxPageSize: {}",
		_maxPageSize
	);

	string encodingPeriod = JSONUtils::asString(configurationRoot["api"]["workspaceDefaults"], "encodingPeriod", "daily");
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->encodingPeriod: {}",
		encodingPeriod
	);
	if (encodingPeriod == "daily")
		_encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;
	else
		_encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;

	_maxIngestionsNumberWorkspaceDefaultValue = JSONUtils::asInt(configurationRoot["api"]["workspaceDefaults"], "maxIngestionsNumber", 100);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->maxIngestionsNumber: {}",
		_maxIngestionsNumberWorkspaceDefaultValue
	);
	_maxStorageInMBWorkspaceDefaultValue = JSONUtils::asInt(configurationRoot["api"]["workspaceDefaults"], "maxStorageInMB", 100);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->maxStorageInMBWorkspaceDefaultValue: {}",
		_maxStorageInMBWorkspaceDefaultValue
	);
	_expirationInDaysWorkspaceDefaultValue = JSONUtils::asInt(configurationRoot["api"]["workspaceDefaults"], "expirationInDays", 30);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->expirationInDaysWorkspaceDefaultValue: {}",
		_expirationInDaysWorkspaceDefaultValue
	);

	{
		json sharedEncodersPoolRoot = configurationRoot["api"]["sharedEncodersPool"];

		_sharedEncodersPoolLabel = JSONUtils::asString(sharedEncodersPoolRoot, "label", "");
		SPDLOG_INFO(
			"Configuration item"
			", api->sharedEncodersPool->label: {}",
			_sharedEncodersPoolLabel
		);

		_sharedEncodersLabel = sharedEncodersPoolRoot["encodersLabel"];
		SPDLOG_INFO(
			"Configuration item"
			", api->sharedEncodersPool->encodersLabel: {}",
			JSONUtils::toString(_sharedEncodersLabel)
		);
	}

	json apiRoot = configurationRoot["api"];

	_defaultSharedHLSChannelsNumber = JSONUtils::asInt(apiRoot, "defaultSharedHLSChannelsNumber", 1);
	SPDLOG_INFO(
		"Configuration item"
		", api->defaultSharedHLSChannelsNumber: {}",
		_defaultSharedHLSChannelsNumber
	);

	_apiProtocol = JSONUtils::asString(apiRoot, "protocol", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->protocol: {}",
		_apiProtocol
	);
	_apiHostname = JSONUtils::asString(apiRoot, "hostname", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->hostname: {}",
		_apiHostname
	);
	_apiPort = JSONUtils::asInt(apiRoot, "port", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->port: {}",
		_apiPort
	);
	_apiVersion = JSONUtils::asString(apiRoot, "version", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->version: {}",
		_apiVersion
	);

	// _binaryBufferLength             = api["binary"].get("binaryBufferLength", "XXX").asInt();
	// SPDLOG_INFO(__FILEREF__ + "Configuration item"
	//    + ", api->binary->binaryBufferLength: " + to_string(_binaryBufferLength)
	// );
	_progressUpdatePeriodInSeconds = JSONUtils::asInt(apiRoot["binary"], "progressUpdatePeriodInSeconds", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->progressUpdatePeriodInSeconds: {}",
		_progressUpdatePeriodInSeconds
	);
	_bandwidthUsagePeriodInSeconds = JSONUtils::asInt(apiRoot["binary"], "bandwidthUsagePeriodInSeconds", 15);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->bandwidthUsagePeriodInSeconds: {}",
		_bandwidthUsagePeriodInSeconds
	);
	_webServerPort = JSONUtils::asInt(apiRoot["binary"], "webServerPort", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->webServerPort: {}",
		_webServerPort
	);
	_maxProgressCallFailures = JSONUtils::asInt(apiRoot["binary"], "maxProgressCallFailures", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->maxProgressCallFailures: {}",
		_maxProgressCallFailures
	);
	_progressURI = JSONUtils::asString(apiRoot["binary"], "progressURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->progressURI: {}",
		_progressURI
	);

	_defaultTTLInSeconds = JSONUtils::asInt(apiRoot["delivery"], "defaultTTLInSeconds", 60);
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->defaultTTLInSeconds: {}",
		_defaultTTLInSeconds
	);

	_defaultMaxRetries = JSONUtils::asInt(apiRoot["delivery"], "defaultMaxRetries", 60);
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->defaultMaxRetries: {}",
		_defaultMaxRetries
	);

	_defaultRedirect = JSONUtils::asBool(apiRoot["delivery"], "defaultRedirect", true);
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->defaultRedirect: {}",
		_defaultRedirect
	);

	_deliveryProtocol = JSONUtils::asString(apiRoot["delivery"], "deliveryProtocol", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryProtocol: {}",
		_deliveryProtocol
	);
	_deliveryHost_authorizationThroughParameter = JSONUtils::asString(apiRoot["delivery"], "deliveryHost_authorizationThroughParameter", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryHost_authorizationThroughParameter: {}",
		_deliveryHost_authorizationThroughParameter
	);
	_deliveryHost_authorizationThroughPath = JSONUtils::asString(apiRoot["delivery"], "deliveryHost_authorizationThroughPath", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryHost_authorizationThroughPath: {}",
		_deliveryHost_authorizationThroughPath
	);

	_ldapEnabled = JSONUtils::asBool(apiRoot["activeDirectory"], "enabled", false);
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->enabled: {}",
		_ldapEnabled
	);
	_ldapURL = JSONUtils::asString(apiRoot["activeDirectory"], "ldapURL", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->ldapURL: {}",
		_ldapURL
	);
	_ldapCertificatePathName = JSONUtils::asString(apiRoot["activeDirectory"], "certificatePathName", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->certificatePathName: {}",
		_ldapCertificatePathName
	);
	_ldapManagerUserName = JSONUtils::asString(apiRoot["activeDirectory"], "managerUserName", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->managerUserName: {}",
		_ldapManagerUserName
	);
	_ldapManagerPassword = JSONUtils::asString(apiRoot["activeDirectory"], "managerPassword", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->managerPassword: {}",
		_ldapManagerPassword
	);
	_ldapBaseDn = JSONUtils::asString(apiRoot["activeDirectory"], "baseDn", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->baseDn: {}",
		_ldapBaseDn
	);
	_ldapDefaultWorkspaceKeys = JSONUtils::asString(apiRoot["activeDirectory"], "defaultWorkspaceKeys", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->defaultWorkspaceKeys: {}",
		_ldapDefaultWorkspaceKeys
	);

	_registerUserEnabled = JSONUtils::asBool(apiRoot, "registerUserEnabled", false);
	SPDLOG_INFO(
		"Configuration item"
		", api->registerUserEnabled: {}",
		_registerUserEnabled
	);

	/*
	_ffmpegEncoderProtocol = _configuration["ffmpeg"].get("encoderProtocol", "").asString();
	SPDLOG_INFO(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderProtocol: " + _ffmpegEncoderProtocol
	);
	_ffmpegEncoderPort = JSONUtils::asInt(_configuration["ffmpeg"], "encoderPort", 0);
	SPDLOG_INFO(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderPort: " + to_string(_ffmpegEncoderPort)
	);
	*/
	_ffmpegEncoderUser = JSONUtils::asString(configurationRoot["ffmpeg"], "encoderUser", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderUser: {}",
		_ffmpegEncoderUser
	);
	_ffmpegEncoderPassword = JSONUtils::asString(configurationRoot["ffmpeg"], "encoderPassword", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderPassword: {}",
		"..."
	);
	_ffmpegEncoderTimeoutInSeconds = JSONUtils::asInt(configurationRoot["ffmpeg"], "encoderTimeoutInSeconds", 120);
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderTimeoutInSeconds: {}",
		_ffmpegEncoderTimeoutInSeconds
	);
	_ffmpegEncoderKillEncodingURI = JSONUtils::asString(configurationRoot["ffmpeg"], "encoderKillEncodingURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderKillEncodingURI: {}",
		_ffmpegEncoderKillEncodingURI
	);
	_ffmpegEncoderChangeLiveProxyPlaylistURI = JSONUtils::asString(configurationRoot["ffmpeg"], "encoderChangeLiveProxyPlaylistURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderChangeLiveProxyPlaylistURI: {}",
		_ffmpegEncoderChangeLiveProxyPlaylistURI
	);
	_ffmpegEncoderChangeLiveProxyOverlayTextURI = JSONUtils::asString(configurationRoot["ffmpeg"], "encoderChangeLiveProxyOverlayTextURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderChangeLiveProxyOverlayTextURI: {}",
		_ffmpegEncoderChangeLiveProxyOverlayTextURI
	);

	_intervalInSecondsToCheckEncodingFinished = JSONUtils::asInt(configurationRoot["encoding"], "intervalInSecondsToCheckEncodingFinished", 0);
	SPDLOG_INFO(
		"Configuration item"
		", encoding->intervalInSecondsToCheckEncodingFinished: {}",
		_intervalInSecondsToCheckEncodingFinished
	);

	_maxSecondsToWaitAPIIngestionLock = JSONUtils::asInt(configurationRoot["mms"]["locks"], "maxSecondsToWaitAPIIngestionLock", 0);
	SPDLOG_INFO(
		"Configuration item"
		", mms->locks->maxSecondsToWaitAPIIngestionLock: {}",
		_maxSecondsToWaitAPIIngestionLock
	);

	_keyPairId = JSONUtils::asString(configurationRoot["aws"], "keyPairId", "");
	SPDLOG_INFO(
		"Configuration item"
		", aws->keyPairId: {}",
		_keyPairId
	);
	_privateKeyPEMPathName = JSONUtils::asString(configurationRoot["aws"], "privateKeyPEMPathName", "");
	SPDLOG_INFO(
		"Configuration item"
		", aws->privateKeyPEMPathName: {}",
		_privateKeyPEMPathName
	);
	/*
	_vodCloudFrontHostNamesRoot = configurationRoot["aws"]["vodCloudFrontHostNames"];
	SPDLOG_INFO(
		"Configuration item"
		", aws->vodCloudFrontHostNames: {}",
		"..."
	);
	*/

	_emailProviderURL = JSONUtils::asString(configurationRoot["EmailNotification"], "providerURL", "");
	SPDLOG_INFO(
		"Configuration item"
		", EmailNotification->providerURL: {}",
		_emailProviderURL
	);
	_emailUserName = JSONUtils::asString(configurationRoot["EmailNotification"], "userName", "");
	SPDLOG_INFO(
		"Configuration item"
		", EmailNotification->userName: {}",
		_emailUserName
	);
	{
		string encryptedPassword = JSONUtils::asString(configurationRoot["EmailNotification"], "password", "");
		_emailPassword = Encrypt::opensslDecrypt(encryptedPassword);
		SPDLOG_INFO(
			"Configuration item"
			", EmailNotification->password: {}",
			encryptedPassword
			// + ", _emailPassword: " + _emailPassword
		);
	}
	_emailCcsCommaSeparated = JSONUtils::asString(configurationRoot["EmailNotification"], "cc", "");
	SPDLOG_INFO(
		"Configuration item"
		", EmailNotification->cc: {}",
		_emailCcsCommaSeparated
	);

	_guiProtocol = JSONUtils::asString(configurationRoot["mms"], "guiProtocol", "");
	SPDLOG_INFO(
		"Configuration item"
		", mms->guiProtocol: {}",
		_guiProtocol
	);
	_guiHostname = JSONUtils::asString(configurationRoot["mms"], "guiHostname", "");
	SPDLOG_INFO(
		"Configuration item"
		", mms->guiHostname: {}",
		_guiHostname
	);
	_guiPort = JSONUtils::asInt(_configurationRoot["mms"], "guiPort", 0);
	SPDLOG_INFO(
		"Configuration item"
		", mms->guiPort: {}",
		_guiPort
	);

	_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(configurationRoot["storage"], "waitingNFSSync_maxMillisecondsToWait", 60000);
	SPDLOG_INFO(
		"Configuration item"
		", storage->_waitingNFSSync_maxMillisecondsToWait: {}",
		_waitingNFSSync_maxMillisecondsToWait
	);
	_waitingNFSSync_milliSecondsWaitingBetweenChecks =
		JSONUtils::asInt(configurationRoot["storage"], "waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	SPDLOG_INFO(
		"Configuration item"
		", storage->waitingNFSSync_milliSecondsWaitingBetweenChecks: {}",
		_waitingNFSSync_milliSecondsWaitingBetweenChecks
	);

	_fileUploadProgressData = fileUploadProgressData;
	_fileUploadProgressThreadShutdown = false;

	_bandwidthUsageThreadShutdown = false;

	try
	{
		vector<tuple<string, string, bool, string>> nativeNetworkInterfaces = System::getActiveNetworkInterface();
		for (const auto &[interfaceName, interfaceType, privateIp, ipAddress] : nativeNetworkInterfaces)
		{
			SPDLOG_INFO(
				"getActiveNetworkInterface"
				", interface name: {}"
				", interface type: {}"
				", private ip: {}"
				", ip address: {}",
				interfaceName, interfaceType, privateIp, ipAddress
			);
			if (interfaceType != "IPv4" || privateIp)
				continue; // rete interna
			_deliveryExternalNetworkInterface = interfaceName;
		}
		SPDLOG_INFO(
			"getActiveNetworkInterface"
			", _deliveryExternalNetworkInterface: {}",
			_deliveryExternalNetworkInterface
		);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"System::getActiveNetworkInterface failed"
			", exception: {}",
			e.what()
		);
	}
}