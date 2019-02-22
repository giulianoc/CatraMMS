/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MMSEngineDBFacade.h
 * Author: giuliano
 *
 * Created on January 27, 2018, 9:38 AM
 */

#ifndef MMSEngineDBFacade_h
#define MMSEngineDBFacade_h

#include <string>
#include <memory>
#include <vector>
#include "spdlog/spdlog.h"
#include "Workspace.h"
#include "json/json.h"
#include "MySQLConnection.h"



#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

using namespace std;

struct LoginFailed: public exception {    
    char const* what() const throw() 
    {
        return "email and/or password are wrong";
    }; 
};

struct APIKeyNotFoundOrExpired: public exception {    
    char const* what() const throw() 
    {
        return "APIKey was not found or it is expired";
    }; 
};

struct MediaItemKeyNotFound: public exception { 
    
    string _errorMessage;
    
    MediaItemKeyNotFound(string errorMessage)
    {
        _errorMessage = errorMessage;
    }
    
    char const* what() const throw() 
    {
        return _errorMessage.c_str();
    }; 
};

class MMSEngineDBFacade {

public:
    enum class ContentType {
        Video		= 0,
	Audio		= 1,
	Image		= 2
//	Application	= 3,
//	Ringtone	= 4,
//	Playlist	= 5,
//	Live		= 6
    };
    static const char* toString(const ContentType& contentType)
    {
        switch (contentType)
        {
            case ContentType::Video:
                return "Video";
            case ContentType::Audio:
                return "Audio";
            case ContentType::Image:
                return "Image";
            default:
                throw runtime_error(string("Wrong ContentType"));
        }
    }
    static ContentType toContentType(const string& contentType)
    {
        string lowerCase;
        lowerCase.resize(contentType.size());
        transform(contentType.begin(), contentType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "video")
            return ContentType::Video;
        else if (lowerCase == "audio")
            return ContentType::Audio;
        else if (lowerCase == "image")
            return ContentType::Image;
        else
            throw runtime_error(string("Wrong ContentType (correct value: video, audio or image")
                    + ", current contentType: " + contentType
                    );
    }

    enum class EncodingPriority {
        Low                 = 0,
        Medium              = 1,
        High                = 2
    };
    static const char* toString(const EncodingPriority& priority)
    {
        switch (priority)
        {
            case EncodingPriority::Low:
                return "Low";
            case EncodingPriority::Medium:
                return "Medium";
            case EncodingPriority::High:
                return "High";
            default:
            throw runtime_error(string("Wrong EncodingPriority"));
        }
    }
    static EncodingPriority toEncodingPriority(const string& priority)
    {
        string lowerCase;
        lowerCase.resize(priority.size());
        transform(priority.begin(), priority.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "low")
            return EncodingPriority::Low;
        else if (lowerCase == "medium")
            return EncodingPriority::Medium;
        else if (lowerCase == "high")
            return EncodingPriority::High;
        else
            throw runtime_error(string("Wrong EncodingPriority")
                    + ", priority: " + priority
                    );
    }

    enum class EncodingStatus {
        ToBeProcessed           = 0,
        Processing              = 1,
        End_Success				= 2,
        End_Failed              = 3,
        End_KilledByUser		= 4
    };
    static const char* toString(const EncodingStatus& encodingStatus)
    {
        switch (encodingStatus)
        {
            case EncodingStatus::ToBeProcessed:
                return "ToBeProcessed";
            case EncodingStatus::Processing:
                return "Processing";
            case EncodingStatus::End_Success:
                return "End_Success";
            case EncodingStatus::End_Failed:
                return "End_Failed";
            case EncodingStatus::End_KilledByUser:
                return "End_KilledByUser";
            default:
            throw runtime_error(string("Wrong EncodingStatus"));
        }
    }
    static EncodingStatus toEncodingStatus(const string& encodingStatus)
    {
        string lowerCase;
        lowerCase.resize(encodingStatus.size());
        transform(encodingStatus.begin(), encodingStatus.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "tobeprocessed")
            return EncodingStatus::ToBeProcessed;
        else if (lowerCase == "processing")
            return EncodingStatus::Processing;
        else if (lowerCase == "end_success")
            return EncodingStatus::End_Success;
        else if (lowerCase == "end_failed")
            return EncodingStatus::End_Failed;
        else if (lowerCase == "end_killedbyuser")
            return EncodingStatus::End_KilledByUser;
        else
            throw runtime_error(string("Wrong EncodingStatus")
                    + ", encodingStatus: " + encodingStatus
                    );
    }
    
