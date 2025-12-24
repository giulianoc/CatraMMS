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

#ifndef MMSDeliveryAuthorization_h
#define MMSDeliveryAuthorization_h

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "HostBandwidthTracker.h"
#include "MMSStorage.h"
#include "spdlog/spdlog.h"
// #include <fstream>

using namespace std;

class MMSDeliveryAuthorization
{
  public:
	MMSDeliveryAuthorization(
		const json &configuration, const shared_ptr<MMSStorage> &mmsStorage, const shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade);

	pair<string, string> createDeliveryAuthorization(
		int64_t userKey, const shared_ptr<Workspace> &requestWorkspace, const string &playerIP, int64_t mediaItemKey, const string &uniqueName,
		int64_t encodingProfileKey, const string &encodingProfileLabel, int64_t physicalPathKey, int64_t ingestionJobKey, int64_t deliveryCode,
		int ttlInSeconds, int maxRetries, bool reuseAuthIfPresent, bool playerIPToBeAuthorized, const string &playerCountry,
		const string &playerRegion, bool save, const string &deliveryType, bool warningIfMissingMediaItemKey, bool filteredByStatistic,
		const string &userId
	);

	string checkDeliveryAuthorizationThroughParameter(const string &contentURI, const string &tokenParameter);

	int64_t checkDeliveryAuthorizationThroughPath(const string &contentURI);

	string checkDeliveryAuthorizationOfAManifest(bool secondaryManifest, const string &token, const string &cookie, const string &contentURI);

	static int64_t checkSignedMMSPath(string tokenSigned, string contentURIToBeVerified);

	static string getSignedCDN77URL(
		const string &resourceURL,
		// i.e.: 1234456789.rsc.cdn77.org
		const string &filePath,
		// /file/playlist/d.m3u8
		const string &secureToken, long expirationInSeconds, string playerIP = ""
	);
	string getAWSSignedURL(const string &playURL, int expirationInSeconds);
	static string getMedianovaSignedTokenURL(
		const string& playURLProtocol, const string& playURLHostname, const string& uri, const string& secureToken,
		long expirationInSeconds, bool uriEnabled, const optional<string>& playerIP = "");

	unordered_map<string, uint64_t> getExternalDeliveriesRunningHosts();
	void updateExternalDeliveriesBandwidthHosts(const unordered_map<string, uint64_t> &hostsBandwidth);

  private:
	json _configuration;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;

	mutex _externalDeliveriesMutex;
	map<string, shared_ptr<HostBandwidthTracker>> _externalDeliveriesGroups;

	string _keyPairId;
	string _privateKeyPEMPathName;
	string _vodCloudFrontHostName;			   // token as parameter
	string _vodDeliveryCloudFrontHostName;	   // token as parameter
	string _vodDeliveryPathCloudFrontHostName; // url signed

	string _deliveryProtocol;
	string _deliveryHost_authorizationThroughParameter;
	string _deliveryHost_authorizationThroughPath;

	static string getSignedMMSPath(const string &contentURI, time_t expirationTime);
	static time_t getReusableExpirationTime(int ttlInSeconds);
	string getDeliveryHost(
		const shared_ptr<Workspace> &requestWorkspace, const string &playerCountry, const string &playerRegion, const string &defaultDeliveryHost
	);
	shared_ptr<HostBandwidthTracker> getHostBandwidthTracker(int64_t workspaceKey, const string &groupName, const json &hostGroupRoot);
};
#endif
