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
#include "catralibraries/MySQLConnection.h"



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

struct DeadlockFound: public exception { 
    
    string _errorMessage;
    
    DeadlockFound(string errorMessage)
    {
        _errorMessage = errorMessage;
    }
    
    char const* what() const throw() 
    {
        return _errorMessage.c_str();
    }; 
};

struct EncoderNotFound: public exception { 

    string _errorMessage;

    EncoderNotFound(string errorMessage)
    {
        _errorMessage = errorMessage;
    }

    char const* what() const throw() 
    {
        return _errorMessage.c_str();
    }; 
};

struct ConfKeyNotFound: public exception { 
    
    string _errorMessage;
    
    ConfKeyNotFound(string errorMessage)
    {
        _errorMessage = errorMessage;
    }
    
    char const* what() const throw() 
    {
        return _errorMessage.c_str();
    }; 
};

struct AlreadyLocked: public exception {    
    string _errorMessage;
    
    AlreadyLocked(string label, string owner, int currentLockDuration)
    {
		_errorMessage = string("Already locked")
			+ ", label: " + label
			+ ", owner: " + owner
			+ ", currentLockDuration (secs): " + to_string(currentLockDuration)
			;
    }
    
    char const* what() const throw() 
    {
        return _errorMessage.c_str();
    };
};

class MMSEngineDBFacade {

public:
    enum class LockType {
		Ingestion						= 0,
		Encoding						= 1
    };
    static const char* toString(const LockType& lockType)
    {
        switch (lockType)
        {
            case LockType::Ingestion:
                return "Ingestion";
            case LockType::Encoding:
                return "Encoding";
            default:
                throw runtime_error(string("Wrong LockType"));
        }
    }
    static LockType toLockType(const string& lockType)
    {
        string lowerCase;
        lowerCase.resize(lockType.size());
        transform(lockType.begin(), lockType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "ingestion")
            return LockType::Ingestion;
		else if (lowerCase == "encoding")
            return LockType::Encoding;
        else
            throw runtime_error(string("Wrong LockType")
                    + ", current lockType: " + lockType
                    );
    }

    enum class OncePerDayType {
		DBDataRetention						= 0
    };
    static const char* toString(const OncePerDayType& oncePerDayType)
    {
        switch (oncePerDayType)
        {
            case OncePerDayType::DBDataRetention:
                return "DBDataRetention";
            default:
                throw runtime_error(string("Wrong OncePerDayType"));
        }
    }
    static OncePerDayType toOncePerDayType(const string& oncePerDayType)
    {
        string lowerCase;
        lowerCase.resize(oncePerDayType.size());
        transform(oncePerDayType.begin(), oncePerDayType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "dbdataretention")
            return OncePerDayType::DBDataRetention;
        else
            throw runtime_error(string("Wrong OncePerDayType")
                    + ", current oncePerDayType: " + oncePerDayType
                    );
    }

