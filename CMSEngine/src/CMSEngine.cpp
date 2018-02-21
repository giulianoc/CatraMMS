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
#include "ActiveEncodingsManager.h"


CMSEngine::CMSEngine(shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<spdlog::logger> logger
        ) 
{
    _logger             = logger;
    _cmsEngineDBFacade  = cmsEngineDBFacade;
}

CMSEngine::~CMSEngine() {
}

pair<int64_t,string> CMSEngine::registerCustomer(
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
)
{    
    _logger->info(__FILEREF__ + "Received addCustomer"
        + ", customerName: " + customerName
        + ", street: " + street
        + ", city: " + city
        + ", state: " + state
        + ", zip: " + zip
        + ", phone: " + phone
        + ", countryCode: " + countryCode
        + ", customerType: " + to_string(static_cast<int>(customerType))
        + ", deliveryURL: " + deliveryURL
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

    pair<int64_t,string> customerKeyAndConfirmationCode;
    try
    {
        customerKeyAndConfirmationCode = _cmsEngineDBFacade->registerCustomer(
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
        string errorMessage = __FILEREF__ + "_cmsEngineDBFacade->registerCustomer failed";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }

    return customerKeyAndConfirmationCode;
}

void CMSEngine::confirmCustomer(string confirmationCode)
{    
    _logger->info(__FILEREF__ + "Received confirmCustomer"
        + ", confirmationCode: " + confirmationCode
    );

    try
    {
        _cmsEngineDBFacade->confirmCustomer(confirmationCode);
    }
    catch(...)
    {
        string errorMessage = __FILEREF__ + "_cmsEngineDBFacade->confirmCustomer failed";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
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
        bool twoPasses,
        string videoMaxRate,
        string videoBufSize,
        int videoFrameRate,
        int videoKeyFrameIntervalInSeconds,

        string audioCodec,
        string audioBitRate
)
{    
    _logger->info(__FILEREF__ + "Received addFFMPEGVideoEncodingProfile"
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
        + ", twoPasses: " + to_string(twoPasses)
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

        string details = getVideoEncodingProfileDetails(
            fileFormat,
                
            videoCodec,
            videoProfile,
            videoWidth,
            videoHeight,
            videoBitRate,
            twoPasses,
            videoMaxRate,
            videoBufSize,
            videoFrameRate,
            videoKeyFrameIntervalInSeconds,

            audioCodec,
            audioBitRate
        );

        int64_t encodingProfileKey = _cmsEngineDBFacade->addVideoEncodingProfile(
            customer,
            encodingProfileSet,
            encodingTechnology,
            details,
            label,
            videoWidth,
            videoHeight,
            videoCodec,
            audioCodec);      
        
        _logger->info(__FILEREF__ + "Created the video/audio encoding profile"
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
        );
    }
    catch(...)
    {
        _logger->error(__FILEREF__ + "_cmsEngineDBFacade->addVideoAudioEncodeProfile failed");
    }
}

void CMSEngine::addImageEncodingProfile(
    shared_ptr<Customer> customer,
    string encodingProfileSet,  // "": default Customer family, != "": named customer family
    string label,

    string format,         // JPG, GIF, PNG

    int width,
    int height,
    bool aspectRatio,   // Aspect is true the proportion are NOT maintained
                        // if Aspect is false the proportion are maintained, the width is fixed and the height will be calculated
    string sInterlaceType    // NoInterlace, LineInterlace, PlaneInterlace, PartitionInterlace
)
{
    _logger->info(__FILEREF__ + "Received addImageEncodingProfile"
        + ", customer->_customerKey: " + to_string(customer->_customerKey)
        + ", customer->_name: " + customer->_name
        + ", encodingProfileSet: " + encodingProfileSet
        + ", label: " + label
        + ", format: " + format

        + ", width: " + to_string(width)
        + ", height: " + to_string(height)
        + ", aspectRatio: " + to_string(aspectRatio)
        + ", sInterlaceType: " + sInterlaceType
    );

    try
    {
        ActiveEncodingsManager::encodingImageFormatValidation(format);
        ActiveEncodingsManager::encodingImageInterlaceTypeValidation(sInterlaceType);


        string details = getImageEncodingProfileDetails(
            format,
                
            width,
            height,
            aspectRatio,
            sInterlaceType
        );

        int64_t encodingProfileKey = _cmsEngineDBFacade->addImageEncodingProfile(
            customer,
            encodingProfileSet,
            details,
            label,
            width,
            height);      
        
        _logger->info(__FILEREF__ + "Created the image encoding profile"
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
        );
    }
    catch(...)
    {
        _logger->error(__FILEREF__ + "_cmsEngineDBFacade->addImageEncodingProfile failed");
    }
}

string CMSEngine::getImageEncodingProfileDetails(
    string format,

    int width,
    int height,
    bool aspectRatio,
    string sInterlaceType
)
{
    string details;
    
    try
    {
        details = string("")
                + "{"
                +    "\"format\": \"" + format + "\", "         // mandatory, JPG, GIF or PNG
                +    "\"width\": " + to_string(width) + ", " // mandatory
                +    "\"height\": " + to_string(height) + ", " // mandatory
                +    "\"aspectRatio\": " + (aspectRatio ? "true" : "false") + ", "      // mandatory
                +    "\"interlaceType\": \"" + sInterlaceType + "\" "         // mandatory
                + "}"
        ;
    }
    catch(...)
    {
        string errorMessage = __FILEREF__ + "getVideoAudioEncodingProfileDetails failed";
        _logger->error(errorMessage);
                
        throw runtime_error(errorMessage);
    }

    return details;
}

string CMSEngine::getVideoEncodingProfileDetails(
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
)
{
    string details;
    
    try
    {
        details = string("")
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
                +    "\"twoPasses\": " + (twoPasses ? "true" : "false") + ", "      // mandatory
                +    "\"maxRate\": \"" + videoMaxRate + "\", "      // optional
                +    "\"bufSize\": \"" + videoBufSize + "\", "     // optional
                +    "\"frameRate\": " + to_string(videoFrameRate) + ", "      // optional
                +    "\"keyFrameIntervalInSeconds\": " + to_string(videoKeyFrameIntervalInSeconds) + " "   // optional and only if framerate is present
                + "}"
        );

        details.append(string("")
            + ", "
            + "\"audio\": { "
            +    "\"codec\": \"" + audioCodec + "\", "  // mandatory, libaacplus, libvo_aacenc or libvorbis
            +    "\"bitRate\": \"" + audioBitRate + "\" "      // mandatory
            + "}"
        );

        details.append(string("")
            + "}"
        );
    }
    catch(...)
    {
        string errorMessage = __FILEREF__ + "getVideoEncodingProfileDetails failed";
        _logger->error(errorMessage);
                
        throw runtime_error(errorMessage);
    }

    return details;
}
