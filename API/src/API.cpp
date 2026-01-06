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

#include "JsonPath.h"

using namespace std;
using json = nlohmann::json;

API::API(
	const bool noFileSystemAccess, const json &configurationRoot, const shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade,
	const shared_ptr<MMSStorage> &mmsStorage, const shared_ptr<MMSDeliveryAuthorization> &mmsDeliveryAuthorization, mutex *fcgiAcceptMutex,
	FileUploadProgressData *fileUploadProgressData, const std::shared_ptr<BandwidthUsageThread>& bandwidthUsageThread
)
	: FastCGIAPI(configurationRoot, fcgiAcceptMutex), _mmsEngineDBFacade(mmsEngineDBFacade), _noFileSystemAccess(noFileSystemAccess),
	  _mmsStorage(mmsStorage), _mmsDeliveryAuthorization(mmsDeliveryAuthorization)
{
	_configurationRoot = configurationRoot;
	_bandwidthUsageThread = bandwidthUsageThread;

	loadConfiguration(configurationRoot, fileUploadProgressData);

	registerHandler(
		"status", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ status(sThreadId, request, requestData); }
	);
	registerHandler(
		"avgBandwidthUsage",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ avgBandwidthUsage(sThreadId, request, requestData); }
	);
	registerHandler(
		"binaryAuthorization",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ binaryAuthorization(sThreadId, request, requestData); }
	);
	registerHandler(
		"deliveryAuthorizationThroughParameter",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ deliveryAuthorizationThroughParameter(sThreadId, request, requestData); }
	);
	registerHandler(
		"deliveryAuthorizationThroughPath",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ deliveryAuthorizationThroughPath(sThreadId, request, requestData); }
	);
	registerHandler(
		"manageHTTPStreamingManifest_authorizationThroughParameter",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ manageHTTPStreamingManifest_authorizationThroughParameter(sThreadId, request, requestData); }
	);
	registerHandler(
		"login", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ login(sThreadId, request, requestData); }
	);
	registerHandler(
		"registerUser", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ registerUser(sThreadId, request, requestData); }
	);
	registerHandler(
		"updateUser", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ updateUser(sThreadId, request, requestData); }
	);
	registerHandler(
		"createTokenToResetPassword",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ createTokenToResetPassword(sThreadId, request, requestData); }
	);
	registerHandler(
		"resetPassword", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ resetPassword(sThreadId, request, requestData); }
	);
	registerHandler(
		"updateWorkspace", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ updateWorkspace(sThreadId, request, requestData); }
	);
	registerHandler(
		"setWorkspaceAsDefault",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ setWorkspaceAsDefault(sThreadId, request, requestData); }
	);
	registerHandler(
		"createWorkspace", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ createWorkspace(sThreadId, request, requestData); }
	);
	registerHandler(
		"deleteWorkspace", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ deleteWorkspace(sThreadId, request, requestData); }
	);
	registerHandler(
		"unshareWorkspace", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ unshareWorkspace(sThreadId, request, requestData); }
	);
	registerHandler(
		"workspaceUsage", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ workspaceUsage(sThreadId, request, requestData); }
	);
	registerHandler(
		"shareWorkspace", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ shareWorkspace_(sThreadId, request, requestData); }
	);
	registerHandler(
		"workspaceList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ workspaceList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addInvoice", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addInvoice(sThreadId, request, requestData); }
	);
	registerHandler(
		"invoiceList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ invoiceList(sThreadId, request, requestData); }
	);
	registerHandler(
		"confirmRegistration",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ confirmRegistration(sThreadId, request, requestData); }
	);
	registerHandler(
		"addEncoder", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addEncoder(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeEncoder", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeEncoder(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyEncoder", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyEncoder(sThreadId, request, requestData); }
	);
	registerHandler(
		"encoderList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ encoderList(sThreadId, request, requestData); }
	);
	registerHandler(
		"encodersPoolList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ encodersPoolList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addEncodersPool", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addEncodersPool(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyEncodersPool",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyEncodersPool(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeEncodersPool",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeEncodersPool(sThreadId, request, requestData); }
	);
	registerHandler(
		"addAssociationWorkspaceEncoder",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addAssociationWorkspaceEncoder(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeAssociationWorkspaceEncoder",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeAssociationWorkspaceEncoder(sThreadId, request, requestData); }
	);
	registerHandler(
		"createDeliveryAuthorization",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ createDeliveryAuthorization(sThreadId, request, requestData); }
	);
	registerHandler(
		"createBulkOfDeliveryAuthorization",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ createBulkOfDeliveryAuthorization(sThreadId, request, requestData); }
	);
	registerHandler(
		"ingestion", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ ingestion(sThreadId, request, requestData); }
	);
	registerHandler(
		"ingestionRootsStatus",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ ingestionRootsStatus(sThreadId, request, requestData); }
	);
	registerHandler(
		"ingestionRootMetaDataContent",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ ingestionRootMetaDataContent(sThreadId, request, requestData); }
	);
	registerHandler(
		"ingestionJobsStatus",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ ingestionJobsStatus(sThreadId, request, requestData); }
	);
	registerHandler(
		"cancelIngestionJob",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ cancelIngestionJob(sThreadId, request, requestData); }
	);
	registerHandler(
		"updateIngestionJob",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ updateIngestionJob(sThreadId, request, requestData); }
	);
	registerHandler(
		"ingestionJobSwitchToEncoder",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ ingestionJobSwitchToEncoder(sThreadId, request, requestData); }
	);
	registerHandler(
		"encodingJobsStatus",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ encodingJobsStatus(sThreadId, request, requestData); }
	);
	registerHandler(
		"encodingJobPriority",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ encodingJobPriority(sThreadId, request, requestData); }
	);
	registerHandler(
		"killOrCancelEncodingJob",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ killOrCancelEncodingJob(sThreadId, request, requestData); }
	);
	registerHandler(
		"changeLiveProxyPlaylist",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ changeLiveProxyPlaylist(sThreadId, request, requestData); }
	);
	registerHandler(
		"changeLiveProxyOverlayText",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ changeLiveProxyOverlayText(sThreadId, request, requestData); }
	);
	registerHandler(
		"mediaItemsList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ mediaItemsList(sThreadId, request, requestData); }
	);
	registerHandler(
		"updateMediaItem", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ updateMediaItem(sThreadId, request, requestData); }
	);
	registerHandler(
		"updatePhysicalPath",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ updatePhysicalPath(sThreadId, request, requestData); }
	);
	registerHandler(
		"tagsList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ tagsList(sThreadId, request, requestData); }
	);
	registerHandler(
		"uploadedBinary", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ uploadedBinary(sThreadId, request, requestData); }
	);
	registerHandler(
		"addUpdateEncodingProfilesSet",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addUpdateEncodingProfilesSet(sThreadId, request, requestData); }
	);
	registerHandler(
		"encodingProfilesSetsList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ encodingProfilesSetsList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addEncodingProfile",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addEncodingProfile(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeEncodingProfile",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeEncodingProfile(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeEncodingProfilesSet",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeEncodingProfilesSet(sThreadId, request, requestData); }
	);
	registerHandler(
		"encodingProfilesList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ encodingProfilesList(sThreadId, request, requestData); }
	);
	registerHandler(
		"workflowsAsLibraryList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ workflowsAsLibraryList(sThreadId, request, requestData); }
	);
	registerHandler(
		"workflowAsLibraryContent",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ workflowAsLibraryContent(sThreadId, request, requestData); }
	);
	registerHandler(
		"saveWorkflowAsLibrary",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ saveWorkflowAsLibrary(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeWorkflowAsLibrary",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeWorkflowAsLibrary(sThreadId, request, requestData); }
	);
	registerHandler(
		"mmsSupport", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ mmsSupport(sThreadId, request, requestData); }
	);
	registerHandler(
		"addYouTubeConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addYouTubeConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyYouTubeConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyYouTubeConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeYouTubeConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeYouTubeConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"youTubeConfList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ youTubeConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addFacebookConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addFacebookConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyFacebookConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyFacebookConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeFacebookConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeFacebookConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"facebookConfList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ facebookConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addTwitchConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addTwitchConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyTwitchConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyTwitchConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeTwitchConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeTwitchConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"twitchConfList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ twitchConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addStream", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addStream(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyStream", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyStream(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeStream", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeStream(sThreadId, request, requestData); }
	);
	registerHandler(
		"streamList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ streamList(sThreadId, request, requestData); }
	);
	registerHandler(
		"streamFreePushEncoderPort",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ streamFreePushEncoderPort(sThreadId, request, requestData); }
	);
	registerHandler(
		"addSourceTVStream",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addSourceTVStream(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifySourceTVStream",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifySourceTVStream(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeSourceTVStream",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeSourceTVStream(sThreadId, request, requestData); }
	);
	registerHandler(
		"sourceTVStreamList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ sourceTVStreamList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addRTMPChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addRTMPChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyRTMPChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyRTMPChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeRTMPChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeRTMPChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"rtmpChannelConfList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ rtmpChannelConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addSRTChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addSRTChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifySRTChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifySRTChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeSRTChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeSRTChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"srtChannelConfList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ srtChannelConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addHLSChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addHLSChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyHLSChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyHLSChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeHLSChannelConf",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeHLSChannelConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"hlsChannelConfList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ hlsChannelConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addFTPConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addFTPConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyFTPConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyFTPConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeFTPConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeFTPConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"ftpConfList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ ftpConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addEMailConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addEMailConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"modifyEMailConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ modifyEMailConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"removeEMailConf", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ removeEMailConf(sThreadId, request, requestData); }
	);
	registerHandler(
		"emailConfList", [this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ emailConfList(sThreadId, request, requestData); }
	);
	registerHandler(
		"loginStatisticList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ loginStatisticList(sThreadId, request, requestData); }
	);
	registerHandler(
		"addRequestStatistic",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ addRequestStatistic(sThreadId, request, requestData); }
	);
	registerHandler(
		"requestStatisticList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ requestStatisticList(sThreadId, request, requestData); }
	);
	registerHandler(
		"requestStatisticPerContentList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ requestStatisticPerContentList(sThreadId, request, requestData); }
	);
	registerHandler(
		"requestStatisticPerUserList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ requestStatisticPerUserList(sThreadId, request, requestData); }
	);
	registerHandler(
		"requestStatisticPerMonthList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ requestStatisticPerMonthList(sThreadId, request, requestData); }
	);
	registerHandler(
		"requestStatisticPerDayList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ requestStatisticPerDayList(sThreadId, request, requestData); }
	);
	registerHandler(
		"requestStatisticPerHourList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ requestStatisticPerHourList(sThreadId, request, requestData); }
	);
	registerHandler(
		"requestStatisticPerCountryList",
		[this](const string_view &sThreadId, FCGX_Request &request, const FCGIRequestData &requestData)
		{ requestStatisticPerCountryList(sThreadId, request, requestData); }
	);
}

