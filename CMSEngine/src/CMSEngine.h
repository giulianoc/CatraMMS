/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMSEngine.h
 * Author: multi
 *
 * Created on January 30, 2018, 3:00 PM
 */

#ifndef CMSEngine_h
#define CMSEngine_h

#include <string>
#include "CMSEngineDBFacade.h"

using namespace std;

class CMSEngine {
    
public:
    CMSEngine(shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<spdlog::logger> logger
            );

    virtual ~CMSEngine();
    
    void addCustomer(
	string customerName,
        string password,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
        CMSEngineDBFacade::CustomerType customerType,
	string deliveryURL,
        bool enabled,
	CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        CMSEngineDBFacade::EncodingPeriod encodingPeriod,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode,
        string userName,
        string userPassword,
        string userEmailAddress,
        chrono::system_clock::time_point userExpirationDate
    );

    void addFFMPEGVideoEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,  // "": default Customer family, != "": named customer family
        CMSEngineDBFacade::EncodingTechnology encodingTechnology,
	string label,
	string fileFormat,
        
        string videoCodec,
        string videoProfile,
        int width,
        int height,
        string videoBitRate,
        string videoMaxRate,
        string videoBufSize,
        int videoFrameRate,
        int videoKeyFrameIntervalInSeconds,

        string audioCodec,
        string audioBitRate
    );

private:
    shared_ptr<CMSEngineDBFacade> _cmsEngineDBFacade;
    shared_ptr<spdlog::logger> _logger;
    
    string getEncodingProfileDetails(
        string fileFormat,

        string videoCodec,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoBitRate,
        string videoMaxRate,
        string videoBufSize,
        int videoFrameRate,
        int videoKeyFrameIntervalInSeconds,

        string audioCodec,
        string audioBitRate
    );
};

#endif