    enum class EncodingType {
        EncodeVideoAudio    = 0,
        EncodeImage         = 1,
        OverlayImageOnVideo = 2,
        OverlayTextOnVideo  = 3,
        GenerateFrames      = 4,
        SlideShow           = 5,
        FaceRecognition		= 6,
        FaceIdentification	= 7,
		LiveRecorder		= 8
    };
    static const char* toString(const EncodingType& encodingType)
    {
        switch (encodingType)
        {
            case EncodingType::EncodeVideoAudio:
                return "EncodeVideoAudio";
            case EncodingType::EncodeImage:
                return "EncodeImage";
            case EncodingType::OverlayImageOnVideo:
                return "OverlayImageOnVideo";
            case EncodingType::OverlayTextOnVideo:
                return "OverlayTextOnVideo";
            case EncodingType::GenerateFrames:
                return "GenerateFrames";
            case EncodingType::SlideShow:
                return "SlideShow";
            case EncodingType::FaceRecognition:
                return "FaceRecognition";
            case EncodingType::FaceIdentification:
                return "FaceIdentification";
            case EncodingType::LiveRecorder:
                return "LiveRecorder";
            default:
				throw runtime_error(string("Wrong EncodingType"));
        }
    }
    static EncodingType toEncodingType(const string& encodingType)
    {
        string lowerCase;
        lowerCase.resize(encodingType.size());
        transform(encodingType.begin(), encodingType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "encodevideoaudio")
            return EncodingType::EncodeVideoAudio;
        if (lowerCase == "encodeimage")
            return EncodingType::EncodeImage;
        else if (lowerCase == "overlayimageonvideo")
            return EncodingType::OverlayImageOnVideo;
        else if (lowerCase == "overlaytextonvideo")
            return EncodingType::OverlayTextOnVideo;
        else if (lowerCase == "generateframes")
            return EncodingType::GenerateFrames;
        else if (lowerCase == "slideshow")
            return EncodingType::SlideShow;
        else if (lowerCase == "facerecognition")
            return EncodingType::FaceRecognition;
        else if (lowerCase == "faceidentification")
            return EncodingType::FaceIdentification;
        else if (lowerCase == "liverecorder")
            return EncodingType::LiveRecorder;
        else
            throw runtime_error(string("Wrong EncodingType")
                    + ", encodingType: " + encodingType
                    );
    }

    enum class EncodingError {
        NoError,
        PunctualError,
        MaxCapacityReached,
        ErrorBeforeEncoding,
		KilledByUser
    };
    
    enum class EncodingTechnology {
        Image      = 0,    // (Download),
        MP4,                // (Streaming+Download),
        MPEG2_TS,           // (IPhone Streaming),
        WEBM,               // (VP8 and Vorbis)
        WindowsMedia,
        Adobe
    };
    
    enum class EncodingPeriod {
        Daily		= 0,
	Weekly		= 1,
	Monthly		= 2,
        Yearly          = 3
    };
    static const char* toString(const EncodingPeriod& encodingPeriod)
    {
        switch (encodingPeriod)
        {
            case EncodingPeriod::Daily:
                return "Daily";
            case EncodingPeriod::Weekly:
                return "Weekly";
            case EncodingPeriod::Monthly:
                return "Monthly";
            case EncodingPeriod::Yearly:
                return "Yearly";
            default:
            throw runtime_error(string("Wrong EncodingPeriod"));
        }
    }
    static EncodingPeriod toEncodingPeriod(const string& encodingPeriod)
    {
        string lowerCase;
        lowerCase.resize(encodingPeriod.size());
        transform(encodingPeriod.begin(), encodingPeriod.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "daily")
            return EncodingPeriod::Daily;
        else if (lowerCase == "weekly")
            return EncodingPeriod::Weekly;
        else if (lowerCase == "monthly")
            return EncodingPeriod::Monthly;
        else if (lowerCase == "yearly")
            return EncodingPeriod::Yearly;
        else
            throw runtime_error(string("Wrong EncodingPeriod")
                    + ", encodingPeriod: " + encodingPeriod
                    );
    }

    struct EncodingItem
    {
        long long                               _encodingJobKey;
        long long                               _ingestionJobKey;
        EncodingPriority                        _encodingPriority;
        EncodingType                            _encodingType;
        string                                  _encodingParameters;
        // MMS_EncodingJob -> parameters
        Json::Value                             _parametersRoot;

        shared_ptr<Workspace>                   _workspace;

        struct EncodeData {
            unsigned long                           _mmsPartitionNumber;
            string                                  _fileName;
            string                                  _relativePath;
            long long                               _mediaItemKey;
            int64_t                                 _durationInMilliSeconds;
            ContentType                             _contentType;
            MMSEngineDBFacade::EncodingTechnology   _encodingProfileTechnology;
            string                                  _jsonProfile;
        };

        struct OverlayImageOnVideoData {
            unsigned long                           _mmsVideoPartitionNumber;
            string                                  _videoFileName;
            string                                  _videoRelativePath;
            int64_t                                 _videoDurationInMilliSeconds;

            unsigned long                           _mmsImagePartitionNumber;
            string                                  _imageFileName;
            string                                  _imageRelativePath;

            // MMS_IngestionJob -> metaDataContent (you need it when the encoding generated a content to be ingested)
            Json::Value                             _overlayParametersRoot;
        };
        
        struct OverlayTextOnVideoData {
            unsigned long                           _mmsVideoPartitionNumber;
            string                                  _videoFileName;
            string                                  _videoRelativePath;
            int64_t                                 _videoDurationInMilliSeconds;

            // MMS_IngestionJob -> metaDataContent (you need it when the encoding generated a content to be ingested)
            Json::Value                             _overlayTextParametersRoot;
        };

        struct GenerateFramesData {
            unsigned long                           _mmsVideoPartitionNumber;
            string                                  _videoFileName;
            string                                  _videoRelativePath;
            int64_t                                 _videoDurationInMilliSeconds;