API::~API() = default;

void API::manageRequestAndResponse(const string_view &sThreadId, /* int64_t requestIdentifier, */ FCGX_Request &request, const FCGIRequestData &requestData)
{
	bool basicAuthenticationPresent = requestData.authorizationDetails != nullptr;
	const shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails =
		static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	if (basicAuthenticationPresent)
	{
		SPDLOG_INFO(
			"Received manageRequestAndResponse"
			", requestData.requestURI: {}"
			", requestData.requestMethod: {}"
			", contentLength: {}"
			", userKey: {}"
			", workspace->_name: {}"
			", requestData.requestBody: {}"
			", flags: {}",
			requestData.requestURI, requestData.requestMethod, requestData.contentLength, apiAuthorizationDetails->userKey,
			apiAuthorizationDetails->workspace->_name, requestData.requestBody, apiAuthorizationDetails->toString()
		);
	}

	if (!basicAuthenticationPresent)
	{
		SPDLOG_INFO(
			"Received manageRequestAndResponse"
			", requestData.requestURI: {}"
			", requestData.requestMethod: {}"
			", contentLength: {}",
			requestData.requestURI, requestData.requestMethod, requestData.contentLength
		);
	}

	try
	{
		handleRequest(sThreadId, /* requestIdentifier, */ request, requestData, true);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"manage request failed"
			", requestData.requestBody: {}"
			", e.what(): {}",
			requestData.requestBody, e.what()
		);

		int htmlResponseCode = 500;
		string errorMessage;
		if (dynamic_cast<FCGIRequestData::HTTPError *>(&e))
		{
			htmlResponseCode = dynamic_cast<FCGIRequestData::HTTPError *>(&e)->httpErrorCode;
			errorMessage = e.what();
		}
		else
			errorMessage = FCGIRequestData::getHtmlStandardMessage(htmlResponseCode);

		SPDLOG_ERROR(errorMessage);

		sendError(request, htmlResponseCode, errorMessage);

		throw;
	}
}

