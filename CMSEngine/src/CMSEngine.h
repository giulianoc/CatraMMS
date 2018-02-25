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
    
    tuple<int64_t,int64_t,string> registerCustomer(
	string customerName,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
        CMSEngineDBFacade::CustomerType customerType,
	string deliveryURL,
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
        string fileNameWithIngestionJobKeyPlaceholder,
        string ingestionJobKeyPlaceHolder,
        string metadataFileContent,
        CMSEngineDBFacade::IngestionType ingestionType,
        CMSEngineDBFacade::IngestionStatus ingestionStatus);

    void removeIngestionJob (int64_t ingestionJobKey);

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
    shared_ptr<CMSEngineDBFacade> _cmsEngineDBFacade;
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