    enum class ContentType {
		Video		= 0,
		Audio		= 1,
		Image		= 2
//		Application	= 3,
//		Ringtone	= 4,
//		Playlist	= 5,
//		Live		= 6
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
        End_KilledByUser		= 4,
        End_CanceledByUser		= 5,
        End_CanceledByMMS		= 6
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
            case EncodingStatus::End_CanceledByUser:
                return "End_CanceledByUser";
            case EncodingStatus::End_CanceledByMMS:
                return "End_CanceledByMMS";
            default:
            throw runtime_error(
					string("Wrong EncodingStatus: ") + to_string(static_cast<int>(encodingStatus)));
        }
    }
    static EncodingStatus toEncodingStatus(const string& encodingStatus)
    {
        string lowerCase;
        lowerCase.resize(encodingStatus.size());
        transform(encodingStatus.begin(), encodingStatus.end(), lowerCase.begin(),
				[](unsigned char c){return tolower(c); } );

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
        else if (lowerCase == "end_canceledbyuser")
            return EncodingStatus::End_CanceledByUser;
        else if (lowerCase == "end_canceledbymms")
            return EncodingStatus::End_CanceledByMMS;
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
		LiveRecorder		= 8,
		VideoSpeed			= 9,
		PictureInPicture	= 10,
		LiveProxy			= 11,
		LiveGrid			= 12,
		Countdown			= 13,
		IntroOutroOverlay	= 14,
		CutFrameAccurate	= 15,
		VODProxy			= 16
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
            case EncodingType::VideoSpeed:
                return "VideoSpeed";
            case EncodingType::PictureInPicture:
                return "PictureInPicture";
            case EncodingType::LiveProxy:
                return "LiveProxy";
            case EncodingType::LiveGrid:
                return "LiveGrid";
            case EncodingType::Countdown:
                return "Countdown";
            case EncodingType::IntroOutroOverlay:
                return "IntroOutroOverlay";
            case EncodingType::CutFrameAccurate:
                return "CutFrameAccurate";
            case EncodingType::VODProxy:
                return "VODProxy";
            default:
				throw runtime_error(string("Wrong EncodingType"));
        }
    }
    static EncodingType toEncodingType(const string& encodingType)
    {
        string lowerCase;
        lowerCase.resize(encodingType.size());
        transform(encodingType.begin(), encodingType.end(), lowerCase.begin(),
			[](unsigned char c){return tolower(c); } );

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
        else if (lowerCase == "videospeed")
            return EncodingType::VideoSpeed;
        else if (lowerCase == "pictureinpicture")
            return EncodingType::PictureInPicture;
        else if (lowerCase == "liveproxy")
            return EncodingType::LiveProxy;
        else if (lowerCase == "livegrid")
            return EncodingType::LiveGrid;
        else if (lowerCase == "countdown")
            return EncodingType::Countdown;
        else if (lowerCase == "introoutrooverlay")
            return EncodingType::IntroOutroOverlay;
        else if (lowerCase == "cutframeaccurate")
            return EncodingType::CutFrameAccurate;
        else if (lowerCase == "vodproxy")
            return EncodingType::VODProxy;
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
		KilledByUser,
		CanceledByUser,
		CanceledByMMS
    };
    static const char* toString(const EncodingError& encodingError)
    {
        switch (encodingError)
        {
            case EncodingError::NoError:
                return "NoError";
            case EncodingError::PunctualError:
                return "PunctualError";
            case EncodingError::MaxCapacityReached:
                return "MaxCapacityReached";
            case EncodingError::ErrorBeforeEncoding:
                return "ErrorBeforeEncoding";
            case EncodingError::KilledByUser:
                return "KilledByUser";
            case EncodingError::CanceledByUser:
                return "CanceledByUser";
            case EncodingError::CanceledByMMS:
                return "CanceledByMMS";
            default:
            throw runtime_error(string("Wrong EncodingError"));
        }
    }
    static EncodingError toEncodingError(const string& encodingError)
    {
        string lowerCase;
        lowerCase.resize(encodingError.size());
        transform(encodingError.begin(), encodingError.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "noerror")
            return EncodingError::NoError;
        else if (lowerCase == "punctualerror")
            return EncodingError::PunctualError;
        else if (lowerCase == "maxcapacityreached")
            return EncodingError::MaxCapacityReached;
        else if (lowerCase == "errorbeforeencoding")
            return EncodingError::ErrorBeforeEncoding;
        else if (lowerCase == "killedbyuser")
            return EncodingError::KilledByUser;
        else if (lowerCase == "canceledbyuser")
            return EncodingError::CanceledByUser;
        else if (lowerCase == "canceledbymms")
            return EncodingError::CanceledByMMS;
        else
            throw runtime_error(string("Wrong EncodingError")
                    + ", encodingError: " + encodingError
                    );
    }

    
    enum class DeliveryTechnology {
        Download,				// image
        DownloadAndStreaming,	// MP4,
        HTTPStreaming			// HLS/DASH
        // WEBM,               // (VP8 and Vorbis)
        // WindowsMedia,
        // MP3					// (Download),
    };
    static const char* toString(const DeliveryTechnology& deliveryTechnology)
    {
        switch (deliveryTechnology)
        {
            case DeliveryTechnology::Download:
                return "Download";
            case DeliveryTechnology::DownloadAndStreaming:
                return "DownloadAndStreaming";
            case DeliveryTechnology::HTTPStreaming:
                return "HTTPStreaming";
            default:
				throw runtime_error(string("Wrong deliveryTechnology"));
        }
    }
    static DeliveryTechnology toDeliveryTechnology(const string& deliveryTechnology)
    {
        string lowerCase;
        lowerCase.resize(deliveryTechnology.size());
        transform(deliveryTechnology.begin(), deliveryTechnology.end(),
				lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "download")
            return DeliveryTechnology::Download;
		else if (lowerCase == "downloadandstreaming")
            return DeliveryTechnology::DownloadAndStreaming;
		else if (lowerCase == "httpstreaming")
            return DeliveryTechnology::HTTPStreaming;
        else
            throw runtime_error(string("Wrong DeliveryTechnology")
                    + ", deliveryTechnology: " + deliveryTechnology
                    );
    }

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

    enum class VideoSpeedType {
        SlowDown			= 0,
        SpeedUp				= 1
    };
    static const char* toString(const VideoSpeedType& videoSpeedType)
    {
        switch (videoSpeedType)
        {
            case VideoSpeedType::SlowDown:
                return "SlowDown";
            case VideoSpeedType::SpeedUp:
                return "SpeedUp";
            default:
            throw runtime_error(string("Wrong VideoSpeedType"));
        }
    }
    static VideoSpeedType toVideoSpeedType(const string& videoSpeedType)
    {
        string lowerCase;
        lowerCase.resize(videoSpeedType.size());
        transform(videoSpeedType.begin(), videoSpeedType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "slowdown")
            return VideoSpeedType::SlowDown;
		else if (lowerCase == "speedup")
            return VideoSpeedType::SpeedUp;
        else
            throw runtime_error(string("Wrong VideoSpeedType")
                    + ", videoSpeedType: " + videoSpeedType
                    );
    }

    struct EncodingItem
    {
        long long                               _encodingJobKey;
        long long                               _ingestionJobKey;
        EncodingPriority                        _encodingPriority;
        EncodingType                            _encodingType;

		// Key of the Encoder used by this job:
		int64_t									_encoderKey;
		string									_stagingEncodedAssetPathName;

        // MMS_IngestionJob -> metadata:
		Json::Value								_ingestedParametersRoot;

        // MMS_EncodingJob -> parameters:
        string                                  _encodingParameters;
        Json::Value                             _encodingParametersRoot;

        shared_ptr<Workspace>                   _workspace;

        struct OverlayImageOnVideoData {
            string                                  _videoFileName;
            string                                  _videoRelativePath;
            int64_t                                 _videoDurationInMilliSeconds;

            string                                  _imageFileName;
            string                                  _imageRelativePath;
        };
        
        struct GenerateFramesData {
            string                                  _videoFileName;
            string                                  _videoRelativePath;
            int64_t                                 _videoDurationInMilliSeconds;
        };

		struct LiveRecorderData {
            // Json::Value								_monitorVirtualVODEncodingProfileDetailsRoot;
			// MMSEngineDBFacade::ContentType			_monitorVirtualVODEncodingProfileContentType;
		};

        struct VideoSpeedData {
            string                                  _videoFileName;
            string                                  _videoRelativePath;
            int64_t                                 _videoDurationInMilliSeconds;
        };

        struct PictureInPictureData {
            string                                  _mainVideoFileName;
            string                                  _mainVideoRelativePath;
            int64_t                                 _mainVideoDurationInMilliSeconds;

            string                                  _overlayVideoFileName;
            string                                  _overlayVideoRelativePath;
            int64_t                                 _overlayVideoDurationInMilliSeconds;
        };

		struct LiveGridData {
			MMSEngineDBFacade::DeliveryTechnology	_deliveryTechnology;
            Json::Value								_encodingProfileDetailsRoot;
		};

        shared_ptr<OverlayImageOnVideoData>         _overlayImageOnVideoData;
        shared_ptr<GenerateFramesData>              _generateFramesData;
		shared_ptr<LiveRecorderData>				_liveRecorderData;
		shared_ptr<VideoSpeedData>					_videoSpeedData;
		shared_ptr<PictureInPictureData>			_pictureInPictureData;
		shared_ptr<LiveGridData>					_liveGridData;
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
        VideoSpeed				= 24,
        PictureInPicture		= 25,
        LiveProxy				= 26,
        LiveCut					= 27,
        LiveGrid				= 28,
        Countdown				= 29,
        IntroOutroOverlay		= 30,
        VODProxy				= 31,
        YouTubeLiveBroadcast	= 32,

        EmailNotification       = 60,
        MediaCrossReference		= 61,
        WorkflowAsLibrary		= 62,
        CheckStreaming			= 63,

        ContentUpdate           = 80,
        ContentRemove           = 90,
        GroupOfTasks			= 100
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
			case IngestionType::VideoSpeed:
				return "Video-Speed";
			case IngestionType::PictureInPicture:
				return "Picture-In-Picture";
			case IngestionType::LiveProxy:
				return "Live-Proxy";
			case IngestionType::LiveCut:
				return "Live-Cut";
			case IngestionType::LiveGrid:
				return "Live-Grid";
			case IngestionType::Countdown:
				return "Countdown";
			case IngestionType::IntroOutroOverlay:
				return "Intro-Outro-Overlay";
			case IngestionType::VODProxy:
				return "VOD-Proxy";
			case IngestionType::YouTubeLiveBroadcast:
				return "YouTube-Live-Broadcast";

            case IngestionType::EmailNotification:
                return "Email-Notification";
            case IngestionType::MediaCrossReference:
                return "Media-Cross-Reference";
            case IngestionType::WorkflowAsLibrary:
                return "Workflow-As-Library";
			case IngestionType::CheckStreaming:
				return "Check-Streaming";

            case IngestionType::ContentUpdate:
                return "ContentUpdate";
            case IngestionType::ContentRemove:
                return "ContentRemove";

            case IngestionType::GroupOfTasks:
                return "GroupOfTasks";

            default:
            throw runtime_error(string("Wrong IngestionType"));
        }
    }
    static IngestionType toIngestionType(const string& ingestionType)
    {
        string lowerCase;
        lowerCase.resize(ingestionType.size());
        transform(ingestionType.begin(), ingestionType.end(), lowerCase.begin(),
			[](unsigned char c){return tolower(c); } );

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
        else if (lowerCase == "video-speed")
            return IngestionType::VideoSpeed;
        else if (lowerCase == "picture-in-picture")
            return IngestionType::PictureInPicture;
        else if (lowerCase == "live-proxy")
            return IngestionType::LiveProxy;
        else if (lowerCase == "live-cut")
            return IngestionType::LiveCut;
        else if (lowerCase == "live-grid")
            return IngestionType::LiveGrid;
        else if (lowerCase == "countdown")
            return IngestionType::Countdown;
        else if (lowerCase == "intro-outro-overlay")
            return IngestionType::IntroOutroOverlay;
        else if (lowerCase == "vod-proxy")
            return IngestionType::VODProxy;
        else if (lowerCase == "youtube-live-broadcast")
            return IngestionType::YouTubeLiveBroadcast;

        else if (lowerCase == "email-notification")
            return IngestionType::EmailNotification;
        else if (lowerCase == "media-cross-reference")
            return IngestionType::MediaCrossReference;
        else if (lowerCase == "workflow-as-library")
            return IngestionType::WorkflowAsLibrary;
        else if (lowerCase == "check-streaming")
            return IngestionType::CheckStreaming;

        else if (lowerCase == "contentupdate")
            return IngestionType::ContentUpdate;
        else if (lowerCase == "contentremove")
            return IngestionType::ContentRemove;

        else if (lowerCase == "groupoftasks")
            return IngestionType::GroupOfTasks;

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

		// DOWNLOAD / UPLOAD / ENCODING / JOB --> cancelled by User
        End_CanceledByUser,
        End_CanceledByMMS,

        End_ValidationMetadataFailed,   

        End_ValidationMediaSourceFailed,   

        End_WorkspaceReachedMaxStorageOrIngestionNumber,
        
        End_IngestionFailure,                    // nothing done
        
        End_NotToBeExecuted,    // because of dependencies    
        End_NotToBeExecuted_ChunkNotSelected,    // because of dependencies    
        
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
            case IngestionStatus::End_CanceledByUser:
                return "End_CanceledByUser";
            case IngestionStatus::End_CanceledByMMS:
                return "End_CanceledByMMS";
            case IngestionStatus::End_ValidationMetadataFailed:
                return "End_ValidationMetadataFailed";
            case IngestionStatus::End_ValidationMediaSourceFailed:
                return "End_ValidationMediaSourceFailed";
            case IngestionStatus::End_WorkspaceReachedMaxStorageOrIngestionNumber:
                return "End_WorkspaceReachedMaxStorageOrIngestionNumber";
            case IngestionStatus::End_IngestionFailure:
                return "End_IngestionFailure";
            case IngestionStatus::End_NotToBeExecuted:
                return "End_NotToBeExecuted";
            case IngestionStatus::End_NotToBeExecuted_ChunkNotSelected:
                return "End_NotToBeExecuted_ChunkNotSelected";
            case IngestionStatus::End_TaskSuccess:
                return "End_TaskSuccess";
            default:
            throw runtime_error(string("Wrong IngestionStatus: ") + to_string(static_cast<int>(ingestionStatus)));
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
        else if (lowerCase == "end_canceledbyuser")
            return IngestionStatus::End_CanceledByUser;
        else if (lowerCase == "end_canceledbymms")
            return IngestionStatus::End_CanceledByMMS;
        else if (lowerCase == "end_validationmetadatafailed")
            return IngestionStatus::End_ValidationMetadataFailed;
        else if (lowerCase == "end_validationmediasourcefailed")
            return IngestionStatus::End_ValidationMediaSourceFailed;
        else if (lowerCase == "end_workspacereachedmaxstorageoringestionnumber")
            return IngestionStatus::End_WorkspaceReachedMaxStorageOrIngestionNumber;
        else if (lowerCase == "end_ingestionfailure")
            return IngestionStatus::End_IngestionFailure;
        else if (lowerCase == "end_nottobeexecuted")
            return IngestionStatus::End_NotToBeExecuted;
        else if (lowerCase == "end_nottobeexecuted_chunknotselected")
            return IngestionStatus::End_NotToBeExecuted_ChunkNotSelected;
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
        return (ingestionStatus == IngestionStatus::End_TaskSuccess
				|| ingestionStatus == IngestionStatus::End_NotToBeExecuted
				|| ingestionStatus == IngestionStatus::End_NotToBeExecuted_ChunkNotSelected);
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
		else if (lowerCase == "completedsuccessful")
            return IngestionRootStatus::CompletedSuccessful;
		else if (lowerCase == "completedwithfailures")
            return IngestionRootStatus::CompletedWithFailures;
        else
            throw runtime_error(string("Wrong IngestionRootStatus")
                    + ", ingestionRootStatus: " + ingestionRootStatus
                    );
    }
    
    enum class CrossReferenceType {
        ImageOfVideo,
		VideoOfImage,	// will be converted to ImageOfVideo
        ImageOfAudio,
		AudioOfImage,	// will be converted to ImageOfAudio
        FaceOfVideo,
        CutOfVideo,
        CutOfAudio
    };
    static const char* toString(const CrossReferenceType& crossReferenceType)
    {
        switch (crossReferenceType)
        {
            case CrossReferenceType::ImageOfVideo:
                return "ImageOfVideo";
            case CrossReferenceType::VideoOfImage:
                return "VideoOfImage";
            case CrossReferenceType::ImageOfAudio:
                return "ImageOfAudio";
            case CrossReferenceType::AudioOfImage:
                return "AudioOfImage";
            case CrossReferenceType::FaceOfVideo:
                return "FaceOfVideo";
            case CrossReferenceType::CutOfVideo:
                return "CutOfVideo";
            case CrossReferenceType::CutOfAudio:
                return "CutOfAudio";
            default:
            throw runtime_error(string("Wrong CrossReferenceType"));
        }
    }
    static CrossReferenceType toCrossReferenceType(const string& crossReferenceType)
    {
        string lowerCase;
        lowerCase.resize(crossReferenceType.size());
        transform(crossReferenceType.begin(), crossReferenceType.end(), lowerCase.begin(),
				[](unsigned char c){return tolower(c); } );

        if (lowerCase == "imageofvideo")
            return CrossReferenceType::ImageOfVideo;
        if (lowerCase == "videoofimage")
            return CrossReferenceType::VideoOfImage;
		else if (lowerCase == "imageofaudio")
            return CrossReferenceType::ImageOfAudio;
		else if (lowerCase == "audioofimage")
            return CrossReferenceType::AudioOfImage;
		else if (lowerCase == "faceofvideo")
            return CrossReferenceType::FaceOfVideo;
		else if (lowerCase == "cutofvideo")
            return CrossReferenceType::CutOfVideo;
		else if (lowerCase == "cutofaudio")
            return CrossReferenceType::CutOfAudio;
        else
            throw runtime_error(string("Wrong CrossReferenceType")
                    + ", crossReferenceType: " + crossReferenceType
                    );
    }
    