            // MMS_IngestionJob -> metaDataContent (you need it when the encoding generated a content to be ingested)
            Json::Value                             _generateFramesParametersRoot;
        };

        struct SlideShowData {
            // MMS_IngestionJob -> metaDataContent (you need it when the encoding generated a content to be ingested)
            Json::Value                             _slideShowParametersRoot;
        };

        struct FaceRecognitionData {
            // MMS_IngestionJob -> metaDataContent (you need it when the encoding generated a content to be ingested)
            Json::Value                             _faceRecognitionParametersRoot;
        };

        struct FaceIdentificationData {
            // MMS_IngestionJob -> metaDataContent (you need it when the encoding generated a content to be ingested)
            Json::Value                             _faceIdentificationParametersRoot;
        };

		struct LiveRecorderData {
			// MMS_IngestionJob -> metaDataContent (you need it when the encoding generated a content to be ingested)
			Json::Value								_liveRecorderParametersRoot;
		};

        shared_ptr<EncodeData>                      _encodeData;
        shared_ptr<OverlayImageOnVideoData>         _overlayImageOnVideoData;
        shared_ptr<OverlayTextOnVideoData>          _overlayTextOnVideoData;
        shared_ptr<GenerateFramesData>              _generateFramesData;
        shared_ptr<SlideShowData>                   _slideShowData;
        shared_ptr<FaceRecognitionData>				_faceRecognitionData;
        shared_ptr<FaceIdentificationData>			_faceIdentificationData;
		shared_ptr<LiveRecorderData>				_liveRecorderData;
    } ;

    enum class WorkspaceType {
        LiveSessionOnly         = 0,
        IngestionAndDelivery    = 1,
        EncodingOnly            = 2
    };

    enum class IngestionType {
        Unknown                 = 0,    // in case json was not able to be parsed
        AddContent              = 1,
        RemoveContent           = 2,
        Encode                  = 3,
        Frame                   = 4,
        PeriodicalFrames        = 5,
        IFrames                 = 6,
        MotionJPEGByPeriodicalFrames        = 7,
        MotionJPEGByIFrames     = 8,
        Slideshow               = 9,
        ConcatDemuxer           = 10,
        Cut                     = 11,
        OverlayImageOnVideo     = 12,
        OverlayTextOnVideo      = 13,
        FTPDelivery             = 14,
        HTTPCallback            = 15,
        LocalCopy               = 16,
        ExtractTracks           = 17,
        PostOnFacebook          = 18,
        PostOnYouTube           = 19,
        FaceRecognition         = 20,
        FaceIdentification		= 21,
        LiveRecorder			= 22,
        ChangeFileFormat		= 23,
        EmailNotification       = 30,
        ContentUpdate           = 50,
        ContentRemove           = 60
    };
    static const char* toString(const IngestionType& ingestionType)
    {
        switch (ingestionType)
        {
            case IngestionType::Unknown:
                return "Unknown";
            case IngestionType::AddContent:
                return "Add-Content";
            case IngestionType::RemoveContent:
                return "Remove-Content";
            case IngestionType::Encode:
                return "Encode";
            case IngestionType::Frame:
                return "Frame";
            case IngestionType::PeriodicalFrames:
                return "Periodical-Frames";
            case IngestionType::IFrames:
                return "I-Frames";
            case IngestionType::MotionJPEGByPeriodicalFrames:
                return "Motion-JPEG-by-Periodical-Frames";
            case IngestionType::MotionJPEGByIFrames:
                return "Motion-JPEG-by-I-Frames";
            case IngestionType::Slideshow:
                return "Slideshow";
            case IngestionType::ConcatDemuxer:
                return "Concat-Demuxer";
            case IngestionType::Cut:
                return "Cut";
            case IngestionType::OverlayImageOnVideo:
                return "Overlay-Image-On-Video";
            case IngestionType::OverlayTextOnVideo:
                return "Overlay-Text-On-Video";
            case IngestionType::FTPDelivery:
                return "FTP-Delivery";
            case IngestionType::HTTPCallback:
                return "HTTP-Callback";
            case IngestionType::LocalCopy:
                return "Local-Copy";
            case IngestionType::ExtractTracks:
                return "Extract-Tracks";
            case IngestionType::PostOnFacebook:
                return "Post-On-Facebook";
            case IngestionType::PostOnYouTube:
                return "Post-On-YouTube";
            case IngestionType::FaceRecognition:
                return "Face-Recognition";
            case IngestionType::FaceIdentification:
                return "Face-Identification";
			case IngestionType::LiveRecorder:
				return "Live-Recorder";
			case IngestionType::ChangeFileFormat:
				return "Change-File-Format";
                
            case IngestionType::EmailNotification:
                return "Email-Notification";
            case IngestionType::ContentUpdate:
                return "ContentUpdate";
            case IngestionType::ContentRemove:
                return "ContentRemove";
            default:
            throw runtime_error(string("Wrong IngestionType"));
        }
    }
    static IngestionType toIngestionType(const string& ingestionType)
    {
        string lowerCase;
        lowerCase.resize(ingestionType.size());
        transform(ingestionType.begin(), ingestionType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "add-content")
            return IngestionType::AddContent;
        else if (lowerCase == "remove-content")
            return IngestionType::RemoveContent;
        else if (lowerCase == "encode")
            return IngestionType::Encode;
        else if (lowerCase == "frame")
            return IngestionType::Frame;
        else if (lowerCase == "periodical-frames")
            return IngestionType::PeriodicalFrames;
        else if (lowerCase == "i-frames")
            return IngestionType::IFrames;
        else if (lowerCase == "motion-jpeg-by-periodical-frames")
            return IngestionType::MotionJPEGByPeriodicalFrames;
        else if (lowerCase == "motion-jpeg-by-i-frames")
            return IngestionType::MotionJPEGByIFrames;
        else if (lowerCase == "slideshow")
            return IngestionType::Slideshow;
        else if (lowerCase == "concat-demuxer")
            return IngestionType::ConcatDemuxer;
        else if (lowerCase == "cut")
            return IngestionType::Cut;
        else if (lowerCase == "overlay-image-on-video")
            return IngestionType::OverlayImageOnVideo;
        else if (lowerCase == "overlay-text-on-video")
            return IngestionType::OverlayTextOnVideo;
        else if (lowerCase == "ftp-delivery")
            return IngestionType::FTPDelivery;
        else if (lowerCase == "http-callback")
            return IngestionType::HTTPCallback;
        else if (lowerCase == "local-copy")
            return IngestionType::LocalCopy;
        else if (lowerCase == "extract-tracks")
            return IngestionType::ExtractTracks;
        else if (lowerCase == "post-on-facebook")
            return IngestionType::PostOnFacebook;
        else if (lowerCase == "post-on-youtube")
            return IngestionType::PostOnYouTube;
        else if (lowerCase == "face-recognition")
            return IngestionType::FaceRecognition;
        else if (lowerCase == "face-identification")
            return IngestionType::FaceIdentification;
        else if (lowerCase == "live-recorder")
            return IngestionType::LiveRecorder;
        else if (lowerCase == "change-file-format")
            return IngestionType::ChangeFileFormat;

        else if (lowerCase == "email-notification")
            return IngestionType::EmailNotification;
        else if (lowerCase == "contentupdate")
            return IngestionType::ContentUpdate;
        else if (lowerCase == "contentremove")
            return IngestionType::ContentRemove;
        else
            throw runtime_error(string("Wrong IngestionType")
                    + ", ingestionType: " + ingestionType
                    );
    }

