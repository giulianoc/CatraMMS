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
	MMSDeliveryAuthorization(
		json configuration, shared_ptr<MMSStorage> mmsStorage, shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<spdlog::logger> logger
	);

	pair<string, string> createDeliveryAuthorization(
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
	);

	string getSignedMMSPath(string contentURI, time_t expirationTime);

	static string getSignedCDN77URL(
		string resourceURL, // i.e.: 1234456789.rsc.cdn77.org
		string filePath,	// /file/playlist/d.m3u8
		string secureToken, long expirationInMinutes, shared_ptr<spdlog::logger> logger
	);

  private:
	json _configuration;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	shared_ptr<spdlog::logger> _logger;

	string _keyPairId;
	string _privateKeyPEMPathName;
	string _vodCloudFrontHostName;

	string _deliveryProtocol;
	string _deliveryHost_authorizationThroughParameter;
	string _deliveryHost_authorizationThroughPath;
};
#endif