void API::mmsSupport(
	const string_view &sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "mmsSupport";

	SPDLOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	try
	{
		string userEmailAddress;
		string subject;
		string text;

		json metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);

		vector<string> mandatoryFields = {"UserEmailAddress", "Subject", "Text"};
		for (string field : mandatoryFields)
		{
			if (!JSONUtils::isPresent(metadataRoot, field))
			{
				string errorMessage = std::format(
					"Json field is not present or it is null"
					", Json field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw FCGIRequestData::HTTPError(400);
			}
		}

		userEmailAddress = JSONUtils::asString(metadataRoot, "UserEmailAddress", "");
		subject = JSONUtils::asString(metadataRoot, "Subject", "");
		text = JSONUtils::asString(metadataRoot, "Text", "");

		{
			shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

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
			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);

		throw;
	}
}

void API::status(
	const string_view &sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "status";

	SPDLOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	try
	{
		json statusRoot;

		statusRoot["status"] = "API server up and running";
		// statusRoot["version-api"] = version;

		string sJson = JSONUtils::toString(statusRoot);

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, requestData.requestURI, requestData.requestMethod, 200, sJson);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);

		throw;
	}
}

void API::avgBandwidthUsage(
	const string_view &sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "avgBandwidthUsage";

	SPDLOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	try
	{
		json statusRoot;

		statusRoot["avgBandwidthUsage"] = _bandwidthUsageThread->getAvgBandwidthUsage();

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, requestData.requestURI, requestData.requestMethod, 200, JSONUtils::toString(statusRoot));
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);

		throw;
	}
}