    enum class IngestionStatus {
        Start_TaskQueued,    
        
        SourceDownloadingInProgress,
        SourceMovingInProgress,
        SourceCopingInProgress,
        SourceUploadingInProgress,
        EncodingQueued,

        End_DwlUplOrEncCancelledByUser,   

        End_ValidationMetadataFailed,   

        End_ValidationMediaSourceFailed,   

        End_WorkspaceReachedHisMaxIngestionNumber,
        
        End_IngestionFailure,                    // nothing done
        
        End_NotToBeExecuted,    // because of dependencies    
        
        End_TaskSuccess
    };
    static const char* toString(const IngestionStatus& ingestionStatus)
    {
        switch (ingestionStatus)
        {
            case IngestionStatus::Start_TaskQueued:
                return "Start_TaskQueued";
            case IngestionStatus::SourceDownloadingInProgress:
                return "SourceDownloadingInProgress";
            case IngestionStatus::SourceMovingInProgress:
                return "SourceMovingInProgress";
            case IngestionStatus::SourceCopingInProgress:
                return "SourceCopingInProgress";
            case IngestionStatus::SourceUploadingInProgress:
                return "SourceUploadingInProgress";
            case IngestionStatus::EncodingQueued:
                return "EncodingQueued";
            case IngestionStatus::End_DwlUplOrEncCancelledByUser:
                return "End_DwlUplOrEncCancelledByUser";
            case IngestionStatus::End_ValidationMetadataFailed:
                return "End_ValidationMetadataFailed";
            case IngestionStatus::End_ValidationMediaSourceFailed:
                return "End_ValidationMediaSourceFailed";
            case IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber:
                return "End_WorkspaceReachedHisMaxIngestionNumber";
            case IngestionStatus::End_IngestionFailure:
                return "End_IngestionFailure";
            case IngestionStatus::End_NotToBeExecuted:
                return "End_NotToBeExecuted";
            case IngestionStatus::End_TaskSuccess:
                return "End_TaskSuccess";
            default:
            throw runtime_error(string("Wrong IngestionStatus"));
        }
    }
    static IngestionStatus toIngestionStatus(const string& ingestionStatus)
    {
        string lowerCase;
        lowerCase.resize(ingestionStatus.size());
        transform(ingestionStatus.begin(), ingestionStatus.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "start_taskqueued")
            return IngestionStatus::Start_TaskQueued;
        else if (lowerCase == "sourcedownloadinginprogress")
            return IngestionStatus::SourceDownloadingInProgress;
        else if (lowerCase == "sourcemovinginprogress")
            return IngestionStatus::SourceMovingInProgress;
        else if (lowerCase == "sourcecopinginprogress")
            return IngestionStatus::SourceCopingInProgress;
        else if (lowerCase == "sourceuploadinginprogress")
            return IngestionStatus::SourceUploadingInProgress;
        else if (lowerCase == "encodingqueued")
            return IngestionStatus::EncodingQueued;
        else if (lowerCase == "end_dwluplorenccancelledbyuser")
            return IngestionStatus::End_DwlUplOrEncCancelledByUser;
        else if (lowerCase == "end_validationmetadatafailed")
            return IngestionStatus::End_ValidationMetadataFailed;
        else if (lowerCase == "end_validationmediasourcefailed")
            return IngestionStatus::End_ValidationMediaSourceFailed;
        else if (lowerCase == "end_workspacereachedhismaxingestionnumber")
            return IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber;
        else if (lowerCase == "end_ingestionfailure")
            return IngestionStatus::End_IngestionFailure;
        else if (lowerCase == "end_nottobeexecuted")
            return IngestionStatus::End_NotToBeExecuted;
        else if (lowerCase == "end_tasksuccess")
            return IngestionStatus::End_TaskSuccess;
        else
            throw runtime_error(string("Wrong IngestionStatus")
                    + ", ingestionStatus: " + ingestionStatus
                    );
    }
    static bool isIngestionStatusFinalState(const IngestionStatus& ingestionStatus)
    {
        string prefix = "End";
        string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
        
        return (sIngestionStatus.size() >= prefix.size() && 0 == sIngestionStatus.compare(0, prefix.size(), prefix));
    }
    static bool isIngestionStatusSuccess(const IngestionStatus& ingestionStatus)
    {
        return (ingestionStatus == IngestionStatus::End_TaskSuccess || ingestionStatus == IngestionStatus::End_NotToBeExecuted);
    }
    static bool isIngestionStatusFailed(const IngestionStatus& ingestionStatus)
    {
        return (isIngestionStatusFinalState(ingestionStatus) && !isIngestionStatusSuccess(ingestionStatus));
    }