public:
    MMSEngineDBFacade(
        Json::Value configuration,
		size_t dbPoolSize,
        shared_ptr<spdlog::logger> logger
            );

    ~MMSEngineDBFacade();

    // vector<shared_ptr<Customer>> getCustomers();

    shared_ptr<Workspace> getWorkspace(int64_t workspaceKey);

    shared_ptr<Workspace> getWorkspace(string workspaceName);

	Json::Value getWorkspaceList(int64_t userKey, bool admin);

	Json::Value getLoginWorkspace(int64_t userKey);

    tuple<int64_t,int64_t,string> registerUserAndAddWorkspace(
        string userName,
        string userEmailAddress,
        string userPassword,
        string userCountry,
        string workspaceName,
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
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInMB,
        string languageCode,
		bool admin,
        chrono::system_clock::time_point userExpirationDate);

	void deleteWorkspace(int64_t userKey, int64_t workspaceKey);

    pair<int64_t,string> registerUserAndShareWorkspace(
			bool ldapEnabled,
        bool userAlreadyPresent,
        string userName,
        string userEmailAddress,
        string userPassword,
        string userCountry,
        bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
		bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
		bool applicationRecorder,
        int64_t workspaceKeyToBeShared,
        chrono::system_clock::time_point userExpirationDate);

    tuple<string,string,string> confirmRegistration(
		string confirmationCode, int expirationInDaysWorkspaceDefaultValue);

	pair<int64_t,string> registerActiveDirectoryUser(
		string userName,
		string userEmailAddress,
		string userCountry,
		bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
		bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
		bool applicationRecorder,
		string defaultWorkspaceKeys, int expirationInDaysWorkspaceDefaultValue,
		chrono::system_clock::time_point userExpirationDate
	);

	string createAPIKeyForActiveDirectoryUser(
		int64_t userKey,
		string userEmailAddress,
		bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
		bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
		bool applicationRecorder,
		int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue);

    pair<string,string> getUserDetails(int64_t userKey);
	pair<int64_t, string> getUserDetailsByEmail (string email);

    tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>
		checkAPIKey (string apiKey);

    Json::Value login (string eMailAddress, string password);

	int64_t saveLoginStatistics(
		int userKey, string ip,
		string continent, string continentCode, string country, string countryCode,
		string region, string city, string org, string isp, int timezoneGMTOffset);

    pair<int64_t,int64_t> getWorkspaceUsage(
        int64_t workspaceKey);

	Json::Value updateWorkspaceDetails (
		int64_t userKey,
		int64_t workspaceKey,
		bool enabledChanged, bool newEnabled,
		bool nameChanged, string newName,
		bool maxEncodingPriorityChanged, string newMaxEncodingPriority,
		bool encodingPeriodChanged, string newEncodingPeriod,
		bool maxIngestionsNumberChanged, int64_t newMaxIngestionsNumber,
		bool maxStorageInMBChanged, int64_t newMaxStorageInMB,
		bool languageCodeChanged, string newLanguageCode,
		bool expirationDateChanged, string newExpirationDate,
		bool newCreateRemoveWorkspace,
		bool newIngestWorkflow,
		bool newCreateProfiles,
		bool newDeliveryAuthorization,
		bool newShareWorkspace,
		bool newEditMedia,
		bool newEditConfiguration,
		bool newKillEncoding,
		bool newCancelIngestionJob,
		bool newEditEncodersPool,
		bool newApplicationRecorder);

	Json::Value setWorkspaceAsDefault (
		int64_t userKey,
		int64_t workspaceKey,
		int64_t workspaceKeyToBeSetAsDefault);

    Json::Value updateUser (
		bool admin,
		bool ldapEnabled,
        int64_t userKey,
        bool nameChanged, string name, 
        bool emailChanged, string email, 
        bool countryChanged, string country,
		bool expirationDateChanged, string expirationDate,
		bool passwordChanged, string newPassword, string oldPassword);

	string createResetPasswordToken(
		int64_t userKey);

	pair<string, string> resetPassword(
		string resetPassworkToken,
		string newPassword);

	void addIngestionJobOutput(
		int64_t ingestionJobKey,
		int64_t mediaItemKey,
		int64_t physicalPathKey,
		int64_t liveRecordingIngestionJobKey
	);

	long getIngestionJobOutputsCount(int64_t ingestionJobKey);

	tuple<int64_t, int64_t, string> getEncodingJobDetailsByIngestionJobKey(
		int64_t ingestionJobKey);

    int64_t addEncodingProfilesSet (
        shared_ptr<MySQLConnection> conn, int64_t workspaceKey,
        MMSEngineDBFacade::ContentType contentType, 
        string label);

    int64_t addEncodingProfile(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        DeliveryTechnology deliveryTechnology,
        string jsonProfile,
        int64_t encodingProfilesSetKey  // -1 if it is not associated to any Set
    );

    int64_t addEncodingProfile(
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        DeliveryTechnology deliveryTechnology,
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

    void getExpiredMediaItemKeysCheckingDependencies(
        string processorMMS,
        vector<tuple<shared_ptr<Workspace>,int64_t, int64_t>>& mediaItemKeyOrPhysicalPathKeyToBeRemoved,
        int maxMediaItemKeysNumber);

	int getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
			int64_t ingestionJobKey);

	int getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
			shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey);

    void getIngestionsToBeManaged(
        vector<tuple<int64_t, string, shared_ptr<Workspace>,string, string, IngestionType,
		IngestionStatus>>& ingestionsToBeManaged, string processorMMS, int maxIngestionJobs,
		int timeBeforeToPrepareResourcesInMinutes,
		bool onlyTasksNotInvolvingMMSEngineThreads
    );

	void setNotToBeExecutedStartingFromBecauseChunkNotSelected (
			int64_t ingestionJobKey, string processorMMS);

	// void manageMainAndBackupOfRunnungLiveRecordingHA(string processorMMS);

	// bool liveRecorderMainAndBackupChunksManagementCompleted(
	// 	int64_t ingestionJobKey);

	// void getRunningLiveRecorderVirtualVODsDetails(
	// 	vector<tuple<int64_t, int64_t, int, string, int, string, string, int64_t,
	// 	string>>& runningLiveRecordersDetails
	// );

    shared_ptr<MySQLConnection> beginIngestionJobs ();
    
    int64_t addIngestionRoot (
        shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, int64_t userKey, string rootType, string rootLabel,
		string metaDataContent);

	void addIngestionJobDependency (                                                           
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        int dependOnSuccess,
        int64_t dependOnIngestionJobKey,
        int orderNumber,
		bool referenceOutputDependency);

    int64_t addIngestionJob (shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, int64_t ingestionRootKey, 
        string label, string metadataContent,
        MMSEngineDBFacade::IngestionType ingestionType, 
		string processingStartingFrom,
        vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
		vector<int64_t> waitForGlobalIngestionJobKeys
    );

	void getIngestionJobsKeyByGlobalLabel (
		int64_t workspaceKey, string globalIngestionLabel,
		vector<int64_t>& ingestionJobsKey);

    void updateIngestionJobMetadataContent (
        int64_t ingestionJobKey,
        string metadataContent);

    void updateIngestionJobMetadataContent (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        string metadataContent);

	void updateIngestionJobParentGroupOfTasks(
		shared_ptr<MySQLConnection> conn,
		int64_t ingestionJobKey,
		int64_t parentGroupOfTasksIngestionJobKey);

	void updateIngestionJob_LiveRecorder (
		int64_t workspaceKey,
		int64_t ingestionJobKey,
		bool ingestionJobLabelModified, string newIngestionJobLabel,
		bool channelLabelModified, string newChannerLabel,
		bool recordingPeriodStartModified, string newRecordingPeriodStart,
		bool recordingPeriodEndModified, string newRecordingPeriodEnd,
		bool recordingVirtualVODModified, bool newRecordingVirtualVOD,
		bool admin);

	void getGroupOfTasksChildrenStatus(
		int64_t groupOfTasksIngestionJobKey,
		vector<pair<int64_t, MMSEngineDBFacade::IngestionStatus>>& groupOfTasksChildrenStatus);

    shared_ptr<MySQLConnection> endIngestionJobs (
        shared_ptr<MySQLConnection> conn, bool commit,
		int64_t ingestionRootKey, string processedMetadataContent);

    /*
    void updateIngestionJob (
        int64_t ingestionJobKey,
        string processorMMS);
    */
    
    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS = "noToBeUpdated");

	void appendIngestionJobErrorMessage (
		int64_t ingestionJobKey,
		string errorMessage);

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
		shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
		bool processedMetadata);

	tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string>
		getIngestionJobDetails(int64_t workspaceKey, int64_t ingestionJobKey);

    Json::Value getIngestionRootsStatus (
        shared_ptr<Workspace> workspace,
		int64_t ingestionRootKey, int64_t mediaItemKey,
        int start, int rows,
        // bool startAndEndIngestionDatePresent, 
        string startIngestionDate, string endIngestionDate,
        string label, string status, bool asc,
		bool dependencyInfo, bool ingestionJobOutputs);

    Json::Value getIngestionJobsStatus (
        shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
        int start, int rows, string label,
        /* bool startAndEndIngestionDatePresent, */ string startIngestionDate, string endIngestionDate,
		string startScheduleDate,
		string ingestionType,
		string configurationLabel, string outputChannelLabel, int64_t deliveryCode,
		bool broadcastIngestionJobKeyNotNull, string jsonParametersCondition,
        bool asc, string status,
		bool dependencyInfo,
		bool ingestionJobOutputs);

    Json::Value getEncodingJobsStatus (
        shared_ptr<Workspace> workspace, int64_t encodingJobKey,
        int start, int rows,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        // bool startAndEndEncodingDatePresent,
		string startEncodingDate, string endEncodingDate,
		int64_t encoderKey, bool alsoEncodingJobsFromOtherWorkspaces,
        bool asc, string status, string types);

	Json::Value updateMediaItem (
		int64_t workspaceKey,
		int64_t mediaItemKey,
		bool titleModified, string newTitle,
		bool userDataModified, string newUserData,
		bool retentionInMinutesModified, int64_t newRetentionInMinutes,
		bool tagsModified, Json::Value tagsRoot,
		bool uniqueNameModified, string newUniqueName,
		bool admin
	);

	Json::Value updatePhysicalPath (
		int64_t workspaceKey,
		int64_t mediaItemKey,
		int64_t physicalPathKey,
		int64_t newRetentionInMinutes,
		bool admin
	);

    Json::Value getMediaItemsList (
        int64_t workspaceKey, 
		int64_t mediaItemKey, string uniqueName, int64_t physicalPathKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk,
		int64_t deliveryCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		string jsonCondition,
		vector<string>& tagsIn, vector<string>& tagsNotIn,
		string orderBy, string jsonOrderBy,
		Json::Value responseFields, bool admin);

	Json::Value getTagsList (
        int64_t workspaceKey, int start, int rows,
        int liveRecordingChunk, bool contentTypePresent, ContentType contentType,
		string tagNameFilter);

	void updateMediaItem(
		int64_t mediaItemKey,
        string processorMMSForRetention
        );

	int64_t addUpdateWorkflowAsLibrary(
		int64_t userKey,
		int64_t workspaceKey,
		string label,
		int64_t thumbnailMediaItemKey,
		string jsonWorkflow,
		bool admin);

	void removeWorkflowAsLibrary(
		int64_t userKey,
		int64_t workspaceKey,
		int64_t workflowLibraryKey,
		bool admin);

	Json::Value getWorkflowsAsLibraryList (
		int64_t workspaceKey);

	string getWorkflowAsLibraryContent (
		int64_t workspaceKey,
		int64_t workflowLibraryKey);

	string getWorkflowAsLibraryContent (
		int64_t workspaceKey,
		string label);

    Json::Value getEncodingProfilesSetList (
        int64_t workspaceKey, int64_t encodingProfilesSetKey,
        bool contentTypePresent, ContentType contentType);

    Json::Value getEncodingProfileList (
        int64_t workspaceKey, int64_t encodingProfileKey,
        bool contentTypePresent, ContentType contentType,
		string label);

    int64_t getPhysicalPathDetails(
        int64_t referenceMediaItemKey, int64_t encodingProfileKey, bool warningIfMissing);
   
    int64_t getPhysicalPathDetails(
        int64_t workspaceKey, 
        int64_t mediaItemKey, ContentType contentType,
        string encodingProfileLabel, bool warningIfMissing);

	tuple<int64_t, int, string, string, int64_t, bool> getSourcePhysicalPath(
		int64_t mediaItemKey, bool warningIfMissing);

    tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> getMediaItemKeyDetails(
        int64_t workspaceKey, int64_t mediaItemKey, bool warningIfMissing);

    tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string>
		getMediaItemKeyDetailsByPhysicalPathKey(
        int64_t workspaceKey, int64_t physicalPathKey, bool warningIfMissing);
    
    void getMediaItemDetailsByIngestionJobKey(
		int64_t workspaceKey, int64_t referenceIngestionJobKey, 
		int maxLastMediaItemsToBeReturned,
		vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>>& mediaItemsDetails,
		bool warningIfMissing);

    pair<int64_t,MMSEngineDBFacade::ContentType> getMediaItemKeyDetailsByUniqueName(
        int64_t workspaceKey, string referenceUniqueName, bool warningIfMissing);
    
	int64_t getMediaDurationInMilliseconds(
		int64_t mediaItemKey, int64_t physicalPathKey);

    // tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> getVideoDetails(
    //     int64_t mediaItemKey, int64_t physicalpathKey);
	void getVideoDetails(
		int64_t mediaItemKey, int64_t physicalPathKey,
		vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>>& videoTracks,
		vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks);

    // tuple<int64_t,string,long,long,int> getAudioDetails(
    //     int64_t mediaItemKey, int64_t physicalpathKey);
	void getAudioDetails(
		int64_t mediaItemKey, int64_t physicalPathKey,
		vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks);

    tuple<int,int,string,int> getImageDetails(
        int64_t mediaItemKey, int64_t physicalpathKey);

    vector<int64_t> getEncodingProfileKeysBySetKey(
        int64_t workspaceKey,
        int64_t encodingProfilesSetKey);

    vector<int64_t> getEncodingProfileKeysBySetLabel(
        int64_t workspaceKey,
        string label);

    tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string>
		getEncodingProfileDetailsByKey( int64_t workspaceKey, int64_t encodingProfileKey);

    tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, int64_t, bool> getStorageDetails(
        int64_t physicalPathKey);

    tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, int64_t, bool> getStorageDetails(
        int64_t mediaItemKey,
        int64_t encodingProfileKey,
		bool warningIfMissing
    );

    void getAllStorageDetails(int64_t mediaItemKey,
        vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>>& allStorageDetails);
    
    int64_t createDeliveryAuthorization(
        int64_t userKey,
        string clientIPAddress,
        int64_t physicalPathKey,
		int64_t liveDeliveryKey,
        string deliveryURI,
        int ttlInSeconds,
        int maxRetries);

    bool checkDeliveryAuthorization(
        int64_t deliveryAuthorizationKey,
        string contentURI);

    void resetProcessingJobsIfNeeded(string processorMMS);

	void retentionOfIngestionData();

	void retentionOfStatisticData();

	void retentionOfDeliveryAuthorization();

	void fixEncodingJobsHavingWrongStatus();
	void fixIngestionJobsHavingWrongStatus();

    void getEncodingJobs(
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems,
		int timeBeforeToPrepareResourcesInMinutes,
		int maxEncodingsNumber);
    
	int64_t getEncodingProfileKeyByLabel (
		int64_t workspaceKey,
		MMSEngineDBFacade::ContentType contentType,
		string encodingProfileLabel,
		bool contentTypeToBeUsed = true);

    void addEncodingJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
		MMSEngineDBFacade::ContentType contentType,
        EncodingPriority encodingPriority,
        int64_t encodingProfileKey,
		Json::Value encodingProfileDetailsRoot,
        Json::Value sourcesToBeEncodedRoot
	);

    void addEncoding_OverlayImageOnVideoJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        int64_t mediaItemKey_1, int64_t physicalPathKey_1,
        int64_t mediaItemKey_2, int64_t physicalPathKey_2,
        string imagePosition_X_InPixel, string imagePosition_Y_InPixel,
        EncodingPriority encodingPriority);

    void addEncoding_OverlayTextOnVideoJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        EncodingPriority encodingPriority,

		string sourceAssetPathName,
		int64_t sourceDurationInMilliSeconds,                                                
		string videoFileNameExtension);

    void addEncoding_GenerateFramesJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        EncodingPriority encodingPriority,
        string imageDirectory, 
        double startTimeInSeconds, int maxFramesNumber, 
        string videoFilter, int periodInSeconds, 
        bool mjpeg, int imageWidth, int imageHeight,
        int64_t sourceVideoPhysicalPathKey,
        int64_t videoDurationInMilliSeconds);

    void addEncoding_SlideShowJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        vector<string>& imagesSourcePhysicalPaths,
        double durationOfEachSlideInSeconds,
        vector<string>& audiosSourcePhysicalPaths,
        double shortestAudioDurationInSeconds,
		string videoSyncMethod,
        int outputFrameRate,
        EncodingPriority encodingPriority);

    void addEncoding_FaceRecognitionJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
		int64_t sourceMediaItemKey,
		int64_t sourceVideoPhysicalPathKey,
        string sourcePhysicalPath,
        string faceRecognitionCascadeName,
		string faceRecognitionOutput,
        EncodingPriority encodingPriority,
		long initialFramesNumberToBeSkipped,
		bool oneFramePerSecond);

    void addEncoding_FaceIdentificationJob (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
		string faceIdentificationCascadeName,
		string deepLearnedModelTagsCommaSeparated,
        EncodingPriority encodingPriority);

	void addEncoding_LiveRecorderJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey, string ingestionJobLabel,
		string streamSourceType,
		// bool highAvailability,
		string configurationLabel, int64_t confKey, string url, string encodersPoolLabel,
		string userAgent,
		time_t utcRecordingPeriodStart,
		time_t utcRecordingPeriodEnd,
		bool autoRenew,
		int segmentDurationInSeconds,
		string outputFileFormat,
		EncodingPriority encodingPriority,

		int pushListenTimeout, int64_t pushEncoderKey, string pushServerName,
		int captureVideoDeviceNumber,
		string captureVideoInputFormat, int captureFrameRate,
		int captureWidth, int captureHeight, int captureAudioDeviceNumber,
		int captureChannelsNumber,

		string tvType, int64_t tvServiceId, int64_t tvFrequency,
		int64_t tvSymbolRate, int64_t tvBandwidthInHz, string tvModulation, int tvVideoPid,
		int tvAudioItalianPid,

		bool monitorHLS,
		bool liveRecorderVirtualVOD,
		string monitorManifestDirectoryPath,
		string monitorManifestFileName,

		Json::Value outputsRoot, Json::Value framesToBeDetectedRoot);

	void addEncoding_LiveProxyJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,
		Json::Value inputsRoot,
		string streamSourceType,
		int64_t utcProxyPeriodStart, // int64_t utcProxyPeriodEnd,
		// long maxAttemptsNumberInCaseOfErrors,
		long waitingSecondsBetweenAttemptsInCaseOfErrors,
		Json::Value outputsRoot);

	void addEncoding_VODProxyJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,
		Json::Value inputsRoot,
		int64_t utcProxyPeriodStart,
		Json::Value outputsRoot,
		long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors);

	void addEncoding_CountdownJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,
		Json::Value inputsRoot,
		int64_t utcProxyPeriodStart,
		Json::Value outputsRoot,
		long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors);

	void addEncoding_LiveGridJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,
		vector<tuple<int64_t, string, string>>& inputChannels,
		int64_t encodingProfileKey,
		string outputType, string manifestDirectoryPath, string manifestFileName,
		int segmentDurationInSeconds, int playlistEntriesNumber,
		string srtURL,
		long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors);

    void addEncoding_VideoSpeed (
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        int64_t mediaItemKey, int64_t physicalPathKey,
        VideoSpeedType videoSpeedType, int videoSpeedSize,
        EncodingPriority encodingPriority);

	void addEncoding_PictureInPictureJob (
		shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
		int64_t mainMediaItemKey, int64_t mainPhysicalPathKey,
		int64_t overlayMediaItemKey, int64_t overlayPhysicalPathKey,
        string overlayPosition_X_InPixel, string overlayPosition_Y_InPixel,
		string overlay_Width_InPixel, string overlay_Height_InPixel,
		bool soundOfMain, EncodingPriority encodingPriority);

	void addEncoding_IntroOutroOverlayJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,

		int64_t encodingProfileKey,
		Json::Value encodingProfileDetailsRoot,

		int64_t introVideoPhysicalPathKey,
		string introVideoAssetPathName,
		int64_t introVideoDurationInMilliSeconds,

		int64_t mainVideoPhysicalPathKey,
		string mainVideoAssetPathName,
		int64_t mainVideoDurationInMilliSeconds,

		int64_t outroVideoPhysicalPathKey,
		string outroVideoAssetPathName,
		int64_t outroVideoDurationInMilliSeconds,

		EncodingPriority encodingPriority);

	void addEncoding_CutFrameAccurate (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,

		int64_t sourceVideoMediaItemKey,
		int64_t sourceVideoPhysicalPathKey,
		string sourceVideoAssetPathName,
		// int64_t sourceDurationInMilliSeconds,
		double endTimeInSeconds,

		int64_t encodingProfileKey,
		Json::Value encodingProfileDetailsRoot,

		EncodingPriority encodingPriority,
		int64_t newUtcStartTimeInMilliSecs, int64_t newUtcEndTimeInMilliSecs);

	void updateIngestionAndEncodingLiveRecordingPeriod (
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		time_t utcRecordingPeriodStart,
		time_t utcRecordingPeriodEnd);

    int updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
		bool isIngestionJobFinished,
        int64_t ingestionJobKey,
		string ingestionErrorMessage = "",
		bool forceEncodingToBeFailed = false);

	void forceCancelEncodingJob(int64_t ingestionJobKey);

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

    void updateEncodingPid (
        int64_t encodingJobKey,
        int encodingPid);

	bool updateEncodingJobFailuresNumber (
        int64_t encodingJobKey,
        long failuresNumber);

	void updateEncodingJobIsKilled (
        int64_t encodingJobKey,
        bool isKilled);

    void updateEncodingJobTranscoder (
		int64_t encodingJobKey,
		int64_t encoderKey,
		string stagingEncodedAssetPathName);

	void updateEncodingJobParameters (
		int64_t encodingJobKey,
		string parameters);

	void updateOutputRtmpAndPlaURL (
		int64_t ingestionJobKey, int64_t encodingJobKey,
		int outputIndex, string rtmpURL, string playURL);

	tuple<int64_t, string, int64_t, MMSEngineDBFacade::EncodingStatus, string>
		getEncodingJobDetails (int64_t encodingJobKey);

    void checkWorkspaceStorageAndMaxIngestionNumber (int64_t workspaceKey);
    
    string nextRelativePathToBeUsed (int64_t workspaceKey);

    pair<int64_t,int64_t> saveSourceContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        bool ingestionRowToBeUpdatedAsSuccess,
        MMSEngineDBFacade::ContentType contentType,
        Json::Value parametersRoot,
		bool externalReadOnlyStorage,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,
        
        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,
		/*
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
		*/

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
    );

    int64_t saveVariantContentMetadata(
        int64_t workspaceKey,
		int64_t ingestionJobKey,
        int64_t liveRecordingIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
		int64_t physicalItemRetentionPeriodInMinutes,
        
        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,
		/*
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
		*/

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
    );
    
	/*
	void updateLiveRecorderVirtualVOD (
		int64_t workspaceKey,
		string liveRecorderVirtualVODUniqueName,
		int64_t mediaItemKey,
		int64_t physicalPathKey,

		int newRetentionInMinutes,

		int64_t firstUtcChunkStartTime,
		string sFirstUtcChunkStartTime,
		int64_t lastUtcChunkEndTime,
		string sLastUtcChunkEndTime,
		string title,
		int64_t durationInMilliSeconds,
		long bitRate,
		unsigned long long sizeInBytes,

		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks
	);
	*/

	void addCrossReference (
		int64_t ingestionJobKey,
		int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType,
		int64_t targetMediaItemKey, Json::Value crossReferenceParametersRoot);

    void removePhysicalPath (
        int64_t physicalPathKey);

    void removeMediaItem (
        int64_t mediaItemKey);

	Json::Value addYouTubeConf(
        int64_t workspaceKey,
        string label,
		string tokenType,
        string refreshToken,
		string accessToken);

	Json::Value modifyYouTubeConf(
        int64_t confKey,
        int64_t workspaceKey,
		string label, bool labelModified,
		string tokenType, bool tokenTypeModified,
		string refreshToken, bool refreshTokenModified,
		string accessToken, bool accessTokenModified
	);

    void removeYouTubeConf(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getYouTubeConfList (
        int64_t workspaceKey);

    tuple<string, string, string> getYouTubeDetailsByConfigurationLabel(
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
    
    Json::Value addStream(
        int64_t workspaceKey,
        string label,
		string sourceType,
		int64_t encodersPoolKey,
		string url,
		string pushProtocol,
		int64_t pushEncoderKey,
		string pushServerName,
		int pushServerPort,
		string pushUri,
		int pushListenTimeout,
		int captureVideoDeviceNumber,
		string captureVideoInputFormat,
		int captureFrameRate,
		int captureWidth,
		int captureHeight,
		int captureAudioDeviceNumber,
		int captureChannelsNumber,
		int64_t tvSourceTVConfKey,
        string type,
        string description,
        string name,
        string region,
        string country,
		int64_t imageMediaItemKey,
		string imageUniqueName,
		int position,
		Json::Value userData);

	Json::Value modifyStream(
		int64_t confKey,
		int64_t workspaceKey,
		bool labelToBeModified, string label,
		bool sourceTypeToBeModified, string sourceType,
		bool encodersPoolKeyToBeModified, int64_t encodersPoolKey,
		bool urlToBeModified, string url,
		bool pushProtocolToBeModified, string pushProtocol,
		bool pushEncoderKeyToBeModified, int64_t pushEncoderKey,
		bool pushServerNameToBeModified, string pushServerName,
		bool pushServerPortToBeModified, int pushServerPort,
		bool pushUriToBeModified, string pushUri,
		bool pushListenTimeoutToBeModified, int pushListenTimeout,
		bool captureVideoDeviceNumberToBeModified, int captureVideoDeviceNumber,
		bool captureVideoInputFormatToBeModified, string captureVideoInputFormat,
		bool captureFrameRateToBeModified, int captureFrameRate,
		bool captureWidthToBeModified, int captureWidth,
		bool captureHeightToBeModified, int captureHeight,
		bool captureAudioDeviceNumberToBeModified, int captureAudioDeviceNumber,
		bool captureChannelsNumberToBeModified, int captureChannelsNumber,
		bool tvSourceTVConfKeyToBeModified, int64_t tvSourceTVConfKey,
		bool typeToBeModified, string type,
		bool descriptionToBeModified, string description,
		bool nameToBeModified, string name,
		bool regionToBeModified, string region,
		bool countryToBeModified, string country,
		bool imageToBeModified, int64_t imageMediaItemKey, string imageUniqueName,
		bool positionToBeModified, int position,
		bool userDataToBeModified, Json::Value userData);

    void removeStream(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getStreamList (
        int64_t workspaceKey, int64_t liveURLKey,
		int start, int rows,
		string label, bool labelLike, string sourceType, string type,
		string name, string region, string country, string url,
		string labelOrder);

	tuple<int64_t, string, string, string, string, int64_t, string, int, string, int,
		int, string, int, int, int, int, int, int64_t>
		getStreamDetails(
			int64_t workspaceKey, string label, bool warningIfMissing);

	tuple<string, string, string> getStreamDetails(
		int64_t workspaceKey, int64_t confKey);

	Json::Value addSourceTVStream(
		string type,
		int64_t serviceId,
		int64_t networkId,
		int64_t transportStreamId,
		string name,
		string satellite,
		int64_t frequency,
		string lnb,
		int videoPid,
		string audioPids,
		int audioItalianPid,
		int audioEnglishPid,
		int teletextPid,
		string modulation,
		string polarization,
		int64_t symbolRate,
		int64_t bandwidthInHz,
		string country,
		string deliverySystem
	);

	Json::Value modifySourceTVStream(
		int64_t confKey,

		bool typeToBeModified, string type,
		bool serviceIdToBeModified, int64_t serviceId,
		bool networkIdToBeModified, int64_t networkId,
		bool transportStreamIdToBeModified, int64_t transportStreamId,
		bool nameToBeModified, string name,
		bool satelliteToBeModified, string satellite,
		bool frequencyToBeModified, int64_t frequency,
		bool lnbToBeModified, string lnb,
		bool videoPidToBeModified, int videoPid,
		bool audioPidsToBeModified, string audioPids,
		bool audioItalianPidToBeModified, int audioItalianPid,
		bool audioEnglishPidToBeModified, int audioEnglishPid,
		bool teletextPidToBeModified, int teletextPid,
		bool modulationToBeModified, string modulation,
		bool polarizationToBeModified, string polarization,
		bool symbolRateToBeModified, int64_t symbolRate,
		bool bandwidthInHzToBeModified, int64_t bandwidthInHz,
		bool countryToBeModified, string country,
		bool deliverySystemToBeModified, string deliverySystem
	);

    void removeSourceTVStream(
        int64_t confKey);

    Json::Value getSourceTVStreamList (
        int64_t confKey,
		int start, int rows,
		string type, int64_t serviceId, string name, int64_t frequency, string lnb,
		int videoPid, string audioPids,
		string nameOrder);

    tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int>
		getSourceTVStreamDetails(int64_t confKey, bool warningIfMissing);

	int64_t addAWSChannelConf(
		int64_t workspaceKey,
		string label,
		string channelId, string rtmpURL, string playURL, string type);

	void modifyAWSChannelConf(
		int64_t confKey,
		int64_t workspaceKey,
		string label,
		string channelId, string rtmpURL, string playURL, string type);

	void removeAWSChannelConf(
		int64_t workspaceKey,
		int64_t confKey);

	Json::Value getAWSChannelConfList (
        int64_t workspaceKey);

	tuple<string, string, string, bool> reserveAWSChannel(
		int64_t workspaceKey, string label, string type,
		int64_t ingestionJobKey);

	// tuple<string, string, string> getAWSChannelByIngestionJobKey(
	// 	int64_t workspaceKey, int64_t ingestionJobKey);

	string releaseAWSChannel(int64_t workspaceKey, int64_t ingestionJobKey);

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
        string addresses,
		string subject,
		string message);

    void modifyEMailConf(
        int64_t confKey,
        int64_t workspaceKey,
        string label,
        string addresses,
		string subject,
		string message);

    void removeEMailConf(
        int64_t workspaceKey,
        int64_t confKey);

    Json::Value getEMailConfList (
        int64_t workspaceKey);

    tuple<string, string, string> getEMailByConfigurationLabel(
        int64_t workspaceKey, string liveURLConfigurationLabel);
    
	Json::Value addRequestStatistic(
		int64_t workspaceKey,
		string userId,
		int64_t physicalPathKey,
		int64_t confStreamKey,
		string title);

	Json::Value getRequestStatisticList (
		int64_t workspaceKey,
		string userId,
		string title,
		string startDate, string endDate,
		int start, int rows);

	Json::Value getRequestStatisticPerContentList (
		int64_t workspaceKey,
		string title,
		string startDate, string endDate,
		int start, int rows);

	Json::Value getRequestStatisticPerMonthList (
		int64_t workspaceKey,
		string title,
		string startStatisticDate, string endStatisticDate,
		int start, int rows);

	Json::Value getRequestStatisticPerDayList (
		int64_t workspaceKey,
		string title,
		string startStatisticDate, string endStatisticDate,
		int start, int rows);

	Json::Value getRequestStatisticPerHourList (
		int64_t workspaceKey,
		string title,
		string startStatisticDate, string endStatisticDate,
		int start, int rows);

	void setLock(LockType lockType, int waitingTimeoutInSecondsIfLocked,
			string owner, string label,
			int milliSecondsToSleepWaitingLock = 500,
			string data = "no data");

	void releaseLock(LockType lockType, string label, string data = "no data");

	int64_t addEncoder(
		string label,
		bool external,
		bool enabled,
		string protocol,
		string publicServerName,
		string internalServerName,
		int port
	);

	void modifyEncoder(
		int64_t encoderKey,
		bool labelToBeModified, string label,
		bool externalToBeModified, bool external,
		bool enabledToBeModified, bool enabled,
		bool protocolToBeModified, string protocol,
		bool publicServerNameToBeModified, string publicServerName,
		bool internalServerNameToBeModified, string internalServerName,
		bool portToBeModified, int port
		// bool maxTranscodingCapabilityToBeModified, int maxTranscodingCapability,
		// bool maxLiveProxiesCapabilitiesToBeModified, int maxLiveProxiesCapabilities,
		// bool maxLiveRecordingCapabilitiesToBeModified, int maxLiveRecordingCapabilities
	);

	void removeEncoder(
		int64_t encoderKey);

	tuple<string, string, string> getEncoderDetails (int64_t encoderKey);

	bool isEncoderRunning(bool external, string protocol,
		string publicServerName, string internalServerName, int port);

	pair<bool, int> getEncoderInfo(bool external, string protocol,
		string publicServerName, string internalServerName, int port);

	void addAssociationWorkspaceEncoder(
		int64_t workspaceKey, int64_t encoderKey);

	void addAssociationWorkspaceEncoder(
		int64_t workspaceKey,
		string sharedEncodersPoolLabel, Json::Value sharedEncodersLabel);

	void removeAssociationWorkspaceEncoder(
		int64_t workspaceKey, int64_t encoderKey);

	Json::Value getEncoderList (
		int start, int rows,
		bool allEncoders, int64_t workspaceKey, bool runningInfo, int64_t encoderKey,
		string label, string serverName, int port,
		string labelOrder	// "" or "asc" or "desc"
	);

	string getEncodersPoolDetails (int64_t encodersPoolKey);

	Json::Value getEncodersPoolList (
		int start, int rows,
		int64_t workspaceKey, int64_t encodersPoolKey, string label,
		string labelOrder	// "" or "asc" or "desc"
	);

	tuple<int64_t, bool, string, string, string, int> getRunningEncoderByEncodersPool(
      int64_t workspaceKey, string encodersPoolLabel,
      int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed);

	int getEncodersNumberByEncodersPool(
      int64_t workspaceKey, string encodersPoolLabel);

	pair<string, bool> getEncoderURL(int64_t encoderKey, string serverName = "");

	int64_t addEncodersPool(
		int64_t workspaceKey,
		string label,
		vector<int64_t>& encoderKeys);

	int64_t modifyEncodersPool(
		int64_t encodersPoolKey,
		int64_t workspaceKey,
		string newLabel,
		vector<int64_t>& newEncoderKeys);

	void removeEncodersPool(
		int64_t encodersPoolKey);

	void addUpdatePartitionInfo(
		int partitionKey,
		string partitionName,
		uint64_t currentFreeSizeInBytes,
		int64_t freeSpaceToLeaveInMB);

	pair<int, uint64_t> getPartitionToBeUsedAndUpdateFreeSpace(
		int64_t ullFSEntrySizeInBytes);

	string getPartitionPathName(int partitionKey);

	uint64_t updatePartitionBecauseOfDeletion(int partitionKey, int64_t ullFSEntrySizeInBytes);

	void getPartitionsInfo(vector<pair<int, uint64_t>>& partitionsInfo);

	static int64_t parseRetention(string retention);

	Json::Value getStreamInputRoot(
		int64_t workspaceKey, string configurationLabel,
		int maxWidth, string userAgent, string otherInputOptions);

	Json::Value getVodInputRoot(
		MMSEngineDBFacade::ContentType vodContentType,
		vector<tuple<int64_t, string, string, int64_t>>& sources);

	Json::Value getCountdownInputRoot(
		string mmsSourceVideoAssetPathName,
		string mmsSourceVideoAssetDeliveryURL,
		int64_t physicalPathKey,
		int64_t videoDurationInMilliSeconds,
		Json::Value broadcastDrawTextDetailsRoot
	);

	Json::Value getDirectURLInputRoot(
		string url);

	bool oncePerDayExecution(OncePerDayType oncePerDayType);

	static MMSEngineDBFacade::DeliveryTechnology fileFormatToDeliveryTechnology(string fileFormat);

private:
    shared_ptr<spdlog::logger>                          _logger;
    shared_ptr<MySQLConnectionFactory>                  _mySQLConnectionFactory;
    shared_ptr<DBConnectionPool<MySQLConnection>>       _connectionPool;
    string                          _defaultContentProviderName;
    // string                          _defaultTerritoryName;
    int								_ingestionJobsSelectPageSize;
    int                             _maxEncodingFailures;
    int                             _confirmationCodeRetentionInDays;
    int                             _contentRetentionInMinutesDefaultValue;
	int								_contentNotTransferredRetentionInHours;

	int								_maxSecondsToWaitUpdateIngestionJobLock;
	int								_maxSecondsToWaitUpdateEncodingJobLock;
	int								_maxSecondsToWaitCheckIngestionLock;
	int								_maxSecondsToWaitCheckEncodingJobLock;
	int								_maxSecondsToWaitMainAndBackupLiveChunkLock;
	int								_maxSecondsToWaitSetNotToBeExecutedLock;
    
	int								_doNotManageIngestionsOlderThanDays;
	int								_ingestionWorkflowRetentionInDays;
	int								_statisticRetentionInMonths;

	string							_ffmpegEncoderUser;
	string							_ffmpegEncoderPassword;
	string							_ffmpegEncoderStatusURI;
	string							_ffmpegEncoderInfoURI;
	int								_ffmpegEncoderInfoTimeout;

    chrono::system_clock::time_point _lastConnectionStatsReport;
    int             _dbConnectionPoolStatsReportPeriodInSeconds;

	string			_predefinedWorkflowLibraryDirectoryPath;

    string          _predefinedVideoProfilesDirectoryPath;
    string          _predefinedAudioProfilesDirectoryPath;
    string          _predefinedImageProfilesDirectoryPath;

	vector<string>	_adminEmailAddresses;

	int				_getIngestionJobsCurrentIndex;
	int				_getEncodingJobsCurrentIndex;

	string createAPIKeyForActiveDirectoryUser(
		shared_ptr<MySQLConnection> conn,
		int64_t userKey,
		string userEmailAddress,
		bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
		bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
		bool applicationRecorder,
		int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue);

		void addWorkspaceForAdminUsers(
			shared_ptr<MySQLConnection> conn,
			int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
		);

	tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> isIngestionJobToBeManaged(
		int64_t ingestionJobKey,
		int64_t workspaceKey,
		IngestionStatus ingestionStatus,
		IngestionType ingestionType,
		shared_ptr<MySQLConnection> conn
		);

	void addIngestionJobOutput(
		shared_ptr<MySQLConnection> conn,
		int64_t ingestionJobKey,
		int64_t mediaItemKey,
		int64_t physicalPathKey,
		int64_t liveRecordingIngestionJobKey
	);

	int getIngestionTypePriority(MMSEngineDBFacade::IngestionType);

	int getEncodingTypePriority(MMSEngineDBFacade::EncodingType);

	pair<shared_ptr<sql::ResultSet>, int64_t> getMediaItemsList_withoutTagsCheck (
		shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey, int64_t mediaItemKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk,
		int64_t deliveryCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		string jsonCondition,
        string orderBy,
		string jsonOrderBy,
		bool admin);

	pair<shared_ptr<sql::ResultSet>, int64_t> getMediaItemsList_withTagsCheck (
		shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey, string temporaryTableName,
		int64_t mediaItemKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk, 
		int64_t deliveryCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		string jsonCondition,
		vector<string>& tagsIn, vector<string>& tagsNotIn,
        string orderBy,
		string jsonOrderBy,
		bool admin);

    void updateIngestionJob (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS = "noToBeUpdated");

    pair<int64_t,string> addWorkspace(
        shared_ptr<MySQLConnection> conn,
        int64_t userKey,
        bool admin,
		bool createRemoveWorkspace,
        bool ingestWorkflow,
        bool createProfiles,
        bool deliveryAuthorization,
        bool shareWorkspace,
        bool editMedia,
		bool editConfiguration,
		bool killEncoding,
		bool cancelIngestionJob,
		bool editEncodersPool,
		bool applicationRecorder,
        string workspaceName,
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInMB,
        string languageCode,
        chrono::system_clock::time_point userExpirationDate);

	void manageExternalUniqueName(
		shared_ptr<MySQLConnection> conn,
		int64_t workspaceKey,
		int64_t mediaItemKey,
		bool allowUniqueNameOverride,
		string uniqueName);

    int64_t saveVariantContentMetadata(
        shared_ptr<MySQLConnection> conn,
        
        int64_t workspaceKey,
		int64_t ingestionJobKey,
        int64_t liveRecordingIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
		int64_t physicalItemRetentionPeriodInMinutes,
        
        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,
		/*
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
		*/

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
    );

	int64_t addUpdateWorkflowAsLibrary(
		shared_ptr<MySQLConnection> conn,
		int64_t userKey,
		int64_t workspaceKey,
		string label,
		int64_t thumbnailMediaItemKey,
		string jsonWorkflow,
		bool admin);

	void addCrossReference (
        shared_ptr<MySQLConnection> conn,
		int64_t ingestionJobKey,
		int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType,
		int64_t targetMediaItemKey, Json::Value crossReferenceParametersRoot);

    Json::Value getIngestionJobRoot(
        shared_ptr<Workspace> workspace,
        shared_ptr<sql::ResultSet> resultSet,
		bool dependencyInfo,
		bool ingestionJobOutputs,
        shared_ptr<MySQLConnection> conn);

    void manageIngestionJobStatusUpdate (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
		bool updateIngestionRootStatus,
        shared_ptr<MySQLConnection> conn);

    pair<int64_t,int64_t> getWorkspaceUsage(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey);

	Json::Value getWorkspaceDetailsRoot (
		shared_ptr<MySQLConnection> conn,
		shared_ptr<sql::ResultSet> resultSet,
		bool userAPIKeyInfo
		// bool encoders
		);

	void addAssociationWorkspaceEncoder(
		int64_t workspaceKey, int64_t encoderKey,
		shared_ptr<MySQLConnection> conn);

	Json::Value getEncoderRoot (
		bool runningInfo,
		shared_ptr<sql::ResultSet> resultSet);

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

	void addTags(
		shared_ptr<MySQLConnection> conn,
		int64_t mediaItemKey,
		Json::Value tagsRoot);

	void removeTags(
		shared_ptr<MySQLConnection> conn,
		int64_t mediaItemKey);
};

#endif /* MMSEngineDBFacade_h */