void API::manageHTTPStreamingManifest_authorizationThroughParameter(
	const string_view &sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "manageHTTPStreamingManifest_authorizationThroughParameter";

	SPDLOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
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

		string token = requestData.getQueryParameter("token", "", true);

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

		bool isNumber = StringUtils::isNumber(token);
		if (isNumber || token.find(",") != string::npos)
		{
			secondaryManifest = false;
			// tokenComingFromURL = stoll(tokenIt->second);
			tokenComingFromURL = token;
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
			token, isNumber, token, secondaryManifest
		);

		string contentURI;
		{
			size_t endOfURIIndex = requestData.requestURI.find_last_of('?');
			if (endOfURIIndex == string::npos)
			{
				string errorMessage = std::format(
					"Wrong URI format"
					", requestData.requestURI: {}",
					requestData.requestURI
				);
				SPDLOG_INFO(errorMessage);

				throw runtime_error(errorMessage);
			}
			contentURI = requestData.requestURI.substr(0, endOfURIIndex);
		}

		if (secondaryManifest)
		{
			string cookie = requestData.getQueryParameter("cookie", "", true);

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
			string mmsInfoCookie = requestData.getQueryParameter("cookie", "", false);

			tokenComingFromURL =
				_mmsDeliveryAuthorization->checkDeliveryAuthorizationOfAManifest(secondaryManifest, tokenComingFromURL, mmsInfoCookie, contentURI);
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
						xmlSetProp(nodes->nodeTab[nodeIndex], BAD_CAST initializationAttributeName, BAD_CAST newInitializationAttributeValue.c_str());

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
			string originHeader = requestData.getHeaderParameter("origin", "");
			if (secondaryManifest)
				sendSuccess(
					sThreadId, requestData.responseBodyCompressed, request, requestData.requestURI, requestData.requestMethod, 200, responseBody, contentType, "", "",
					"", enableCorsGETHeader, originHeader
				);
			else
				sendSuccess(
					sThreadId, requestData.responseBodyCompressed, request, requestData.requestURI, requestData.requestMethod, 200, responseBody, contentType,
					cookieName, cookieValue, cookiePath, enableCorsGETHeader, originHeader
				);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);

		throw FCGIRequestData::HTTPError(403);
	}
}