    enum class IngestionRootStatus {
        NotCompleted,
        CompletedSuccessful,
        CompletedWithFailures
    };
    static const char* toString(const IngestionRootStatus& ingestionRootStatus)
    {
        switch (ingestionRootStatus)
        {
            case IngestionRootStatus::NotCompleted:
                return "NotCompleted";
            case IngestionRootStatus::CompletedSuccessful:
                return "CompletedSuccessful";
            case IngestionRootStatus::CompletedWithFailures:
                return "CompletedWithFailures";
            default:
            throw runtime_error(string("Wrong IngestionRootStatus"));
        }
    }
    static IngestionRootStatus toIngestionRootStatus(const string& ingestionRootStatus)
    {
        string lowerCase;
        lowerCase.resize(ingestionRootStatus.size());
        transform(ingestionRootStatus.begin(), ingestionRootStatus.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "notcompleted")
            return IngestionRootStatus::NotCompleted;
        if (lowerCase == "completedsuccessful")
            return IngestionRootStatus::CompletedSuccessful;
        if (lowerCase == "completedwithfailures")
            return IngestionRootStatus::CompletedWithFailures;
        else
            throw runtime_error(string("Wrong IngestionRootStatus")
                    + ", ingestionRootStatus: " + ingestionRootStatus
                    );
    }
    
public:
    MMSEngineDBFacade(
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger
            );

    ~MMSEngineDBFacade();

    // vector<shared_ptr<Customer>> getCustomers();
    
    shared_ptr<Workspace> getWorkspace(int64_t workspaceKey);

    shared_ptr<Workspace> getWorkspace(string workspaceName);

    bool isMetadataPresent(Json::Value root, string field);

    tuple<int64_t,int64_t,string> registerUserAndAddWorkspace(
        string userName,
        string userEmailAddress,
        string userPassword,
        string userCountry,
        string workspaceName,
        string workspaceDirectoryName,
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInMB,
        string languageCode,
        chrono::system_clock::time_point userExpirationDate);
    
    pair<int64_t,string> createWorkspace(
        int64_t userKey,
        string workspaceName,
        string workspaceDirectoryName,
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInMB,
        string languageCode,
        chrono::system_clock::time_point userExpirationDate);

    pair<int64_t,string> registerUserAndShareWorkspace(
        bool userAlreadyPresent,
        string userName,
        string userEmailAddress,
        string userPassword,
        string userCountry,
        bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia,
        int64_t workspaceKeyToBeShared,
        chrono::system_clock::time_point userExpirationDate);

    tuple<string,string,string> confirmRegistration(string confirmationCode);

    pair<string,string> getUserDetails(int64_t userKey);

    tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool> checkAPIKey (string apiKey);

    Json::Value login (string eMailAddress, string password);

    Json::Value getWorkspaceDetails (int64_t userKey);

    Json::Value updateWorkspaceDetails (
        int64_t userKey,
        int64_t workspaceKey,
        bool newEnabled, string newMaxEncodingPriority,
        string newEncodingPeriod, int64_t newMaxIngestionsNumber,
        int64_t newMaxStorageInMB, string newLanguageCode,
        bool newIngestWorkflow, bool newCreateProfiles,
        bool newDeliveryAuthorization, bool newShareWorkspace,
        bool newEditMedia);

    Json::Value updateUser (
        int64_t userKey,
        string name, 
        string email, 
        string password,
        string country);

    int64_t addEncodingProfilesSet (
        shared_ptr<MySQLConnection> conn, int64_t workspaceKey,
        MMSEngineDBFacade::ContentType contentType, 
        string label);

