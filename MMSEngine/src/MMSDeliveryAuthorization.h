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
#include "MMSStorage.h"
#include "spdlog/spdlog.h"
// #include <fstream>

using namespace std;

class MMSDeliveryAuthorization
{

  public:
	MMSDeliveryAuthorization(json configuration, shared_ptr<MMSStorage> mmsStorage, shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade);

	pair<string, string> createDeliveryAuthorization(
		int64_t userKey, shared_ptr<Workspace> requestWorkspace, string playerIP, int64_t mediaItemKey, string uniqueName, int64_t encodingProfileKey,
		string encodingProfileLabel, int64_t physicalPathKey, int64_t ingestionJobKey, int64_t deliveryCode, int ttlInSeconds, int maxRetries,
		bool playerIPToBeAuthorized, bool save, string deliveryType, bool warningIfMissingMediaItemKey, bool filteredByStatistic, string userId
	);

	string checkDeliveryAuthorizationThroughParameter(string contentURI, string tokenParameter);

	int64_t checkDeliveryAuthorizationThroughPath(string contentURI);

	string checkDeliveryAuthorizationOfAManifest(bool secondaryManifest, string token, string cookie, string contentURI);

	int64_t checkSignedMMSPath(string tokenSigned, string contentURIToBeVerified);

	static string getSignedCDN77URL(
		string resourceURL, // i.e.: 1234456789.rsc.cdn77.org
		string filePath,	// /file/playlist/d.m3u8
		string secureToken, long expirationInSeconds, string clientIP = ""
	);
	string getAWSSignedURL(string playURL, int expirationInSeconds);

  private:
	json _configuration;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;

	string _keyPairId;
	string _privateKeyPEMPathName;
	string _vodCloudFrontHostName;			   // token as parameter
	string _vodDeliveryCloudFrontHostName;	   // token as parameter
	string _vodDeliveryPathCloudFrontHostName; // url signed

	string _deliveryProtocol;
	string _deliveryHost_authorizationThroughParameter;
	string _deliveryHost_authorizationThroughPath;

	string getSignedMMSPath(string contentURI, time_t expirationTime);
	time_t getReusableExpirationTime(int ttlInSeconds);
};
#endif