void API::sendError(FCGX_Request &request, int htmlResponseCode, const string_view &errorMessage)
{
	json responseBodyRoot;
	responseBodyRoot["status"] = to_string(htmlResponseCode);
	responseBodyRoot["error"] = errorMessage;

	FastCGIAPI::sendError(request, htmlResponseCode, JSONUtils::toString(responseBodyRoot));
}

shared_ptr<FCGIRequestData::AuthorizationDetails> API::checkAuthorization(const string_view &sThreadId,
	const FCGIRequestData& requestData, const string_view &userName, const string_view &password)
{
	auto apiAuthorizationDetails = make_shared<APIAuthorizationDetails>();
	try
	{
		apiAuthorizationDetails->userName = userName;
		apiAuthorizationDetails->password = password;
		tie (apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace, apiAuthorizationDetails->admin,
			apiAuthorizationDetails->canCreateRemoveWorkspace, apiAuthorizationDetails->canIngestWorkflow,
			apiAuthorizationDetails->canCreateProfiles, apiAuthorizationDetails->canDeliveryAuthorization,
			apiAuthorizationDetails->canShareWorkspace, apiAuthorizationDetails->canEditMedia,
			apiAuthorizationDetails->canEditConfiguration, apiAuthorizationDetails->canKillEncoding,
			apiAuthorizationDetails->canCancelIngestionJob, apiAuthorizationDetails->canEditEncodersPool,
			apiAuthorizationDetails->canApplicationRecorder, apiAuthorizationDetails->canCreateRemoveLiveChannel,
			apiAuthorizationDetails->canUpdateEncoderStats) =
				_mmsEngineDBFacade->checkAPIKey(password,
				// 2022-12-18: controllo della apikey, non vedo motivi per mettere true
				false
			);

		return apiAuthorizationDetails;
	}
	catch (exception &e)
	{
		SPDLOG_INFO(
			"_mmsEngineDBFacade->checkAPIKey failed"
			", threadId: {}"
			", apiKey: {}"
			", exception: {}",
			sThreadId, password, e.what()
		);

		throw FCGIRequestData::HTTPError(401);
	}

	if (apiAuthorizationDetails->userKey != StringUtils::toInt64(userName))
	{
		SPDLOG_INFO(
			"Username of the basic authorization (UserKey) is not the same UserKey the apiKey is referring"
			", threadId: {}"
			", username of basic authorization (userKey): {}"
			", userKey associated to the APIKey: {}"
			", apiKey: {}",
			sThreadId, userName, apiAuthorizationDetails->userKey, password
		);

		throw FCGIRequestData::HTTPError(401);
	}
}

bool API::basicAuthenticationRequired(const FCGIRequestData &requestData)
{
	bool basicAuthenticationRequired = true;

	const string method = requestData.getQueryParameter("x-api-method");
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
	if (requestData.requestURI == "/catramms/delivery/authorization")
		basicAuthenticationRequired = false;

	return basicAuthenticationRequired;
}

