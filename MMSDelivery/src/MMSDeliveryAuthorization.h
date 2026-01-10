/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   MMSDeliveryAuthorization.h
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "../../CatraLibraries/BandwidthUsageThread/src/HostsBandwidthTracker.h"
#include "MMSStorage.h"
#include "spdlog/spdlog.h"
// #include <fstream>

// using namespace std;

class MMSDeliveryAuthorization
{
  public:
	MMSDeliveryAuthorization(
		const nlohmann::json &configuration, const std::shared_ptr<MMSStorage> &mmsStorage, const std::shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade);

	std::pair<std::string, std::string> createDeliveryAuthorization(
		int64_t userKey, const std::shared_ptr<Workspace> &requestWorkspace, const std::string &playerIP, int64_t mediaItemKey, const std::string &uniqueName,
		int64_t encodingProfileKey, const std::string &encodingProfileLabel, int64_t physicalPathKey, int64_t ingestionJobKey, int64_t deliveryCode,
		int ttlInSeconds, int maxRetries, bool reuseAuthIfPresent, bool playerIPToBeAuthorized, const std::string &playerCountry,
		const std::string &playerRegion, bool save, const std::string &deliveryType, bool warningIfMissingMediaItemKey, bool filteredByStatistic,
		const std::string &userId
	);

	[[nodiscard]] std::string checkDeliveryAuthorizationThroughParameter(const std::string &contentURI, const std::string &tokenParameter) const;

	int64_t checkDeliveryAuthorizationThroughPath(const std::string &contentURI);

	std::string checkDeliveryAuthorizationOfAManifest(bool secondaryManifest, const std::string &token, const std::string &cookie, const std::string &contentURI);

	static int64_t checkSignedMMSPath(std::string tokenSigned, std::string contentURIToBeVerified);

	static std::string getSignedCDN77URL(
		const std::string &resourceURL,
		// i.e.: 1234456789.rsc.cdn77.org
		const std::string &filePath,
		// /file/playlist/d.m3u8
		const std::string &secureToken, long expirationInSeconds, std::string playerIP = ""
	);
	static std::string getMedianovaSignedTokenURL(
		const std::string& playURLProtocol, const std::string& playURLHostname, const std::string& uri, const std::string& secureToken,
		long expirationInSeconds, const std::string& playerIP, bool uriEnabled, bool playerIPEnabled = false
);

	std::unordered_map<std::string, uint64_t> getExternalDeliveriesRunningHosts();
	void updateExternalDeliveriesBandwidthHosts(const std::unordered_map<std::string, uint64_t> &hostsBandwidth);

	void startUpdateExternalDeliveriesGroupsBandwidthUsageThread();
	void stopUpdateExternalDeliveriesGroupsBandwidthUsageThread();

private:
	nlohmann::json _configuration;
	std::shared_ptr<MMSStorage> _mmsStorage;
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;

	std::mutex _externalDeliveriesMutex;
	std::map<std::string, std::shared_ptr<HostsBandwidthTracker>> _externalDeliveriesGroups;

	std::string _keyPairId;
	std::string _privateKeyPEMPathName;
	std::string _vodCloudFrontHostName;			   // token as parameter
	std::string _vodDeliveryCloudFrontHostName;	   // token as parameter
	std::string _vodDeliveryPathCloudFrontHostName; // url signed

	std::string _deliveryProtocol;
	std::string _deliveryHost_authorizationThroughParameter;
	std::string _deliveryHost_authorizationThroughPath;

	std::string _apiProtocol;
	int32_t _apiPort;
	std::string _apiVersion;
	bool _updateExternalDeliveriesGroupsBandwidthUsageThreadStop;
	std::thread _updateExternalDeliveriesGroupsBandwidthUsageThread;

	static std::string getSignedMMSPath(const std::string &contentURI, time_t expirationTime);
	static time_t getExpirationTime(int ttlInSeconds, bool reusable);
	std::string getDeliveryHost(
		const std::shared_ptr<Workspace> &requestWorkspace, const std::string &playerCountry, const std::string &playerRegion, const std::string &defaultDeliveryHost
	);
	std::shared_ptr<HostsBandwidthTracker> getHostBandwidthTracker(int64_t workspaceKey, const std::string &groupName, const nlohmann::json &hostGroupRoot);

	void updateExternalDeliveriesGroupsBandwidthUsageThread();
};