    int64_t addEncodingProfile(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        EncodingTechnology encodingTechnology,
        string jsonProfile,
        int64_t encodingProfilesSetKey  // -1 if it is not associated to any Set
    );

    int64_t addEncodingProfile(
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        EncodingTechnology encodingTechnology,
        string jsonEncodingProfile);

    void removeEncodingProfile(
        int64_t workspaceKey, int64_t encodingProfileKey);

    int64_t addEncodingProfileIntoSet(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        int64_t encodingProfilesSetKey);

    void removeEncodingProfilesSet(
        int64_t workspaceKey, int64_t encodingProfilesSetKey);

    void getExpiredMediaItemKeys(
        string processorMMS,
        vector<pair<shared_ptr<Workspace>,int64_t>>& mediaItemKeyToBeRemoved,
        int maxMediaItemKeysNumber);

    void getIngestionsToBeManaged(
        vector<tuple<int64_t,shared_ptr<Workspace>,string, IngestionType, IngestionStatus>>& ingestionsToBeManaged,
        string processorMMS,
        int maxIngestionJobs
        // int maxIngestionJobsWithDependencyToCheck
    );

	void manageMainAndBackupOfRunnungLiveRecordingHA();

    shared_ptr<MySQLConnection> beginIngestionJobs ();
    
    int64_t addIngestionRoot (
        shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, string rootType, string rootLabel,
		string metaDataContent);

    int64_t addIngestionJob (shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, int64_t ingestionRootKey, 
        string label, string metadataContent,
        MMSEngineDBFacade::IngestionType ingestionType, 
        vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess
    );

    void updateIngestionJobMetadataContent (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        string metadataContent);

    shared_ptr<MySQLConnection> endIngestionJobs (
        shared_ptr<MySQLConnection> conn, bool commit);

