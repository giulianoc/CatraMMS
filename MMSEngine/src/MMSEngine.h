/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MMSEngine.h
 * Author: multi
 *
 * Created on January 30, 2018, 3:00 PM
 */

#ifndef MMSEngine_h
#define MMSEngine_h

#include <string>
#include "MMSEngineDBFacade.h"

using namespace std;

class MMSEngine {
    
public:
    MMSEngine(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<spdlog::logger> logger
            );

    virtual ~MMSEngine();
    
    tuple<int64_t,int64_t,string> registerCustomer(
	string customerName,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
        MMSEngineDBFacade::CustomerType customerType,
	string deliveryURL,
	MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        MMSEngineDBFacade::EncodingPeriod encodingPeriod,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode,
        string userName,
        string userPassword,
        string userEmailAddress,
        chrono::system_clock::time_point userExpirationDate
    );

    void confirmCustomer(string confirmationCode);
    
    string createAPIKey(
        int64_t customerKey,
        int64_t userKey,
        bool adminAPI, 
        bool userAPI,
        chrono::system_clock::time_point apiKeyExpirationDate);

    tuple<shared_ptr<Customer>,bool,bool> checkAPIKey (string apiKey);
    
    int64_t addIngestionJob (
	int64_t customerKey,
        string metadataFileContent,
        MMSEngineDBFacade::IngestionType ingestionType,
        MMSEngineDBFacade::IngestionStatus ingestionStatus);

    void addFFMPEGVideoEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,  // "": default Customer family, != "": named customer family
        MMSEngineDBFacade::EncodingTechnology encodingTechnology,
	string label,
	string fileFormat,
        
        string videoCodec,
        string videoProfile,
        int width,
        int height,
        string videoBitRate,
        bool twoPasses,
        string videoMaxRate,
        string videoBufSize,
        int videoFrameRate,
        int videoKeyFrameIntervalInSeconds,

        string audioCodec,
        string audioBitRate
    );

    void addImageEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,  // "": default Customer family, != "": named customer family
	string label,

	string format,         // JPG, GIF, PNG
        
        int width,
        int height,
        bool aspectRatio,   // Aspect is true the proportion are NOT maintained
                            // if Aspect is false the proportion are maintained, the width is fixed and the height will be calculated
        string sInterlaceType    // NoInterlace, LineInterlace, PlaneInterlace, PartitionInterlace
    );

private:
    shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
    shared_ptr<spdlog::logger> _logger;
    
    string getVideoEncodingProfileDetails(
        string fileFormat,

        string videoCodec,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoBitRate,
        bool twoPasses,
        string videoMaxRate,
        string videoBufSize,
        int videoFrameRate,
        int videoKeyFrameIntervalInSeconds,

        string audioCodec,
        string audioBitRate
    );
    
    string getImageEncodingProfileDetails(
        string format,

        int width,
        int height,
        bool aspectRatio,
        string sInterlaceType
    );
};

#endif

