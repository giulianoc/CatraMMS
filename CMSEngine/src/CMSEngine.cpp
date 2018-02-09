/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMSEngine.cpp
 * Author: multi
 * 
 * Created on January 30, 2018, 3:00 PM
 */

#include "CMSEngine.h"
#include "EncoderVideoAudioProxy.h"


CMSEngine::CMSEngine(shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<spdlog::logger> logger
        ) 
{
    _logger             = logger;
    _cmsEngineDBFacade  = cmsEngineDBFacade;
}

CMSEngine::~CMSEngine() {
}

void CMSEngine::addCustomer(
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
)
{    
    _logger->info(string("Received addCustomer")
        + ", customerName: " + customerName
        + ", password: " + password
        + ", street: " + street
        + ", city: " + city
        + ", state: " + state
        + ", zip: " + zip
        + ", phone: " + phone
        + ", countryCode: " + countryCode
        + ", customerType: " + to_string(static_cast<int>(customerType))
        + ", deliveryURL: " + deliveryURL
        + ", enabled: " + to_string(enabled)
        + ", maxEncodingPriority: " + to_string(static_cast<int>(maxEncodingPriority))
        + ", encodingPeriod: " + to_string(static_cast<int>(encodingPeriod))
        + ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
        + ", maxStorageInGB: " + to_string(maxStorageInGB)
        + ", languageCode: " + languageCode
        + ", userName: " + userName
        + ", userPassword: " + userPassword
        + ", userEmailAddress: " + userEmailAddress
        // ", userExpirationDate: " + userExpirationDate
    );

    string customerDirectoryName;

    customerDirectoryName.resize(customerName.size());

    transform(
        customerName.begin(), 
        customerName.end(), 
        customerDirectoryName.begin(), 
        [](unsigned char c){
            if (isalpha(c)) 
                return c; 
            else 
                return (unsigned char) '_'; } 
    );

    try
    {
        int64_t customerKey = _cmsEngineDBFacade->addCustomer(
            customerName, 
            customerDirectoryName,
            street,
            city,
            state,
            zip,
            phone,
            countryCode,
            customerType,
            deliveryURL,
            enabled,
            maxEncodingPriority,
            encodingPeriod,
            maxIngestionsNumber,
            maxStorageInGB,
            languageCode,
            userName,
            userPassword,
            userEmailAddress,
            userExpirationDate);
    }
    catch(...)
    {
        _logger->error("_cmsEngineDBFacade->addCustomer failed");
    }

}

void CMSEngine::addFFMPEGVideoEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,  // "": default Customer family, != "": named customer family
        CMSEngineDBFacade::EncodingTechnology encodingTechnology,
	string label,

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
)
{    
    _logger->info(string("Received addEncodingProfile")
        + ", customer->_customerKey: " + to_string(customer->_customerKey)
        + ", customer->_name: " + customer->_name
        + ", encodingProfileSet: " + encodingProfileSet
        + ", encodingTechnology: " + to_string(static_cast<int>(encodingTechnology))
        + ", label: " + label
        + ", fileFormat: " + fileFormat

        + ", videoCodec: " + videoCodec
        + ", videoProfile: " + videoProfile
        + ", videoWidth: " + to_string(videoWidth)
        + ", videoHeight: " + to_string(videoHeight)
        + ", videoBitRate: " + videoBitRate
        + ", videoMaxRate: " + videoMaxRate
        + ", videoBufSize: " + videoBufSize
        + ", videoFrameRate: " + to_string(videoFrameRate)
        + ", videoKeyFrameIntervalInSeconds: " + to_string(videoKeyFrameIntervalInSeconds)

        + ", audioCodec: " + audioCodec
        + ", audioBitRate: " + audioBitRate
    );

    try
    {
        EncoderVideoAudioProxy::encodingFileFormatValidation(fileFormat);
        EncoderVideoAudioProxy::ffmpeg_encodingVideoCodecValidation(videoCodec);
        EncoderVideoAudioProxy::ffmpeg_encodingVideoProfileValidation(videoCodec, videoProfile);
        EncoderVideoAudioProxy::ffmpeg_encodingAudioCodecValidation(audioCodec);

        string details = getEncodingProfileDetails(
            fileFormat,
                
            videoCodec,
            videoProfile,
            videoWidth,
            videoHeight,
            videoBitRate,
            videoMaxRate,
            videoBufSize,
            videoFrameRate,
            videoKeyFrameIntervalInSeconds,

            audioCodec,
            audioBitRate
        );

        int64_t encodingProfileKey = _cmsEngineDBFacade->addVideoEncodeProfile(
            customer,
            encodingProfileSet,
            encodingTechnology,
            details,
            label,
            videoWidth,
            videoHeight,
            videoCodec);        
    }
    catch(...)
    {
        _logger->error("_cmsEngineDBFacade->addVideoEncodeProfile failed");
    }
}

string CMSEngine::getEncodingProfileDetails(
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
)
{
    string details = string("")
            + "{"
            + "\"fileFormat\": \"" + fileFormat + "\", "         // mandatory, 3gp, webm or segment
    ;
    
    details.append(string("")
            + "\"video\": { "
            +    "\"codec\": \"" + videoCodec + "\", "     // mandatory, libx264 or libvpx
            +    "\"profile\": \"" + videoProfile + "\", "      // optional, if libx264 -> high or baseline or main. if libvpx -> best or good
            +    "\"width\": " + to_string(videoWidth) + ", " // mandatory
            +    "\"height\": " + to_string(videoHeight) + ", " // mandatory
            +    "\"bitRate\": \"" + videoBitRate + "\", "      // mandatory
            +    "\"maxRate\": \"" + videoMaxRate + "\", "      // optional
            +    "\"bufSize\": \"" + videoBufSize + "\", "     // optional
            +    "\"frameRate\": " + to_string(videoFrameRate) + ", "      // optional
            +    "\"keyFrameIntervalInSeconds\": " + to_string(videoKeyFrameIntervalInSeconds) + " "   // optional and only if framerate is present
            + "}"
    );
    
    if (audioCodec != "")
    {
        details.append(string("")
            + ", "
            + "\"audio\": { "
            +    "\"codec\": \"" + audioCodec + "\", "  // mandatory, libaacplus, libvo_aacenc or libvorbis
            +    "\"bitRate\": \"" + audioBitRate + "\" "      // mandatory
            + "}"
        );
    }
    
    details.append(string("")
        + "}"
    );

    return details;
}