    /*
    void updateIngestionJob (
        int64_t ingestionJobKey,
        string processorMMS);
    */
    
    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS);

    void updateIngestionJob (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS);

    /*
    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        int64_t mediaItemKey,
        int64_t physicalPathKey,
        string errorMessage,
        string processorMMS);
    */
    
    /*
    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionType ingestionType,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS);
    */
    
    bool updateIngestionJobSourceDownloadingInProgress (
        int64_t ingestionJobKey,
        double downloadingPercentage);

    bool updateIngestionJobSourceUploadingInProgress (
        int64_t ingestionJobKey,
        double uploadingPercentage);

    void updateIngestionJobSourceBinaryTransferred (
        int64_t ingestionJobKey,
        bool sourceBinaryTransferred);

	string getIngestionRootMetaDataContent (
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey);

    Json::Value getIngestionRootsStatus (
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, 
        string startIngestionDate, string endIngestionDate,
        string label, string status, bool asc);

    Json::Value getIngestionJobsStatus (
        shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        bool asc, string status);

    Json::Value getEncodingJobsStatus (
        shared_ptr<Workspace> workspace, int64_t encodingJobKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        bool asc, string status);

    Json::Value getMediaItemsList (
        int64_t workspaceKey, int64_t mediaItemKey, int64_t physicalPathKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk, string jsonCondition, vector<string>& tags,
		string ingestionDateAndTitleOrder, string jsonOrderBy, bool admin);

    Json::Value getEncodingProfilesSetList (
        int64_t workspaceKey, int64_t encodingProfilesSetKey,
        bool contentTypePresent, ContentType contentType);

    Json::Value getEncodingProfileList (
        int64_t workspaceKey, int64_t encodingProfileKey,
        bool contentTypePresent, ContentType contentType);

    int64_t getPhysicalPathDetails(
        int64_t referenceMediaItemKey, int64_t encodingProfileKey);
   
    int64_t getPhysicalPathDetails(
        int64_t workspaceKey, 
        int64_t mediaItemKey, ContentType contentType,
        string encodingProfileLabel);

    tuple<MMSEngineDBFacade::ContentType,string,string> getMediaItemKeyDetails(
        int64_t mediaItemKey, bool warningIfMissing);

    tuple<int64_t,MMSEngineDBFacade::ContentType,string,string> getMediaItemKeyDetailsByPhysicalPathKey(
        int64_t physicalPathKey, bool warningIfMissing);
    
    void getMediaItemDetailsByIngestionJobKey(
        int64_t referenceIngestionJobKey, 
            vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>>& mediaItemsDetails,
            bool warningIfMissing);

    pair<int64_t,MMSEngineDBFacade::ContentType> getMediaItemKeyDetailsByUniqueName(
        int64_t workspaceKey, string referenceUniqueName, bool warningIfMissing);
    
    tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> getVideoDetails(
        int64_t mediaItemKey, int64_t physicalpathKey);

    tuple<int64_t,string,long,long,int> getAudioDetails(
        int64_t mediaItemKey, int64_t physicalpathKey);

    tuple<int,int,string,int> getImageDetails(
        int64_t mediaItemKey, int64_t physicalpathKey);

    vector<int64_t> getEncodingProfileKeysBySetKey(
        int64_t workspaceKey,
        int64_t encodingProfilesSetKey);

    vector<int64_t> getEncodingProfileKeysBySetLabel(
        int64_t workspaceKey,
        string label);
    
    tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> getStorageDetails(
        int64_t physicalPathKey);

    tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> getStorageDetails(
        int64_t mediaItemKey,
        int64_t encodingProfileKey
    );

    void getAllStorageDetails(int64_t mediaItemKey,
        vector<tuple<int,string,string,string>>& allStorageDetails);
    
    int64_t createDeliveryAuthorization(
        int64_t userKey,
        string clientIPAddress,
        int64_t physicalPathKey,
        string deliveryURI,
        int ttlInSeconds,
        int maxRetries);

    bool checkDeliveryAuthorization(
        int64_t deliveryAuthorizationKey,
        string contentURI);

    void resetProcessingJobsIfNeeded(string processorMMS);

    void getEncodingJobs(
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems,
		int maxEncodingsNumber);
    
    int addEncodingJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        string destEncodingProfileLabel,
        int64_t sourceMediaItemKey,
        int64_t sourcePhysicalPathKey,
        EncodingPriority encodingPriority);

    int addEncodingJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        int64_t destEncodingProfileKey,
        int64_t sourceMediaItemKey,
        int64_t sourcePhysicalPathKey,
        EncodingPriority encodingPriority);

    int addEncoding_OverlayImageOnVideoJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        int64_t mediaItemKey_1, int64_t physicalPathKey_1,
        int64_t mediaItemKey_2, int64_t physicalPathKey_2,
        string imagePosition_X_InPixel, string imagePosition_Y_InPixel,
        EncodingPriority encodingPriority);

    int addEncoding_OverlayTextOnVideoJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        EncodingPriority encodingPriority,

        int64_t mediaItemKey, int64_t physicalPathKey,
        string text,
        string textPosition_X_InPixel,
        string textPosition_Y_InPixel,
        string fontType,
        int fontSize,
        string fontColor,
        int textPercentageOpacity,
        bool boxEnable,
        string boxColor,
        int boxPercentageOpacity);

    int addEncoding_GenerateFramesJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        EncodingPriority encodingPriority,
        string imageDirectory, 
        double startTimeInSeconds, int maxFramesNumber, 
        string videoFilter, int periodInSeconds, 
        bool mjpeg, int imageWidth, int imageHeight,
        int64_t sourceVideoPhysicalPathKey,
        int64_t videoDurationInMilliSeconds);

    int addEncoding_SlideShowJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        vector<string>& sourcePhysicalPaths,
        double durationOfEachSlideInSeconds,
        int outputFrameRate,
        EncodingPriority encodingPriority);

    int addEncoding_FaceRecognitionJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
        string faceRecognitionCascadeName,
		string faceRecognitionOutput,
        EncodingPriority encodingPriority);

    int addEncoding_FaceIdentificationJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
		string faceIdentificationCascadeName,
		string jsonDeepLearnedModelTags,
        EncodingPriority encodingPriority);

	int addEncoding_LiveRecorderJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,
		bool highAvailability,
		bool main,
		string liveURL,
		time_t utcRecordingPeriodStart,
		time_t utcRecordingPeriodEnd,
		int segmentDurationInSeconds,
		string outputFileFormat,
		EncodingPriority encodingPriority);

    int updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t mediaItemKey,
        int64_t encodedPhysicalPathKey,
        int64_t ingestionJobKey);

    void updateEncodingJobPriority (
        shared_ptr<Workspace> workspace,
        int64_t encodingJobKey,
        EncodingPriority newEncodingPriority);

    void updateEncodingJobTryAgain (
        shared_ptr<Workspace> workspace,
        int64_t encodingJobKey);
    
    void updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage);

    void updateEncodingJobTranscoder (
        int64_t encodingJobKey,
        string transcoder);

	string getEncodingJobDetails (
		int64_t encodingJobKey);

    void checkWorkspaceMaxIngestionNumber (int64_t workspaceKey);
    
    string nextRelativePathToBeUsed (int64_t workspaceKey);

    pair<int64_t,int64_t> saveIngestedContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        bool ingestionRowToBeUpdatedAsSuccess,
        MMSEngineDBFacade::ContentType contentType,
        Json::Value parametersRoot,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,
        
        // video-audio
        int64_t durationInMilliSeconds,
        long bitRate,
        string videoCodecName,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoAvgFrameRate,
        long videoBitRate,
        string audioCodecName,
        long audioSampleRate,
        int audioChannels,
        long audioBitRate,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
    );

    int64_t saveEncodedContentMetadata(
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
        // video-audio
        int64_t durationInMilliSeconds,
        long bitRate,
        string videoCodecName,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoAvgFrameRate,
        long videoBitRate,
        string audioCodecName,
        long audioSampleRate,
        int audioChannels,
        long audioBitRate,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
    );
    
    void removePhysicalPath (
        int64_t physicalPathKey);

    void removeMediaItem (
        int64_t mediaItemKey);

    int64_t addYouTubeConf(
        int64_t workspaceKey,
        string label,
        string refreshToken);

    void modifyYouTubeConf(
        int64_t confKey,
        int64_t workspaceKey,
        string label,
        string refreshToken);

    void removeYouTubeConf(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getYouTubeConfList (
        int64_t workspaceKey);

    string getYouTubeRefreshTokenByConfigurationLabel(
        int64_t workspaceKey, string youTubeConfigurationLabel);
    
    int64_t addFacebookConf(
        int64_t workspaceKey,
        string label,
        string pageToken);

    void modifyFacebookConf(
        int64_t confKey,
        int64_t workspaceKey,
        string label,
        string pageToken);

    void removeFacebookConf(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getFacebookConfList (
        int64_t workspaceKey);

    string getFacebookPageTokenByConfigurationLabel(
        int64_t workspaceKey, string facebookConfigurationLabel);
    
    int64_t addLiveURLConf(
        int64_t workspaceKey,
        string label,
        string liveURL);

    void modifyLiveURLConf(
        int64_t confKey,
        int64_t workspaceKey,
        string label,
        string liveURL);

    void removeLiveURLConf(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getLiveURLConfList (
        int64_t workspaceKey);

    string getLiveURLByConfigurationLabel(
        int64_t workspaceKey, string liveURLConfigurationLabel);
    
    int64_t addFTPConf(
        int64_t workspaceKey,
        string label,
        string server,
		int port,
		string userName,
		string password,
		string remoteDirectory);

    void modifyFTPConf(
        int64_t confKey,
        int64_t workspaceKey,
        string label,
        string server,
		int port,
		string userName,
		string password,
		string remoteDirectory);

    void removeFTPConf(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getFTPConfList (
        int64_t workspaceKey);

    tuple<string, int, string, string, string> getFTPByConfigurationLabel(
        int64_t workspaceKey, string liveURLConfigurationLabel);
    
    int64_t addEMailConf(
        int64_t workspaceKey,
        string label,
        string address,
		string subject,
		string message);

    void modifyEMailConf(
        int64_t confKey,
        int64_t workspaceKey,
        string label,
        string address,
		string subject,
		string message);

    void removeEMailConf(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getEMailConfList (
        int64_t workspaceKey);

    tuple<string, string, string> getEMailByConfigurationLabel(
        int64_t workspaceKey, string liveURLConfigurationLabel);
    
private:
    shared_ptr<spdlog::logger>                          _logger;
    shared_ptr<MySQLConnectionFactory>                  _mySQLConnectionFactory;
    shared_ptr<DBConnectionPool<MySQLConnection>>       _connectionPool;
    string                          _defaultContentProviderName;
    // string                          _defaultTerritoryName;
    int                             _maxEncodingFailures;
    int                             _confirmationCodeRetentionInDays;
    int                             _contentRetentionInMinutesDefaultValue;
	int								_contentNotTransferredRetentionInDays;
    
    chrono::system_clock::time_point _lastConnectionStatsReport;
    int             _dbConnectionPoolStatsReportPeriodInSeconds;

    string          _predefinedVideoProfilesDirectoryPath;
    string          _predefinedAudioProfilesDirectoryPath;
    string          _predefinedImageProfilesDirectoryPath;

    pair<int64_t,string> addWorkspace(
        shared_ptr<MySQLConnection> conn,
        int64_t userKey,
        bool admin,
        bool ingestWorkflow,
        bool createProfiles,
        bool deliveryAuthorization,
        bool shareWorkspace,
        bool editMedia,
        string workspaceName,
        string workspaceDirectoryName,
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInMB,
        string languageCode,
        chrono::system_clock::time_point userExpirationDate);

    int64_t saveEncodedContentMetadata(
        shared_ptr<MySQLConnection> conn,
        
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
        // video-audio
        int64_t durationInMilliSeconds,
        long bitRate,
        string videoCodecName,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoAvgFrameRate,
        long videoBitRate,
        string audioCodecName,
        long audioSampleRate,
        int audioChannels,
        long audioBitRate,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
    );
    
    Json::Value getIngestionJobRoot(
        shared_ptr<Workspace> workspace,
        shared_ptr<sql::ResultSet> resultSet,
        int64_t ingestionRootKey,
        shared_ptr<MySQLConnection> conn);

    void manageIngestionJobStatusUpdate (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        shared_ptr<MySQLConnection> conn);

    pair<int64_t,int64_t> getWorkspaceUsage(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey);

    void createTablesIfNeeded();

    bool isRealDBError(string exceptionMessage);

    bool isJsonTypeSupported(shared_ptr<sql::Statement> statement);
    
    int64_t getLastInsertId(shared_ptr<MySQLConnection> conn);

    /*
    int64_t addTerritory (
	shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string territoryName
    );
    */

    bool isMMSAdministratorUser (long lUserType)
    {
        return (lUserType & 0x1) != 0 ? true : false;
    }

    bool isMMSUser (long lUserType)
    {
        return (lUserType & 0x2) != 0 ? true : false;
    }

    bool isEndUser (long lUserType)
    {
        return (lUserType & 0x4) != 0 ? true : false;
    }

    bool isMMSEditorialUser (long lUserType)
    {
        return (lUserType & 0x8) != 0 ? true : false;
    }

    bool isBillingAdministratorUser (long lUserType)
    {
        return (lUserType & 0x10) != 0 ? true : false;
    }

    int getMMSAdministratorUser ()
    {
        return ((int) 0x1);
    }

    int getMMSUser ()
    {
        return ((int) 0x2);
    }

    int getEndUser ()
    {
        return ((int) 0x4);
    }

    int getMMSEditorialUser ()
    {
        return ((int) 0x8);
    }

};

#endif /* MMSEngineDBFacade_h */