void API::loadConfiguration(const json &configurationRoot, FileUploadProgressData *fileUploadProgressData)
{
	auto encodingPriority = JsonPath(&configurationRoot)["api"]["workspaceDefaults"]["encodingPriority"].as<string>("low");
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

	_maxPageSize = JSONUtils::asInt32(configurationRoot["postgres"], "maxPageSize", 5);
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

	_maxIngestionsNumberWorkspaceDefaultValue = JSONUtils::asInt32(configurationRoot["api"]["workspaceDefaults"], "maxIngestionsNumber", 100);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->maxIngestionsNumber: {}",
		_maxIngestionsNumberWorkspaceDefaultValue
	);
	_maxStorageInMBWorkspaceDefaultValue = JSONUtils::asInt32(configurationRoot["api"]["workspaceDefaults"], "maxStorageInMB", 100);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->maxStorageInMBWorkspaceDefaultValue: {}",
		_maxStorageInMBWorkspaceDefaultValue
	);
	_expirationInDaysWorkspaceDefaultValue = JSONUtils::asInt32(configurationRoot["api"]["workspaceDefaults"], "expirationInDays", 30);
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

	_defaultSharedHLSChannelsNumber = JSONUtils::asInt32(apiRoot, "defaultSharedHLSChannelsNumber", 1);
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
	_apiPort = JSONUtils::asInt32(apiRoot, "port", 0);
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
	_progressUpdatePeriodInSeconds = JSONUtils::asInt32(apiRoot["binary"], "progressUpdatePeriodInSeconds", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->progressUpdatePeriodInSeconds: {}",
		_progressUpdatePeriodInSeconds
	);
	_webServerPort = JSONUtils::asInt32(apiRoot["binary"], "webServerPort", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->webServerPort: {}",
		_webServerPort
	);
	_maxProgressCallFailures = JSONUtils::asInt32(apiRoot["binary"], "maxProgressCallFailures", 0);
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

	_defaultTTLInSeconds = JSONUtils::asInt32(apiRoot["delivery"], "defaultTTLInSeconds", 60);
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->defaultTTLInSeconds: {}",
		_defaultTTLInSeconds
	);

	_defaultMaxRetries = JSONUtils::asInt32(apiRoot["delivery"], "defaultMaxRetries", 60);
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
	_ffmpegEncoderPort = JSONUtils::asInt32(_configuration["ffmpeg"], "encoderPort", 0);
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
	_ffmpegEncoderTimeoutInSeconds = JSONUtils::asInt32(configurationRoot["ffmpeg"], "encoderTimeoutInSeconds", 120);
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

	_intervalInSecondsToCheckEncodingFinished = JSONUtils::asInt32(configurationRoot["encoding"], "intervalInSecondsToCheckEncodingFinished", 0);
	SPDLOG_INFO(
		"Configuration item"
		", encoding->intervalInSecondsToCheckEncodingFinished: {}",
		_intervalInSecondsToCheckEncodingFinished
	);

	_maxSecondsToWaitAPIIngestionLock = JSONUtils::asInt32(configurationRoot["mms"]["locks"], "maxSecondsToWaitAPIIngestionLock", 0);
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
	_guiPort = JSONUtils::asInt32(_configurationRoot["mms"], "guiPort", 0);
	SPDLOG_INFO(
		"Configuration item"
		", mms->guiPort: {}",
		_guiPort
	);

	_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt32(configurationRoot["storage"], "waitingNFSSync_maxMillisecondsToWait", 60000);
	SPDLOG_INFO(
		"Configuration item"
		", storage->_waitingNFSSync_maxMillisecondsToWait: {}",
		_waitingNFSSync_maxMillisecondsToWait
	);
	_waitingNFSSync_milliSecondsWaitingBetweenChecks =
		JSONUtils::asInt32(configurationRoot["storage"], "waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	SPDLOG_INFO(
		"Configuration item"
		", storage->waitingNFSSync_milliSecondsWaitingBetweenChecks: {}",
		_waitingNFSSync_milliSecondsWaitingBetweenChecks
	);

	_fileUploadProgressData = fileUploadProgressData;
	_fileUploadProgressThreadShutdown = false;
}
