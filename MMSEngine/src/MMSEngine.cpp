/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MMSEngine.cpp
 * Author: multi
 * 
 * Created on January 30, 2018, 3:00 PM
 */

#include "MMSEngine.h"
#include "FFMpeg.h"
#include "ActiveEncodingsManager.h"


MMSEngine::MMSEngine(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<spdlog::logger> logger
        ) 
{
    _logger             = logger;
    _mmsEngineDBFacade  = mmsEngineDBFacade;
}

MMSEngine::~MMSEngine() {
}

tuple<int64_t,int64_t,string> MMSEngine::registerCustomer(
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

    tuple<int64_t,int64_t,string> customerKeyUserKeyAndConfirmationCode;
    try
    {
        customerKeyUserKeyAndConfirmationCode = _mmsEngineDBFacade->registerCustomer(
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
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->registerCustomer failed";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }

    return customerKeyUserKeyAndConfirmationCode;
}

void MMSEngine::confirmCustomer(string confirmationCode)
{    
    _logger->info(__FILEREF__ + "Received confirmCustomer"
        + ", confirmationCode: " + confirmationCode
    );

    try
    {
        _mmsEngineDBFacade->confirmCustomer(confirmationCode);
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->confirmCustomer failed";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

string MMSEngine::createAPIKey(
        int64_t customerKey,
        int64_t userKey,
        bool adminAPI, 
        bool userAPI,
        chrono::system_clock::time_point apiKeyExpirationDate)
{    
    _logger->info(__FILEREF__ + "Received createAPIKey"
        + ", customerKey: " + to_string(customerKey)
        + ", userKey: " + to_string(userKey)
        + ", adminAPI: " + to_string(adminAPI)
        + ", userAPI: " + to_string(userAPI)
    );

    string apiKey;
    try
    {
        apiKey = _mmsEngineDBFacade->createAPIKey(customerKey, userKey, adminAPI, userAPI, apiKeyExpirationDate);
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->createAPIKey failed";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    return apiKey;
}

tuple<shared_ptr<Customer>,bool,bool> MMSEngine::checkAPIKey (string apiKey)
{
    _logger->info(__FILEREF__ + "Received checkAPIKey"
        + ", apiKey: " + apiKey
    );

    tuple<shared_ptr<Customer>,bool,bool> customerAndFlags;
    try
    {
        customerAndFlags = _mmsEngineDBFacade->checkAPIKey(apiKey);
    }
    catch(APIKeyNotFoundOrExpired e)
    {
        string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->checkAPIKey failed"
                + ", e.what(): " + e.what()
                ;
        _logger->error(errorMessage);
        
        throw e;
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->checkAPIKey failed";
        _logger->error(errorMessage);
        
        throw e;
    }
    
    return customerAndFlags;
}

int64_t MMSEngine::addIngestionJob (
        int64_t customerKey,
        string metadataFileContent,
        MMSEngineDBFacade::IngestionType ingestionType,
        MMSEngineDBFacade::IngestionStatus ingestionStatus)
{
    _logger->info(__FILEREF__ + "Received addIngestionJob"
        + ", customerKey: " + to_string(customerKey)
        + ", metadataFileContent: " + metadataFileContent
        + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType)
        + ", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
    );

    int64_t ingestionJobKey;

    try
    {
        ingestionJobKey = _mmsEngineDBFacade->addIngestionJob(
                customerKey,
                metadataFileContent,
                ingestionType,
                ingestionStatus);
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->addIngestionJob failed";
        _logger->error(errorMessage);
        
        throw e;
    }
    
    return ingestionJobKey;
}

void MMSEngine::addFFMPEGVideoEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,  // "": default Customer family, != "": named customer family
        MMSEngineDBFacade::EncodingTechnology encodingTechnology,
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
        FFMpeg::encodingFileFormatValidation(fileFormat, _logger);
        FFMpeg::encodingVideoCodecValidation(videoCodec, _logger);
        FFMpeg::encodingVideoProfileValidation(videoCodec, videoProfile, _logger);
        FFMpeg::encodingAudioCodecValidation(audioCodec, _logger);

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

        int64_t encodingProfileKey = _mmsEngineDBFacade->addVideoEncodingProfile(
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
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addVideoAudioEncodeProfile failed");
    }
}

void MMSEngine::addImageEncodingProfile(
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

        int64_t encodingProfileKey = _mmsEngineDBFacade->addImageEncodingProfile(
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
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addImageEncodingProfile failed");
    }
}

string MMSEngine::getImageEncodingProfileDetails(
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

string MMSEngine::getVideoEncodingProfileDetails(
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
