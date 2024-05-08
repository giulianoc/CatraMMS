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

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "Workspace.h"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#define DBCONNECTIONPOOL_LOG
#include "catralibraries/MySQLConnection.h"
#include "catralibraries/PostgresConnection.h"

namespace fs = std::filesystem;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename((char *)__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

using namespace std;

struct LoginFailed : public exception
{
	char const *what() const throw() { return "email and/or password are wrong"; };
};

struct APIKeyNotFoundOrExpired : public exception
{
	char const *what() const throw() { return "APIKey was not found or it is expired"; };
};

struct MediaItemKeyNotFound : public exception
{

	string _errorMessage;

	MediaItemKeyNotFound(string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct DeadlockFound : public exception
{

	string _errorMessage;

	DeadlockFound(string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct EncoderNotFound : public exception
{

	string _errorMessage;

	EncoderNotFound(string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct ConfKeyNotFound : public exception
{

	string _errorMessage;

	ConfKeyNotFound(string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct AlreadyLocked : public exception
{
	string _errorMessage;

	AlreadyLocked(string label, string owner, int currentLockDuration)
	{
		_errorMessage =
			string("Already locked") + ", label: " + label + ", owner: " + owner + ", currentLockDuration (secs): " + to_string(currentLockDuration);
	}

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct YouTubeURLNotRetrieved : public exception
{
	char const *what() const throw() { return "YouTube URL was not retrieved"; };
};

struct NoMoreSpaceInMMSPartition : public exception
{
	char const *what() const throw() { return "No more space in MMS Partitions"; };
};

class MMSEngineDBFacade
{

  public:
	enum class CodeType
	{
		UserRegistration = 0,
		ShareWorkspace = 1,
		UserRegistrationComingFromShareWorkspace = 2
	};
	static const char *toString(const CodeType &codeType)
	{
		switch (codeType)
		{
		case CodeType::UserRegistration:
			return "UserRegistration";
		case CodeType::ShareWorkspace:
			return "ShareWorkspace";
		case CodeType::UserRegistrationComingFromShareWorkspace:
			return "UserRegistrationComingFromShareWorkspace";
		default:
			throw runtime_error(string("Wrong CodeType"));
		}
	}
	static CodeType toCodeType(const string &codeType)
	{
		string lowerCase;
		lowerCase.resize(codeType.size());
		transform(codeType.begin(), codeType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "userregistration")
			return CodeType::UserRegistration;
		else if (lowerCase == "shareworkspace")
			return CodeType::ShareWorkspace;
		else if (lowerCase == "userregistrationcomingfromshareworkspace")
			return CodeType::UserRegistrationComingFromShareWorkspace;
		else
			throw runtime_error(string("Wrong CodeType") + ", current codeType: " + codeType);
	}

	enum class LockType
	{
		Ingestion = 0,
		Encoding = 1
	};
	static const char *toString(const LockType &lockType)
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
	static LockType toLockType(const string &lockType)
	{
		string lowerCase;
		lowerCase.resize(lockType.size());
		transform(lockType.begin(), lockType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "ingestion")
			return LockType::Ingestion;
		else if (lowerCase == "encoding")
			return LockType::Encoding;
		else
			throw runtime_error(string("Wrong LockType") + ", current lockType: " + lockType);
	}

	enum class OncePerDayType
	{
		DBDataRetention = 0,
		GEOInfo = 1
	};
	static const char *toString(const OncePerDayType &oncePerDayType)
	{
		switch (oncePerDayType)
		{
		case OncePerDayType::DBDataRetention:
			return "DBDataRetention";
		case OncePerDayType::GEOInfo:
			return "GEOInfo";
		default:
			throw runtime_error(string("Wrong OncePerDayType"));
		}
	}
	static OncePerDayType toOncePerDayType(const string &oncePerDayType)
	{
		string lowerCase;
		lowerCase.resize(oncePerDayType.size());
		transform(oncePerDayType.begin(), oncePerDayType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "dbdataretention")
			return OncePerDayType::DBDataRetention;
		if (lowerCase == "geoinfo")
			return OncePerDayType::GEOInfo;
		else
			throw runtime_error(string("Wrong OncePerDayType") + ", current oncePerDayType: " + oncePerDayType);
	}

	enum class ContentType
	{
		Video = 0,
		Audio = 1,
		Image = 2
		//		Application	= 3,
		//		Ringtone	= 4,
		//		Playlist	= 5,
		//		Live		= 6
	};
	static const char *toString(const ContentType &contentType)
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
	static ContentType toContentType(const string &contentType)
	{
		string lowerCase;
		lowerCase.resize(contentType.size());
		transform(contentType.begin(), contentType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "video")
			return ContentType::Video;
		else if (lowerCase == "audio")
			return ContentType::Audio;
		else if (lowerCase == "image")
			return ContentType::Image;
		else
			throw runtime_error(string("Wrong ContentType (correct value: video, audio or image") + ", current contentType: " + contentType);
	}

	enum class EncodingPriority
	{
		Low = 0,
		Medium = 1,
		High = 2
	};
	static const char *toString(const EncodingPriority &priority)
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
	static EncodingPriority toEncodingPriority(const string &priority)
	{
		string lowerCase;
		lowerCase.resize(priority.size());
		transform(priority.begin(), priority.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "low")
			return EncodingPriority::Low;
		else if (lowerCase == "medium")
			return EncodingPriority::Medium;
		else if (lowerCase == "high")
			return EncodingPriority::High;
		else
			throw runtime_error(string("Wrong EncodingPriority") + ", priority: " + priority);
	}

	enum class EncodingStatus
	{
		ToBeProcessed = 0,
		Processing = 1,
		End_Success = 2,
		End_Failed = 3,
		End_KilledByUser = 4,
		End_CanceledByUser = 5,
		End_CanceledByMMS = 6
	};
	static const char *toString(const EncodingStatus &encodingStatus)
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
			throw runtime_error(string("Wrong EncodingStatus: ") + to_string(static_cast<int>(encodingStatus)));
		}
	}
	static EncodingStatus toEncodingStatus(const string &encodingStatus)
	{
		string lowerCase;
		lowerCase.resize(encodingStatus.size());
		transform(encodingStatus.begin(), encodingStatus.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

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
			throw runtime_error(string("Wrong EncodingStatus") + ", encodingStatus: " + encodingStatus);
	}

	enum class EncodingType
	{
		EncodeVideoAudio = 0,
		EncodeImage = 1,
		OverlayImageOnVideo = 2,
		OverlayTextOnVideo = 3,
		GenerateFrames = 4,
		SlideShow = 5,
		FaceRecognition = 6,
		FaceIdentification = 7,
		LiveRecorder = 8,
		VideoSpeed = 9,
		PictureInPicture = 10,
		LiveProxy = 11,
		LiveGrid = 12,
		Countdown = 13,
		IntroOutroOverlay = 14,
		CutFrameAccurate = 15,
		VODProxy = 16,
		AddSilentAudio = 17
	};
	static const char *toString(const EncodingType &encodingType)
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
		case EncodingType::AddSilentAudio:
			return "AddSilentAudio";
		default:
			throw runtime_error(string("Wrong EncodingType"));
		}
	}
	static EncodingType toEncodingType(const string &encodingType)
	{
		string lowerCase;
		lowerCase.resize(encodingType.size());
		transform(encodingType.begin(), encodingType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

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
		else if (lowerCase == "addsilentaudio")
			return EncodingType::AddSilentAudio;
		else
			throw runtime_error(string("Wrong EncodingType") + ", encodingType: " + encodingType);
	}

	enum class EncodingError
	{
		NoError,
		PunctualError,
		MaxCapacityReached,
		ErrorBeforeEncoding,
		KilledByUser,
		CanceledByUser,
		CanceledByMMS
	};
	static const char *toString(const EncodingError &encodingError)
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
	static EncodingError toEncodingError(const string &encodingError)
	{
		string lowerCase;
		lowerCase.resize(encodingError.size());
		transform(encodingError.begin(), encodingError.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

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
			throw runtime_error(string("Wrong EncodingError") + ", encodingError: " + encodingError);
	}

	enum class DeliveryTechnology
	{
		Download,			  // image
		DownloadAndStreaming, // MP4,
		HTTPStreaming		  // HLS/DASH
							  // WEBM,               // (VP8 and Vorbis)
							  // WindowsMedia,
							  // MP3					// (Download),
	};
	static const char *toString(const DeliveryTechnology &deliveryTechnology)
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
	static DeliveryTechnology toDeliveryTechnology(const string &deliveryTechnology)
	{
		string lowerCase;
		lowerCase.resize(deliveryTechnology.size());
		transform(deliveryTechnology.begin(), deliveryTechnology.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "download")
			return DeliveryTechnology::Download;
		else if (lowerCase == "downloadandstreaming")
			return DeliveryTechnology::DownloadAndStreaming;
		else if (lowerCase == "httpstreaming")
			return DeliveryTechnology::HTTPStreaming;
		else
			throw runtime_error(string("Wrong DeliveryTechnology") + ", deliveryTechnology: " + deliveryTechnology);
	}

	enum class EncodingPeriod
	{
		Daily = 0,
		Weekly = 1,
		Monthly = 2,
		Yearly = 3
	};
	static const char *toString(const EncodingPeriod &encodingPeriod)
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
	static EncodingPeriod toEncodingPeriod(const string &encodingPeriod)
	{
		string lowerCase;
		lowerCase.resize(encodingPeriod.size());
		transform(encodingPeriod.begin(), encodingPeriod.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "daily")
			return EncodingPeriod::Daily;
		else if (lowerCase == "weekly")
			return EncodingPeriod::Weekly;
		else if (lowerCase == "monthly")
			return EncodingPeriod::Monthly;
		else if (lowerCase == "yearly")
			return EncodingPeriod::Yearly;
		else
			throw runtime_error(string("Wrong EncodingPeriod") + ", encodingPeriod: " + encodingPeriod);
	}

	enum class VideoSpeedType
	{
		SlowDown = 0,
		SpeedUp = 1
	};
	static const char *toString(const VideoSpeedType &videoSpeedType)
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
	static VideoSpeedType toVideoSpeedType(const string &videoSpeedType)
	{
		string lowerCase;
		lowerCase.resize(videoSpeedType.size());
		transform(videoSpeedType.begin(), videoSpeedType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "slowdown")
			return VideoSpeedType::SlowDown;
		else if (lowerCase == "speedup")
			return VideoSpeedType::SpeedUp;
		else
			throw runtime_error(string("Wrong VideoSpeedType") + ", videoSpeedType: " + videoSpeedType);
	}

	struct EncodingItem
	{
		long long _encodingJobKey;
		long long _ingestionJobKey;
		EncodingPriority _encodingPriority;
		EncodingType _encodingType;

		// Key of the Encoder used by this job:
		int64_t _encoderKey;
		string _stagingEncodedAssetPathName;

		json _ingestedParametersRoot;

		json _encodingParametersRoot;

		shared_ptr<Workspace> _workspace;
	};

	enum class WorkspaceType
	{
		LiveSessionOnly = 0,
		IngestionAndDelivery = 1,
		EncodingOnly = 2
	};

	enum class IngestionType
	{
		Unknown = 0, // in case json was not able to be parsed
		AddContent = 1,
		RemoveContent = 2,
		Encode = 3,
		Frame = 4,
		PeriodicalFrames = 5,
		IFrames = 6,
		MotionJPEGByPeriodicalFrames = 7,
		MotionJPEGByIFrames = 8,
		Slideshow = 9,
		ConcatDemuxer = 10,
		Cut = 11,
		OverlayImageOnVideo = 12,
		OverlayTextOnVideo = 13,
		FTPDelivery = 14,
		HTTPCallback = 15,
		LocalCopy = 16,
		ExtractTracks = 17,
		PostOnFacebook = 18,
		PostOnYouTube = 19,
		FaceRecognition = 20,
		FaceIdentification = 21,
		LiveRecorder = 22,
		ChangeFileFormat = 23,
		VideoSpeed = 24,
		PictureInPicture = 25,
		LiveProxy = 26,
		LiveCut = 27,
		LiveGrid = 28,
		Countdown = 29,
		IntroOutroOverlay = 30,
		VODProxy = 31,
		YouTubeLiveBroadcast = 32,
		FacebookLiveBroadcast = 33,
		AddSilentAudio = 34,

		EmailNotification = 60,
		MediaCrossReference = 61,
		WorkflowAsLibrary = 62,
		CheckStreaming = 63,

		ContentUpdate = 80,
		ContentRemove = 90,
		GroupOfTasks = 100
	};
	static const char *toString(const IngestionType &ingestionType)
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
		case IngestionType::FacebookLiveBroadcast:
			return "Facebook-Live-Broadcast";
		case IngestionType::AddSilentAudio:
			return "Add-Silent-Audio";

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
	static IngestionType toIngestionType(const string &ingestionType)
	{
		string lowerCase;
		lowerCase.resize(ingestionType.size());
		transform(ingestionType.begin(), ingestionType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

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
		else if (lowerCase == "facebook-live-broadcast")
			return IngestionType::FacebookLiveBroadcast;
		else if (lowerCase == "add-silent-audio")
			return IngestionType::AddSilentAudio;

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
			throw runtime_error(string("Wrong IngestionType") + ", ingestionType: " + ingestionType);
	}

	enum class IngestionStatus
	{
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

		End_IngestionFailure, // nothing done

		End_NotToBeExecuted,				  // because of dependencies
		End_NotToBeExecuted_ChunkNotSelected, // because of dependencies

		End_TaskSuccess
	};
	static const char *toString(const IngestionStatus &ingestionStatus)
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
	static IngestionStatus toIngestionStatus(const string &ingestionStatus)
	{
		string lowerCase;
		lowerCase.resize(ingestionStatus.size());
		transform(ingestionStatus.begin(), ingestionStatus.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

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
			throw runtime_error(string("Wrong IngestionStatus") + ", ingestionStatus: " + ingestionStatus);
	}
	static bool isIngestionStatusFinalState(const IngestionStatus &ingestionStatus)
	{
		string prefix = "End";
		string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);

		return (sIngestionStatus.size() >= prefix.size() && 0 == sIngestionStatus.compare(0, prefix.size(), prefix));
	}
	static bool isIngestionStatusSuccess(const IngestionStatus &ingestionStatus)
	{
		return (
			ingestionStatus == IngestionStatus::End_TaskSuccess || ingestionStatus == IngestionStatus::End_NotToBeExecuted ||
			ingestionStatus == IngestionStatus::End_NotToBeExecuted_ChunkNotSelected
		);
	}
	static bool isIngestionStatusFailed(const IngestionStatus &ingestionStatus)
	{
		return (isIngestionStatusFinalState(ingestionStatus) && !isIngestionStatusSuccess(ingestionStatus));
	}

	enum class IngestionRootStatus
	{
		NotCompleted,
		CompletedSuccessful,
		CompletedWithFailures
	};
	static const char *toString(const IngestionRootStatus &ingestionRootStatus)
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
	static IngestionRootStatus toIngestionRootStatus(const string &ingestionRootStatus)
	{
		string lowerCase;
		lowerCase.resize(ingestionRootStatus.size());
		transform(ingestionRootStatus.begin(), ingestionRootStatus.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "notcompleted")
			return IngestionRootStatus::NotCompleted;
		else if (lowerCase == "completedsuccessful")
			return IngestionRootStatus::CompletedSuccessful;
		else if (lowerCase == "completedwithfailures")
			return IngestionRootStatus::CompletedWithFailures;
		else
			throw runtime_error(string("Wrong IngestionRootStatus") + ", ingestionRootStatus: " + ingestionRootStatus);
	}

	enum class CrossReferenceType
	{
		ImageOfVideo,
		VideoOfImage, // will be converted to ImageOfVideo
		ImageOfAudio,
		AudioOfImage, // will be converted to ImageOfAudio
		FaceOfVideo,
		VideoOfFace, // will be converted to FaceOfVideo
		CutOfVideo,
		CutOfAudio,
		PosterOfVideo,
		VideoOfPoster // will be converted to PosterOfVideo
	};
	static const char *toString(const CrossReferenceType &crossReferenceType)
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
		case CrossReferenceType::VideoOfFace:
			return "VideoOfFace";
		case CrossReferenceType::CutOfVideo:
			return "CutOfVideo";
		case CrossReferenceType::CutOfAudio:
			return "CutOfAudio";
		case CrossReferenceType::PosterOfVideo:
			return "PosterOfVideo";
		case CrossReferenceType::VideoOfPoster:
			return "VideoOfPoster";
		default:
			throw runtime_error(string("Wrong CrossReferenceType"));
		}
	}
	static CrossReferenceType toCrossReferenceType(const string &crossReferenceType)
	{
		string lowerCase;
		lowerCase.resize(crossReferenceType.size());
		transform(crossReferenceType.begin(), crossReferenceType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

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
		else if (lowerCase == "videoofface")
			return CrossReferenceType::VideoOfFace;
		else if (lowerCase == "cutofvideo")
			return CrossReferenceType::CutOfVideo;
		else if (lowerCase == "cutofaudio")
			return CrossReferenceType::CutOfAudio;
		else if (lowerCase == "posterofvideo")
			return CrossReferenceType::PosterOfVideo;
		else if (lowerCase == "videoofposter")
			return CrossReferenceType::VideoOfPoster;
		else
			throw runtime_error(string("Wrong CrossReferenceType") + ", crossReferenceType: " + crossReferenceType);
	}

  public:
	MMSEngineDBFacade(json configuration, size_t masterDbPoolSize, size_t slaveDbPoolSize, shared_ptr<spdlog::logger> logger);

	~MMSEngineDBFacade();

	// vector<shared_ptr<Customer>> getCustomers();

	shared_ptr<Workspace> getWorkspace(int64_t workspaceKey);

	shared_ptr<Workspace> getWorkspace(string workspaceName);

	json getWorkspaceList(int64_t userKey, bool admin, bool costDetails);

	json getLoginWorkspace(int64_t userKey, bool fromMaster);

	int64_t addInvoice(int64_t userKey, string description, int amount, string expirationDate);

	json getInvoicesList(int64_t userKey, bool admin, int start, int rows);

	string createCode(
		int64_t workspaceKey, int64_t userKey, string userEmail, CodeType codeType, bool admin, bool createRemoveWorkspace, bool ingestWorkflow,
		bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding,
		bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder
	);

#ifdef __POSTGRES__
	tuple<int64_t, int64_t, string> registerUserAndAddWorkspace(
		string userName, string userEmailAddress, string userPassword, string userCountry, string userTimezone, string workspaceName,
		WorkspaceType workspaceType, string deliveryURL, EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod,
		long maxIngestionsNumber, long maxStorageInMB, string languageCode, chrono::system_clock::time_point userExpirationDate
	);
#else
	tuple<int64_t, int64_t, string> registerUserAndAddWorkspace(
		string userName, string userEmailAddress, string userPassword, string userCountry, string workspaceName, WorkspaceType workspaceType,
		string deliveryURL, EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod, long maxIngestionsNumber, long maxStorageInMB,
		string languageCode, chrono::system_clock::time_point userExpirationDate
	);
#endif

#ifdef __POSTGRES__
	tuple<int64_t, int64_t, string> registerUserAndShareWorkspace(
		string userName, string userEmailAddress, string userPassword, string userCountry, string userTimezone, string shareWorkspaceCode,
		chrono::system_clock::time_point userExpirationDate
	);
#else
	tuple<int64_t, int64_t, string> registerUserAndShareWorkspace(
		string userName, string userEmailAddress, string userPassword, string userCountry, string shareWorkspaceCode,
		chrono::system_clock::time_point userExpirationDate
	);
#endif

	pair<int64_t, string> createWorkspace(
		int64_t userKey, string workspaceName, WorkspaceType workspaceType, string deliveryURL, EncodingPriority maxEncodingPriority,
		EncodingPeriod encodingPeriod, long maxIngestionsNumber, long maxStorageInMB, string languageCode, bool admin,
		chrono::system_clock::time_point userExpirationDate
	);

	vector<tuple<int64_t, string, string>> deleteWorkspace(int64_t userKey, int64_t workspaceKey);
#ifdef __POSTGRES__
	tuple<bool, string, string> unshareWorkspace(int64_t userKey, int64_t workspaceKey);
#endif

	tuple<string, string, string> confirmRegistration(string confirmationCode, int expirationInDaysWorkspaceDefaultValue);

#ifdef __POSTGRES__
	pair<int64_t, string> registerActiveDirectoryUser(
		string userName, string userEmailAddress, string userCountry, string userTimezone, bool createRemoveWorkspace, bool ingestWorkflow,
		bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding,
		bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder, string defaultWorkspaceKeys,
		int expirationInDaysWorkspaceDefaultValue, chrono::system_clock::time_point userExpirationDate
	);
#else
	pair<int64_t, string> registerActiveDirectoryUser(
		string userName, string userEmailAddress, string userCountry, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles,
		bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob,
		bool editEncodersPool, bool applicationRecorder, string defaultWorkspaceKeys, int expirationInDaysWorkspaceDefaultValue,
		chrono::system_clock::time_point userExpirationDate
	);
#endif

	string createAPIKeyForActiveDirectoryUser(
		int64_t userKey, string userEmailAddress, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
		bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
		bool applicationRecorder, int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
	);

	pair<string, string> getUserDetails(int64_t userKey);
#ifdef __POSTGRES__
	pair<int64_t, string> getUserDetailsByEmail(string email, bool warningIfError);
#else
	pair<int64_t, string> getUserDetailsByEmail(string email);
#endif

	tuple<int64_t, shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>
	checkAPIKey(string apiKey, bool fromMaster);

	json login(string eMailAddress, string password);

	int64_t saveLoginStatistics(int userKey, string ip);

#ifdef __POSTGRES__
	void saveGEOInfo(string ipAddress, transaction_base *trans, shared_ptr<PostgresConnection> conn);
	json getGEOInfo(string ip);
	void updateGEOInfo();
	vector<tuple<string, string, string, string, string, string, string, string, string>> getGEOInfo_ipAPI(vector<string> &ips);
	vector<tuple<string, string, string, string, string, string, string, string, string>> getGEOInfo_ipwhois(vector<string> &ips);
#else
#endif

	pair<int64_t, int64_t> getWorkspaceUsage(int64_t workspaceKey);

#ifdef __POSTGRES__
	json getWorkspaceCost(shared_ptr<PostgresConnection> conn, nontransaction &trans, int64_t workspaceKey);
#else
#endif

#ifdef __POSTGRES__
	json updateWorkspaceDetails(
		int64_t userKey, int64_t workspaceKey, bool enabledChanged, bool newEnabled, bool nameChanged, string newName,
		bool maxEncodingPriorityChanged, string newMaxEncodingPriority, bool encodingPeriodChanged, string newEncodingPeriod,
		bool maxIngestionsNumberChanged, int64_t newMaxIngestionsNumber, bool languageCodeChanged, string newLanguageCode, bool expirationDateChanged,
		string newExpirationDate,

		bool maxStorageInGBChanged, int64_t maxStorageInGB, bool currentCostForStorageChanged, int64_t currentCostForStorage,
		bool dedicatedEncoder_power_1Changed, int64_t dedicatedEncoder_power_1, bool currentCostForDedicatedEncoder_power_1Changed,
		int64_t currentCostForDedicatedEncoder_power_1, bool dedicatedEncoder_power_2Changed, int64_t dedicatedEncoder_power_2,
		bool currentCostForDedicatedEncoder_power_2Changed, int64_t currentCostForDedicatedEncoder_power_2, bool dedicatedEncoder_power_3Changed,
		int64_t dedicatedEncoder_power_3, bool currentCostForDedicatedEncoder_power_3Changed, int64_t currentCostForDedicatedEncoder_power_3,
		bool CDN_type_1Changed, int64_t CDN_type_1, bool currentCostForCDN_type_1Changed, int64_t currentCostForCDN_type_1,
		bool support_type_1Changed, bool support_type_1, bool currentCostForSupport_type_1Changed, int64_t currentCostForSupport_type_1,

		bool newCreateRemoveWorkspace, bool newIngestWorkflow, bool newCreateProfiles, bool newDeliveryAuthorization, bool newShareWorkspace,
		bool newEditMedia, bool newEditConfiguration, bool newKillEncoding, bool newCancelIngestionJob, bool newEditEncodersPool,
		bool newApplicationRecorder
	);
#else
	json updateWorkspaceDetails(
		int64_t userKey, int64_t workspaceKey, bool enabledChanged, bool newEnabled, bool nameChanged, string newName,
		bool maxEncodingPriorityChanged, string newMaxEncodingPriority, bool encodingPeriodChanged, string newEncodingPeriod,
		bool maxIngestionsNumberChanged, int64_t newMaxIngestionsNumber, bool maxStorageInMBChanged, int64_t newMaxStorageInMB,
		bool languageCodeChanged, string newLanguageCode, bool expirationDateChanged, string newExpirationDate, bool newCreateRemoveWorkspace,
		bool newIngestWorkflow, bool newCreateProfiles, bool newDeliveryAuthorization, bool newShareWorkspace, bool newEditMedia,
		bool newEditConfiguration, bool newKillEncoding, bool newCancelIngestionJob, bool newEditEncodersPool, bool newApplicationRecorder
	);
#endif

	json setWorkspaceAsDefault(int64_t userKey, int64_t workspaceKey, int64_t workspaceKeyToBeSetAsDefault);

#ifdef __POSTGRES__
	json updateUser(
		bool admin, bool ldapEnabled, int64_t userKey, bool nameChanged, string name, bool emailChanged, string email, bool countryChanged,
		string country, bool timezoneChanged, string timezone, bool insolventChanged, bool insolvent, bool expirationDateChanged,
		string expirationDate, bool passwordChanged, string newPassword, string oldPassword
	);
#else
	json updateUser(
		bool admin, bool ldapEnabled, int64_t userKey, bool nameChanged, string name, bool emailChanged, string email, bool countryChanged,
		string country, bool insolventChanged, bool insolvent, bool expirationDateChanged, string expirationDate, bool passwordChanged,
		string newPassword, string oldPassword
	);
#endif

	string createResetPasswordToken(int64_t userKey);

	pair<string, string> resetPassword(string resetPassworkToken, string newPassword);

	void addIngestionJobOutput(int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey);

	long getIngestionJobOutputsCount(int64_t ingestionJobKey, bool fromMaster);

	tuple<int64_t, int64_t, string> getEncodingJobDetailsByIngestionJobKey(int64_t ingestionJobKey, bool fromMaster);

#ifdef __POSTGRES__
	int64_t addEncodingProfilesSetIfNotAlreadyPresent(
		transaction_base *trans, shared_ptr<PostgresConnection> conn, int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, string label,
		bool removeEncodingProfilesIfPresent
	);
#else
	int64_t addEncodingProfilesSetIfNotAlreadyPresent(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, string label,
		bool removeEncodingProfilesIfPresent
	);
#endif

#ifdef __POSTGRES__
	int64_t addEncodingProfile(
		nontransaction &trans, shared_ptr<PostgresConnection> conn, int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType,
		DeliveryTechnology deliveryTechnology, string jsonProfile, int64_t encodingProfilesSetKey
	);
#else
	int64_t addEncodingProfile(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType,
		DeliveryTechnology deliveryTechnology, string jsonProfile,
		int64_t encodingProfilesSetKey // -1 if it is not associated to any Set
	);
#endif

	int64_t addEncodingProfile(
		int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType, DeliveryTechnology deliveryTechnology,
		string jsonEncodingProfile
	);

	void removeEncodingProfile(int64_t workspaceKey, int64_t encodingProfileKey);

#ifdef __POSTGRES__
	int64_t addEncodingProfileIntoSetIfNotAlreadyPresent(
		transaction_base *trans, shared_ptr<PostgresConnection> conn, int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType,
		int64_t encodingProfilesSetKey
	);
#else
	int64_t addEncodingProfileIntoSetIfNotAlreadyPresent(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType,
		int64_t encodingProfilesSetKey
	);
#endif

	void removeEncodingProfilesSet(int64_t workspaceKey, int64_t encodingProfilesSetKey);

	void getExpiredMediaItemKeysCheckingDependencies(
		string processorMMS, vector<tuple<shared_ptr<Workspace>, int64_t, int64_t>> &mediaItemKeyOrPhysicalPathKeyToBeRemoved,
		int maxMediaItemKeysNumber
	);

	int getNotFinishedIngestionDependenciesNumberByIngestionJobKey(int64_t ingestionJobKey, bool fromMaster);

#ifdef __POSTGRES__
	int
	getNotFinishedIngestionDependenciesNumberByIngestionJobKey(shared_ptr<PostgresConnection> conn, nontransaction &trans, int64_t ingestionJobKey);
#else
	int getNotFinishedIngestionDependenciesNumberByIngestionJobKey(shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey);
#endif

	void getIngestionsToBeManaged(
		vector<tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType, IngestionStatus>> &ingestionsToBeManaged,
		string processorMMS, int maxIngestionJobs, int timeBeforeToPrepareResourcesInMinutes, bool onlyTasksNotInvolvingMMSEngineThreads
	);

	void setNotToBeExecutedStartingFromBecauseChunkNotSelected(int64_t ingestionJobKey, string processorMMS);

	// void manageMainAndBackupOfRunnungLiveRecordingHA(string processorMMS);

	// bool liveRecorderMainAndBackupChunksManagementCompleted(
	// 	int64_t ingestionJobKey);

	// void getRunningLiveRecorderVirtualVODsDetails(
	// 	vector<tuple<int64_t, int64_t, int, string, int, string, string,
	// int64_t, 	string>>& runningLiveRecordersDetails
	// );

#ifdef __POSTGRES__
	shared_ptr<PostgresConnection> beginIngestionJobs();
#else
	shared_ptr<MySQLConnection> beginIngestionJobs();
#endif

#ifdef __POSTGRES__
	int64_t addIngestionRoot(
		shared_ptr<PostgresConnection> conn, work &trans, int64_t workspaceKey, int64_t userKey, string rootType, string rootLabel,
		string metaDataContent
	);
#else
	int64_t addIngestionRoot(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t userKey, string rootType, string rootLabel, string metaDataContent
	);
#endif

#ifdef __POSTGRES__
	void addIngestionJobDependency(
		shared_ptr<PostgresConnection> conn, work &trans, int64_t ingestionJobKey, int dependOnSuccess, int64_t dependOnIngestionJobKey,
		int orderNumber, bool referenceOutputDependency
	);
#else
	void addIngestionJobDependency(
		shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int dependOnSuccess, int64_t dependOnIngestionJobKey, int orderNumber,
		bool referenceOutputDependency
	);
#endif

	void changeIngestionJobDependency(int64_t previousDependOnIngestionJobKey, int64_t newDependOnIngestionJobKey);

#ifdef __POSTGRES__
	int64_t addIngestionJob(
		shared_ptr<PostgresConnection> conn, work &trans, int64_t workspaceKey, int64_t ingestionRootKey, string label, string metadataContent,
		MMSEngineDBFacade::IngestionType ingestionType, string processingStartingFrom, vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
		vector<int64_t> waitForGlobalIngestionJobKeys
	);
#else
	int64_t addIngestionJob(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t ingestionRootKey, string label, string metadataContent,
		MMSEngineDBFacade::IngestionType ingestionType, string processingStartingFrom, vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
		vector<int64_t> waitForGlobalIngestionJobKeys
	);
#endif

	void getIngestionJobsKeyByGlobalLabel(int64_t workspaceKey, string globalIngestionLabel, bool fromMaster, vector<int64_t> &ingestionJobsKey);

	void updateIngestionJobMetadataContent(int64_t ingestionJobKey, string metadataContent);

#ifdef __POSTGRES__
	void updateIngestionJobMetadataContent(shared_ptr<PostgresConnection>, nontransaction &trans, int64_t ingestionJobKey, string metadataContent);
#else
	void updateIngestionJobMetadataContent(shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, string metadataContent);
#endif

#ifdef __POSTGRES__
	void updateIngestionJobParentGroupOfTasks(
		shared_ptr<PostgresConnection> conn, work &trans, int64_t ingestionJobKey, int64_t parentGroupOfTasksIngestionJobKey
	);
#else
	void updateIngestionJobParentGroupOfTasks(shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int64_t parentGroupOfTasksIngestionJobKey);
#endif

	void updateIngestionJob_LiveRecorder(
		int64_t workspaceKey, int64_t ingestionJobKey, bool ingestionJobLabelModified, string newIngestionJobLabel, bool channelLabelModified,
		string newChannerLabel, bool recordingPeriodStartModified, string newRecordingPeriodStart, bool recordingPeriodEndModified,
		string newRecordingPeriodEnd, bool recordingVirtualVODModified, bool newRecordingVirtualVOD, bool admin
	);

	/*
	void getGroupOfTasksChildrenStatus(
		int64_t groupOfTasksIngestionJobKey,
		bool fromMaster,
		vector<pair<int64_t, MMSEngineDBFacade::IngestionStatus>>&
	groupOfTasksChildrenStatus);
	*/

#ifdef __POSTGRES__
	void endIngestionJobs(shared_ptr<PostgresConnection> conn, work &trans, bool commit, int64_t ingestionRootKey, string processedMetadataContent);
#else
	shared_ptr<MySQLConnection>
	endIngestionJobs(shared_ptr<MySQLConnection> conn, bool commit, int64_t ingestionRootKey, string processedMetadataContent);
#endif

	void updateIngestionJob(int64_t ingestionJobKey, IngestionStatus newIngestionStatus, string errorMessage, string processorMMS = "noToBeUpdated");

	void appendIngestionJobErrorMessage(int64_t ingestionJobKey, string errorMessage);

	bool updateIngestionJobSourceDownloadingInProgress(int64_t ingestionJobKey, double downloadingPercentage);

	bool updateIngestionJobSourceUploadingInProgress(int64_t ingestionJobKey, double uploadingPercentage);

	void updateIngestionJobSourceBinaryTransferred(int64_t ingestionJobKey, bool sourceBinaryTransferred);

	string getIngestionRootMetaDataContent(shared_ptr<Workspace> workspace, int64_t ingestionRootKey, bool processedMetadata, bool fromMaster);

	tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string>
	getIngestionJobDetails(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);

	json getIngestionRootsStatus(
		shared_ptr<Workspace> workspace, int64_t ingestionRootKey, int64_t mediaItemKey, int start, int rows,
		// bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate, string label, string status, bool asc, bool dependencyInfo, bool ingestionJobOutputs,
		bool fromMaster
	);

	json getIngestionJobsStatus(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int start, int rows, string label, bool labelLike,
		/* bool startAndEndIngestionDatePresent, */ string startIngestionDate, string endIngestionDate, string startScheduleDate,
		string ingestionType, string configurationLabel, string outputChannelLabel, int64_t recordingCode, bool broadcastIngestionJobKeyNotNull,
		string jsonParametersCondition, bool asc, string status, bool dependencyInfo, bool ingestionJobOutputs, bool fromMaster
	);

	json getEncodingJobsStatus(
		shared_ptr<Workspace> workspace, int64_t encodingJobKey, int start, int rows,
		// bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
		// bool startAndEndEncodingDatePresent,
		string startEncodingDate, string endEncodingDate, int64_t encoderKey, bool alsoEncodingJobsFromOtherWorkspaces, bool asc, string status,
		string types, bool fromMaster
	);

#ifdef __POSTGRES__
	json updateMediaItem(
		int64_t workspaceKey, int64_t mediaItemKey, bool titleModified, string newTitle, bool userDataModified, string newUserData,
		bool retentionInMinutesModified, int64_t newRetentionInMinutes, bool tagsModified, json tagsRoot, bool uniqueNameModified,
		string newUniqueName, json crossReferencesRoot, bool admin
	);
#else
	json updateMediaItem(
		int64_t workspaceKey, int64_t mediaItemKey, bool titleModified, string newTitle, bool userDataModified, string newUserData,
		bool retentionInMinutesModified, int64_t newRetentionInMinutes, bool tagsModified, json tagsRoot, bool uniqueNameModified,
		string newUniqueName, bool admin
	);
#endif

	json updatePhysicalPath(int64_t workspaceKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t newRetentionInMinutes, bool admin);

	json getMediaItemsList(
		int64_t workspaceKey, int64_t mediaItemKey, string uniqueName, int64_t physicalPathKey, vector<int64_t> &otherMediaItemsKey, int start,
		int rows, bool contentTypePresent, ContentType contentType,
		// bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate, string title, int liveRecordingChunk, int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, string jsonCondition,
		vector<string> &tagsIn, vector<string> &tagsNotIn, string orderBy, string jsonOrderBy, set<string> &responseFields, bool admin,
		bool fromMaster
	);

	json getTagsList(
		int64_t workspaceKey, int start, int rows, int liveRecordingChunk, bool contentTypePresent, ContentType contentType, string tagNameFilter,
		bool fromMaster
	);

	void updateMediaItem(int64_t mediaItemKey, string processorMMSForRetention);

	int64_t
	addUpdateWorkflowAsLibrary(int64_t userKey, int64_t workspaceKey, string label, int64_t thumbnailMediaItemKey, string jsonWorkflow, bool admin);

	void removeWorkflowAsLibrary(int64_t userKey, int64_t workspaceKey, int64_t workflowLibraryKey, bool admin);

	json getWorkflowsAsLibraryList(int64_t workspaceKey);

	string getWorkflowAsLibraryContent(int64_t workspaceKey, int64_t workflowLibraryKey);

	string getWorkflowAsLibraryContent(int64_t workspaceKey, string label);

	json getEncodingProfilesSetList(int64_t workspaceKey, int64_t encodingProfilesSetKey, bool contentTypePresent, ContentType contentType);

	json getEncodingProfileList(int64_t workspaceKey, int64_t encodingProfileKey, bool contentTypePresent, ContentType contentType, string label);

	int64_t getPhysicalPathDetails(int64_t referenceMediaItemKey, int64_t encodingProfileKey, bool warningIfMissing, bool fromMaster);

	int64_t getPhysicalPathDetails(
		int64_t workspaceKey, int64_t mediaItemKey, ContentType contentType, string encodingProfileLabel, bool warningIfMissing, bool fromMaster
	);

	string getPhysicalPathDetails(int64_t physicalPathKey, bool warningIfMissing, bool fromMaster);

	tuple<int64_t, int, string, string, uint64_t, bool, int64_t> getSourcePhysicalPath(int64_t mediaItemKey, bool warningIfMissing, bool fromMaster);

	tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
	getMediaItemKeyDetails(int64_t workspaceKey, int64_t mediaItemKey, bool warningIfMissing, bool fromMaster);

	tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
	getMediaItemKeyDetailsByPhysicalPathKey(int64_t workspaceKey, int64_t physicalPathKey, bool warningIfMissing, bool fromMaster);

	void getMediaItemDetailsByIngestionJobKey(
		int64_t workspaceKey, int64_t referenceIngestionJobKey, int maxLastMediaItemsToBeReturned,
		vector<tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType>> &mediaItemsDetails, bool warningIfMissing, bool fromMaster
	);

	pair<int64_t, MMSEngineDBFacade::ContentType>
	getMediaItemKeyDetailsByUniqueName(int64_t workspaceKey, string referenceUniqueName, bool warningIfMissing, bool fromMaster);

	int64_t getMediaDurationInMilliseconds(int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster);

	// tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
	// getVideoDetails(
	//     int64_t mediaItemKey, int64_t physicalpathKey);
	void getVideoDetails(
		int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster,
		vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> &videoTracks,
		vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> &audioTracks
	);

	// tuple<int64_t,string,long,long,int> getAudioDetails(
	//     int64_t mediaItemKey, int64_t physicalpathKey);
	void getAudioDetails(
		int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster,
		vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> &audioTracks
	);

	tuple<int, int, string, int> getImageDetails(int64_t mediaItemKey, int64_t physicalpathKey, bool fromMaster);

	vector<int64_t> getEncodingProfileKeysBySetKey(int64_t workspaceKey, int64_t encodingProfilesSetKey);

	vector<int64_t> getEncodingProfileKeysBySetLabel(int64_t workspaceKey, string label);

	tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string>
	getEncodingProfileDetailsByKey(int64_t workspaceKey, int64_t encodingProfileKey);

	tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, uint64_t, bool>
	getStorageDetails(int64_t physicalPathKey, bool fromMaster);

	tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, uint64_t, bool>
	getStorageDetails(int64_t mediaItemKey, int64_t encodingProfileKey, bool warningIfMissing, bool fromMaster);

	void getAllStorageDetails(
		int64_t mediaItemKey, bool fromMaster,
		vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>> &allStorageDetails
	);

	int64_t createDeliveryAuthorization(
		int64_t userKey, string clientIPAddress, int64_t physicalPathKey, int64_t liveDeliveryKey, string deliveryURI, int ttlInSeconds,
		int maxRetries
	);

	bool checkDeliveryAuthorization(int64_t deliveryAuthorizationKey, string contentURI);

	void resetProcessingJobsIfNeeded(string processorMMS);

	void retentionOfIngestionData();

	void retentionOfStatisticData();

	void retentionOfDeliveryAuthorization();

	void fixEncodingJobsHavingWrongStatus();
	void fixIngestionJobsHavingWrongStatus();

	void getEncodingJobs(
		string processorMMS, vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> &encodingItems, int timeBeforeToPrepareResourcesInMinutes,
		int maxEncodingsNumber
	);

	int64_t getEncodingProfileKeyByLabel(
		int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, string encodingProfileLabel, bool contentTypeToBeUsed = true
	);

	void addEncodingJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, MMSEngineDBFacade::ContentType contentType, EncodingPriority encodingPriority,
		int64_t encodingProfileKey, json encodingProfileDetailsRoot,

		json sourcesToBeEncodedRoot,

		string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
	);

	void addEncoding_OverlayImageOnVideoJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t encodingProfileKey, json encodingProfileDetailsRoot,
		int64_t sourceVideoMediaItemKey, int64_t sourceVideoPhysicalPathKey, int64_t videoDurationInMilliSeconds, string mmsSourceVideoAssetPathName,
		string sourceVideoPhysicalDeliveryURL, string sourceVideoFileExtension, int64_t sourceImageMediaItemKey, int64_t sourceImagePhysicalPathKey,
		string mmsSourceImageAssetPathName, string sourceImagePhysicalDeliveryURL, string sourceVideoTranscoderStagingAssetPathName,
		string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName, EncodingPriority encodingPriority,
		string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
	);

	void addEncoding_OverlayTextOnVideoJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, EncodingPriority encodingPriority,

		int64_t encodingProfileKey, json encodingProfileDetailsRoot,

		string sourceAssetPathName, int64_t sourceDurationInMilliSeconds, string sourcePhysicalDeliveryURL, string sourceFileExtension,

		string sourceTranscoderStagingAssetPathName, string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
		string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
	);

	void addEncoding_GenerateFramesJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, EncodingPriority encodingPriority, string nfsImagesDirectory,
		string transcoderStagingImagesDirectory, string sourcePhysicalDeliveryURL, string sourceTranscoderStagingAssetPathName,
		string sourceAssetPathName, int64_t sourceVideoPhysicalPathKey, string sourceFileExtension, string sourceFileName,
		int64_t videoDurationInMilliSeconds, double startTimeInSeconds, int maxFramesNumber, string videoFilter, int periodInSeconds, bool mjpeg,
		int imageWidth, int imageHeight, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
	);

	void addEncoding_SlideShowJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t encodingProfileKey, json encodingProfileDetailsRoot,
		string targetFileFormat, json imagesRoot, json audiosRoot, float shortestAudioDurationInSeconds, string encodedTranscoderStagingAssetPathName,
		string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,
		EncodingPriority encodingPriority
	);

	void addEncoding_FaceRecognitionJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t sourceMediaItemKey, int64_t sourceVideoPhysicalPathKey,
		string sourcePhysicalPath, string faceRecognitionCascadeName, string faceRecognitionOutput, EncodingPriority encodingPriority,
		long initialFramesNumberToBeSkipped, bool oneFramePerSecond
	);

	void addEncoding_FaceIdentificationJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, string sourcePhysicalPath, string faceIdentificationCascadeName,
		string deepLearnedModelTagsCommaSeparated, EncodingPriority encodingPriority
	);

	void addEncoding_LiveRecorderJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, string ingestionJobLabel, string streamSourceType,
		// bool highAvailability,
		string configurationLabel, int64_t confKey, string url, string encodersPoolLabel, EncodingPriority encodingPriority,

		int pushListenTimeout, int64_t pushEncoderKey, string pushEncoderName, json captureRoot, json tvRoot,

		bool monitorHLS, bool liveRecorderVirtualVOD, int monitorVirtualVODOutputRootIndex,

		json outputsRoot, json framesToBeDetectedRoot,

		string chunksTranscoderStagingContentsPath, string chunksNFSStagingContentsPath, string segmentListFileName, string recordedFileNamePrefix,
		string virtualVODStagingContentsPath, string virtualVODTranscoderStagingContentsPath, int64_t liveRecorderVirtualVODImageMediaItemKey,

		string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL
	);

	void addEncoding_LiveProxyJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputsRoot, string streamSourceType,
		int64_t utcProxyPeriodStart, // int64_t utcProxyPeriodEnd,
		// long maxAttemptsNumberInCaseOfErrors,
		long waitingSecondsBetweenAttemptsInCaseOfErrors, json outputsRoot, string mmsWorkflowIngestionURL
	);

	void addEncoding_VODProxyJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputsRoot, int64_t utcProxyPeriodStart, json outputsRoot,
		long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors, string mmsWorkflowIngestionURL
	);

	void addEncoding_CountdownJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputsRoot, int64_t utcProxyPeriodStart, json outputsRoot,
		long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors, string mmsWorkflowIngestionURL
	);

	void addEncoding_LiveGridJob(shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputChannelsRoot, json outputsRoot);

	void addEncoding_VideoSpeed(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,
		string sourceAssetPathName, int64_t sourceDurationInMilliSeconds, string sourceFileExtension, string sourcePhysicalDeliveryURL,
		string sourceTranscoderStagingAssetPathName, int64_t encodingProfileKey, json encodingProfileDetailsRoot,
		string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL,
		string mmsBinaryIngestionURL, string mmsIngestionURL, EncodingPriority encodingPriority
	);

	void addEncoding_AddSilentAudio(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json sourcesRoot, int64_t encodingProfileKey, json encodingProfileDetailsRoot,
		string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL, EncodingPriority encodingPriority
	);

	void addEncoding_PictureInPictureJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t mainSourceMediaItemKey, int64_t mainSourcePhysicalPathKey,
		string mainSourceAssetPathName, int64_t mainSourceDurationInMilliSeconds, string mainSourceFileExtension,
		string mainSourcePhysicalDeliveryURL, string mainSourceTranscoderStagingAssetPathName, int64_t overlaySourceMediaItemKey,
		int64_t overlaySourcePhysicalPathKey, string overlaySourceAssetPathName, int64_t overlaySourceDurationInMilliSeconds,
		string overlaySourceFileExtension, string overlaySourcePhysicalDeliveryURL, string overlaySourceTranscoderStagingAssetPathName,
		bool soundOfMain, int64_t encodingProfileKey, json encodingProfileDetailsRoot, string encodedTranscoderStagingAssetPathName,
		string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,
		EncodingPriority encodingPriority
	);

	void addEncoding_IntroOutroOverlayJob(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey,

		int64_t encodingProfileKey, json encodingProfileDetailsRoot,

		int64_t introSourcePhysicalPathKey, string introSourceAssetPathName, string introSourceFileExtension,
		int64_t introSourceDurationInMilliSeconds, string introSourcePhysicalDeliveryURL, string introSourceTranscoderStagingAssetPathName,

		int64_t mainSourcePhysicalPathKey, string mainSourceAssetPathName, string mainSourceFileExtension, int64_t mainSourceDurationInMilliSeconds,
		string mainSourcePhysicalDeliveryURL, string mainSourceTranscoderStagingAssetPathName,

		int64_t outroSourcePhysicalPathKey, string outroSourceAssetPathName, string outroSourceFileExtension,
		int64_t outroSourceDurationInMilliSeconds, string outroSourcePhysicalDeliveryURL, string outroSourceTranscoderStagingAssetPathName,

		string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL,
		string mmsBinaryIngestionURL, string mmsIngestionURL,

		EncodingPriority encodingPriority
	);

	void addEncoding_CutFrameAccurate(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey,

		int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey, string sourceAssetPathName, int64_t sourceDurationInMilliSeconds,
		string sourceFileExtension, string sourcePhysicalDeliveryURL, string sourceTranscoderStagingAssetPathName, string endTime,

		int64_t encodingProfileKey, json encodingProfileDetailsRoot,

		string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL,
		string mmsBinaryIngestionURL, string mmsIngestionURL,

		EncodingPriority encodingPriority, int64_t newUtcStartTimeInMilliSecs, int64_t newUtcEndTimeInMilliSecs
	);

	void updateIngestionAndEncodingLiveRecordingPeriod(
		int64_t ingestionJobKey, int64_t encodingJobKey, time_t utcRecordingPeriodStart, time_t utcRecordingPeriodEnd
	);

	int updateEncodingJob(
		int64_t encodingJobKey, EncodingError encodingError, bool isIngestionJobFinished, int64_t ingestionJobKey, string ingestionErrorMessage = "",
		bool forceEncodingToBeFailed = false
	);

	void forceCancelEncodingJob(int64_t ingestionJobKey);

	void updateEncodingJobPriority(shared_ptr<Workspace> workspace, int64_t encodingJobKey, EncodingPriority newEncodingPriority);

	void updateEncodingJobTryAgain(shared_ptr<Workspace> workspace, int64_t encodingJobKey);

	void updateEncodingJobProgress(int64_t encodingJobKey, double encodingPercentage);

	void updateEncodingPid(int64_t encodingJobKey, int encodingPid);

	bool updateEncodingJobFailuresNumber(int64_t encodingJobKey, long failuresNumber);

	void updateEncodingJobIsKilled(int64_t encodingJobKey, bool isKilled);

	void updateEncodingJobTranscoder(int64_t encodingJobKey, int64_t encoderKey, string stagingEncodedAssetPathName);

	void updateEncodingJobParameters(int64_t encodingJobKey, string parameters);

	void updateOutputRtmpAndPlaURL(int64_t ingestionJobKey, int64_t encodingJobKey, int outputIndex, string rtmpURL, string playURL);

	void updateOutputHLSDetails(
		int64_t ingestionJobKey, int64_t encodingJobKey, int outputIndex, int64_t deliveryCode, int segmentDurationInSeconds,
		int playlistEntriesNumber, string manifestDirectoryPath, string manifestFileName, string otherOutputOptions
	);

	tuple<int64_t, string, int64_t, MMSEngineDBFacade::EncodingStatus, string> getEncodingJobDetails(int64_t encodingJobKey, bool fromMaster);

	void checkWorkspaceStorageAndMaxIngestionNumber(int64_t workspaceKey);

	string nextRelativePathToBeUsed(int64_t workspaceKey);

	pair<int64_t, int64_t> saveSourceContentMetadata(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, bool ingestionRowToBeUpdatedAsSuccess, MMSEngineDBFacade::ContentType contentType,
		int64_t encodingProfileKey, json parametersRoot, bool externalReadOnlyStorage, string relativePath, string mediaSourceFileName,
		int mmsPartitionIndexUsed, unsigned long sizeInBytes,

		// video-audio
		tuple<int64_t, long, json> &mediaInfoDetails, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks,

		// image
		int imageWidth, int imageHeight, string imageFormat, int imageQuality
	);

	int64_t saveVariantContentMetadata(
		int64_t workspaceKey, int64_t ingestionJobKey, int64_t liveRecordingIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
		string externalDeliveryTechnology, string externalDeliveryURL, string encodedFileName, string relativePath, int mmsPartitionIndexUsed,
		unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

		// video-audio
		tuple<int64_t, long, json> &mediaInfoDetails, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks,
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
		int imageWidth, int imageHeight, string imageFormat, int imageQuality
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

		vector<tuple<int, int64_t, string, string, int, int, string, long>>&
	videoTracks, vector<tuple<int, int64_t, string, long, int, long, string>>&
	audioTracks
	);
	*/

	void addCrossReference(
		int64_t ingestionJobKey, int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
		json crossReferenceParametersRoot
	);

	void removePhysicalPath(int64_t physicalPathKey);

	void removeMediaItem(int64_t mediaItemKey);

	json addYouTubeConf(int64_t workspaceKey, string label, string tokenType, string refreshToken, string accessToken);

	json modifyYouTubeConf(
		int64_t confKey, int64_t workspaceKey, string label, bool labelModified, string tokenType, bool tokenTypeModified, string refreshToken,
		bool refreshTokenModified, string accessToken, bool accessTokenModified
	);

	void removeYouTubeConf(int64_t workspaceKey, int64_t confKey);

	json getYouTubeConfList(int64_t workspaceKey, string label);

	tuple<string, string, string> getYouTubeDetailsByConfigurationLabel(int64_t workspaceKey, string youTubeConfigurationLabel);

	int64_t addFacebookConf(int64_t workspaceKey, string label, string userAccessToken);

	void modifyFacebookConf(int64_t confKey, int64_t workspaceKey, string label, string userAccessToken);

	void removeFacebookConf(int64_t workspaceKey, int64_t confKey);

	json getFacebookConfList(int64_t workspaceKey, int64_t confKey, string label);

	string getFacebookUserAccessTokenByConfigurationLabel(int64_t workspaceKey, string facebookConfigurationLabel);

	int64_t addTwitchConf(int64_t workspaceKey, string label, string refreshToken);

	void modifyTwitchConf(int64_t confKey, int64_t workspaceKey, string label, string refreshToken);

	void removeTwitchConf(int64_t workspaceKey, int64_t confKey);

	json getTwitchConfList(int64_t workspaceKey, int64_t confKey, string label);

	string getTwitchUserAccessTokenByConfigurationLabel(int64_t workspaceKey, string twitchConfigurationLabel);

	int64_t addTiktokConf(int64_t workspaceKey, string label, string token);

	void modifyTiktokConf(int64_t confKey, int64_t workspaceKey, string label, string token);

	void removeTiktokConf(int64_t workspaceKey, int64_t confKey);

	json getTiktokConfList(int64_t workspaceKey, int64_t confKey, string label);

	string getTiktokTokenByConfigurationLabel(int64_t workspaceKey, string tiktokConfigurationLabel);

	json addStream(
		int64_t workspaceKey, string label, string sourceType, int64_t encodersPoolKey, string url, string pushProtocol, int64_t pushEncoderKey,
		bool pushPublicEncoderName, int pushServerPort, string pushUri, int pushListenTimeout, int captureVideoDeviceNumber,
		string captureVideoInputFormat, int captureFrameRate, int captureWidth, int captureHeight, int captureAudioDeviceNumber,
		int captureChannelsNumber, int64_t tvSourceTVConfKey, string type, string description, string name, string region, string country,
		int64_t imageMediaItemKey, string imageUniqueName, int position, json userData
	);

	json modifyStream(
		int64_t confKey, string labelKey, int64_t workspaceKey, bool labelToBeModified, string label, bool sourceTypeToBeModified, string sourceType,
		bool encodersPoolKeyToBeModified, int64_t encodersPoolKey, bool urlToBeModified, string url, bool pushProtocolToBeModified,
		string pushProtocol, bool pushEncoderKeyToBeModified, int64_t pushEncoderKey, bool pushPublicEncoderNameToBeModified,
		bool pushPublicEncoderName, bool pushServerPortToBeModified, int pushServerPort, bool pushUriToBeModified, string pushUri,
		bool pushListenTimeoutToBeModified, int pushListenTimeout, bool captureVideoDeviceNumberToBeModified, int captureVideoDeviceNumber,
		bool captureVideoInputFormatToBeModified, string captureVideoInputFormat, bool captureFrameRateToBeModified, int captureFrameRate,
		bool captureWidthToBeModified, int captureWidth, bool captureHeightToBeModified, int captureHeight, bool captureAudioDeviceNumberToBeModified,
		int captureAudioDeviceNumber, bool captureChannelsNumberToBeModified, int captureChannelsNumber, bool tvSourceTVConfKeyToBeModified,
		int64_t tvSourceTVConfKey, bool typeToBeModified, string type, bool descriptionToBeModified, string description, bool nameToBeModified,
		string name, bool regionToBeModified, string region, bool countryToBeModified, string country, bool imageToBeModified,
		int64_t imageMediaItemKey, string imageUniqueName, bool positionToBeModified, int position, bool userDataToBeModified, json userData
	);

	void removeStream(int64_t workspaceKey, int64_t confKey, string label);

#ifdef __POSTGRES__
	json getStreamList(
		int64_t workspaceKey, int64_t liveURLKey, int start, int rows, string label, bool labelLike, string sourceType, string type, string name,
		string region, string country, string url, string labelOrder, bool fromMaster = false
	);
#else
	json getStreamList(
		int64_t workspaceKey, int64_t liveURLKey, int start, int rows, string label, bool labelLike, string sourceType, string type, string name,
		string region, string country, string url, string labelOrder
	);
#endif

#ifdef __POSTGRES__
	json getStreamFreePushEncoderPort(int64_t encoderKey, bool fromMaster = false);
#endif

	tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
	getStreamDetails(int64_t workspaceKey, string label, bool warningIfMissing);

	tuple<string, string, string> getStreamDetails(int64_t workspaceKey, int64_t confKey);

	json addSourceTVStream(
		string type, int64_t serviceId, int64_t networkId, int64_t transportStreamId, string name, string satellite, int64_t frequency, string lnb,
		int videoPid, string audioPids, int audioItalianPid, int audioEnglishPid, int teletextPid, string modulation, string polarization,
		int64_t symbolRate, int64_t bandwidthInHz, string country, string deliverySystem
	);

	json modifySourceTVStream(
		int64_t confKey,

		bool typeToBeModified, string type, bool serviceIdToBeModified, int64_t serviceId, bool networkIdToBeModified, int64_t networkId,
		bool transportStreamIdToBeModified, int64_t transportStreamId, bool nameToBeModified, string name, bool satelliteToBeModified,
		string satellite, bool frequencyToBeModified, int64_t frequency, bool lnbToBeModified, string lnb, bool videoPidToBeModified, int videoPid,
		bool audioPidsToBeModified, string audioPids, bool audioItalianPidToBeModified, int audioItalianPid, bool audioEnglishPidToBeModified,
		int audioEnglishPid, bool teletextPidToBeModified, int teletextPid, bool modulationToBeModified, string modulation,
		bool polarizationToBeModified, string polarization, bool symbolRateToBeModified, int64_t symbolRate, bool bandwidthInHzToBeModified,
		int64_t bandwidthInHz, bool countryToBeModified, string country, bool deliverySystemToBeModified, string deliverySystem
	);

	void removeSourceTVStream(int64_t confKey);

#ifdef __POSTGRES__
	json getSourceTVStreamList(
		int64_t confKey, int start, int rows, string type, int64_t serviceId, string name, int64_t frequency, string lnb, int videoPid,
		string audioPids, string nameOrder, bool fromMaster = false
	);
#else
	json getSourceTVStreamList(
		int64_t confKey, int start, int rows, string type, int64_t serviceId, string name, int64_t frequency, string lnb, int videoPid,
		string audioPids, string nameOrder
	);
#endif

	tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int> getSourceTVStreamDetails(int64_t confKey, bool warningIfMissing);

	int64_t addAWSChannelConf(int64_t workspaceKey, string label, string channelId, string rtmpURL, string playURL, string type);

	void modifyAWSChannelConf(int64_t confKey, int64_t workspaceKey, string label, string channelId, string rtmpURL, string playURL, string type);

	void removeAWSChannelConf(int64_t workspaceKey, int64_t confKey);

	json getAWSChannelConfList(int64_t workspaceKey, int64_t confKey, string label,
							   int type); // 0: all, 1: SHARED, 2: DEDICATED

	tuple<string, string, string, bool> reserveAWSChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey);

	string releaseAWSChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey);

	int64_t
	addCDN77ChannelConf(int64_t workspaceKey, string label, string rtmpURL, string resourceURL, string filePath, string secureToken, string type);

	void modifyCDN77ChannelConf(
		int64_t confKey, int64_t workspaceKey, string label, string rtmpURL, string resourceURL, string filePath, string secureToken, string type
	);

	void removeCDN77ChannelConf(int64_t workspaceKey, int64_t confKey);

	json getCDN77ChannelConfList(int64_t workspaceKey, int64_t confKey, string label,
								 int type); // 0: all, 1: SHARED, 2: DEDICATED

	tuple<string, string, string> getCDN77ChannelDetails(int64_t workspaceKey, string label);

	tuple<string, string, string, string, string, bool>
	reserveCDN77Channel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey);

	void releaseCDN77Channel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey);

	int64_t addRTMPChannelConf(
		int64_t workspaceKey, string label, string rtmpURL, string streamName, string userName, string password, string playURL, string type
	);

	void modifyRTMPChannelConf(
		int64_t confKey, int64_t workspaceKey, string label, string rtmpURL, string streamName, string userName, string password, string playURL,
		string type
	);

	void removeRTMPChannelConf(int64_t workspaceKey, int64_t confKey);

	json getRTMPChannelConfList(int64_t workspaceKey, int64_t confKey, string label,
								int type); // 0: all, 1: SHARED, 2: DEDICATED

	tuple<int64_t, string, string, string, string, string> getRTMPChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing);

	tuple<string, string, string, string, string, string, bool>
	reserveRTMPChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey);

	void releaseRTMPChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey);

	int64_t addHLSChannelConf(int64_t workspaceKey, string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, string type);

	void modifyHLSChannelConf(
		int64_t confKey, int64_t workspaceKey, string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, string type
	);

	void removeHLSChannelConf(int64_t workspaceKey, int64_t confKey);

	json getHLSChannelConfList(int64_t workspaceKey, int64_t confKey, string label,
							   int type); // 0: all, 1: SHARED, 2: DEDICATED

	tuple<int64_t, int64_t, int, int> getHLSChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing);

	tuple<string, int64_t, int, int, bool> reserveHLSChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey);

	void releaseHLSChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey);

	int64_t addFTPConf(int64_t workspaceKey, string label, string server, int port, string userName, string password, string remoteDirectory);

	void modifyFTPConf(
		int64_t confKey, int64_t workspaceKey, string label, string server, int port, string userName, string password, string remoteDirectory
	);

	void removeFTPConf(int64_t workspaceKey, int64_t confKey);

	json getFTPConfList(int64_t workspaceKey);

	tuple<string, int, string, string, string> getFTPByConfigurationLabel(int64_t workspaceKey, string liveURLConfigurationLabel);

	int64_t addEMailConf(int64_t workspaceKey, string label, string addresses, string subject, string message);

	void modifyEMailConf(int64_t confKey, int64_t workspaceKey, string label, string addresses, string subject, string message);

	void removeEMailConf(int64_t workspaceKey, int64_t confKey);

	json getEMailConfList(int64_t workspaceKey);

	tuple<string, string, string> getEMailByConfigurationLabel(int64_t workspaceKey, string liveURLConfigurationLabel);

	json addRequestStatistic(int64_t workspaceKey, string ipAddress, string userId, int64_t physicalPathKey, int64_t confStreamKey, string title);

	json getRequestStatisticList(int64_t workspaceKey, string userId, string title, string startDate, string endDate, int start, int rows);

	json getRequestStatisticPerContentList(
		int64_t workspaceKey, string title, string userId, string startDate, string endDate, int64_t minimalNextRequestDistanceInSeconds,
		bool totalNumFoundToBeCalculated, int start, int rows
	);

	json getRequestStatisticPerUserList(
		int64_t workspaceKey, string title, string userId, string startDate, string endDate, int64_t minimalNextRequestDistanceInSeconds,
		bool totalNumFoundToBeCalculated, int start, int rows
	);

	json getRequestStatisticPerMonthList(
		int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
		int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
	);

	json getRequestStatisticPerDayList(
		int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
		int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
	);

	json getRequestStatisticPerHourList(
		int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
		int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
	);

	json getLoginStatisticList(string startDate, string endDate, int start, int rows);

	void setLock(
		LockType lockType, int waitingTimeoutInSecondsIfLocked, string owner, string label, int milliSecondsToSleepWaitingLock = 500,
		string data = "no data"
	);

	void releaseLock(LockType lockType, string label, string data = "no data");

	int64_t addEncoder(string label, bool external, bool enabled, string protocol, string publicServerName, string internalServerName, int port);

	void modifyEncoder(
		int64_t encoderKey, bool labelToBeModified, string label, bool externalToBeModified, bool external, bool enabledToBeModified, bool enabled,
		bool protocolToBeModified, string protocol, bool publicServerNameToBeModified, string publicServerName, bool internalServerNameToBeModified,
		string internalServerName, bool portToBeModified, int port
		// bool maxTranscodingCapabilityToBeModified, int
		// maxTranscodingCapability, bool
		// maxLiveProxiesCapabilitiesToBeModified, int
		// maxLiveProxiesCapabilities, bool
		// maxLiveRecordingCapabilitiesToBeModified, int
		// maxLiveRecordingCapabilities
	);

	void removeEncoder(int64_t encoderKey);

	tuple<string, string, string> getEncoderDetails(int64_t encoderKey);

	bool isEncoderRunning(bool external, string protocol, string publicServerName, string internalServerName, int port);

	pair<bool, int> getEncoderInfo(bool external, string protocol, string publicServerName, string internalServerName, int port);

	void addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey);

	void addAssociationWorkspaceEncoder(int64_t workspaceKey, string sharedEncodersPoolLabel, json sharedEncodersLabel);

	void removeAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey);

	json getEncoderWorkspacesAssociation(int64_t encoderKey);

	json getEncoderList(
		bool admin, int start, int rows, bool allEncoders, int64_t workspaceKey, bool runningInfo, int64_t encoderKey, string label,
		string serverName, int port,
		string labelOrder // "" or "asc" or "desc"
	);

	string getEncodersPoolDetails(int64_t encodersPoolKey);

	json getEncodersPoolList(
		int start, int rows, int64_t workspaceKey, int64_t encodersPoolKey, string label,
		string labelOrder // "" or "asc" or "desc"
	);

	tuple<int64_t, bool, string, string, string, int>
	getRunningEncoderByEncodersPool(int64_t workspaceKey, string encodersPoolLabel, int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed);

	int getEncodersNumberByEncodersPool(int64_t workspaceKey, string encodersPoolLabel);

	pair<string, bool> getEncoderURL(int64_t encoderKey, string serverName = "");

	int64_t addEncodersPool(int64_t workspaceKey, string label, vector<int64_t> &encoderKeys);

	int64_t modifyEncodersPool(int64_t encodersPoolKey, int64_t workspaceKey, string newLabel, vector<int64_t> &newEncoderKeys);

	void removeEncodersPool(int64_t encodersPoolKey);

	void addUpdatePartitionInfo(int partitionKey, string partitionName, uint64_t currentFreeSizeInBytes, int64_t freeSpaceToLeaveInMB);

#ifdef __POSTGRES__
	pair<int, uint64_t> getPartitionToBeUsedAndUpdateFreeSpace(int64_t ingestionJobKey, uint64_t ullFSEntrySizeInBytes);
#else
	pair<int, uint64_t> getPartitionToBeUsedAndUpdateFreeSpace(uint64_t ullFSEntrySizeInBytes);
#endif

	fs::path getPartitionPathName(int partitionKey);

	uint64_t updatePartitionBecauseOfDeletion(int partitionKey, uint64_t ullFSEntrySizeInBytes);

	void getPartitionsInfo(vector<pair<int, uint64_t>> &partitionsInfo);

	static int64_t parseRetention(string retention);

	json getStreamInputRoot(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, string configurationLabel, string useVideoTrackFromPhysicalPathName,
		string useVideoTrackFromPhysicalDeliveryURL, int maxWidth, string userAgent, string otherInputOptions, string taskEncodersPoolLabel,
		json drawTextDetailsRoot
	);

	json
	getVodInputRoot(MMSEngineDBFacade::ContentType vodContentType, vector<tuple<int64_t, string, string, string>> &sources, json drawTextDetailsRoot);

	json getCountdownInputRoot(
		string mmsSourceVideoAssetPathName, string mmsSourceVideoAssetDeliveryURL, int64_t physicalPathKey, int64_t videoDurationInMilliSeconds,
		json drawTextDetailsRoot
	);

	json getDirectURLInputRoot(string url, json drawTextDetailsRoot);

	string getStreamingYouTubeLiveURL(shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t confKey, string liveURL);

	bool oncePerDayExecution(OncePerDayType oncePerDayType);

	static MMSEngineDBFacade::DeliveryTechnology fileFormatToDeliveryTechnology(string fileFormat);

  private:
	shared_ptr<spdlog::logger> _logger;
	json _configuration;

#ifdef __POSTGRES__
#else
	shared_ptr<MySQLConnectionFactory> _mySQLMasterConnectionFactory;
	shared_ptr<DBConnectionPool<MySQLConnection>> _masterConnectionPool;
	shared_ptr<MySQLConnectionFactory> _mySQLSlaveConnectionFactory;
	shared_ptr<DBConnectionPool<MySQLConnection>> _slaveConnectionPool;
#endif

	shared_ptr<PostgresConnectionFactory> _postgresMasterConnectionFactory;
	shared_ptr<DBConnectionPool<PostgresConnection>> _masterPostgresConnectionPool;
	shared_ptr<PostgresConnectionFactory> _postgresSlaveConnectionFactory;
	shared_ptr<DBConnectionPool<PostgresConnection>> _slavePostgresConnectionPool;

#ifdef __POSTGRES__
#else
	string _defaultContentProviderName;
#endif
	// string                          _defaultTerritoryName;
	int _ingestionJobsSelectPageSize;
	int _maxEncodingFailures;
	int _confirmationCodeRetentionInDays;
	int _contentRetentionInMinutesDefaultValue;
	int _contentNotTransferredRetentionInHours;

	int _maxSecondsToWaitUpdateIngestionJobLock;
	int _maxSecondsToWaitUpdateEncodingJobLock;
	int _maxSecondsToWaitCheckIngestionLock;
	int _maxSecondsToWaitCheckEncodingJobLock;
	int _maxSecondsToWaitMainAndBackupLiveChunkLock;
	int _maxSecondsToWaitSetNotToBeExecutedLock;

	int _doNotManageIngestionsOlderThanDays;
	int _ingestionWorkflowRetentionInDays;
	int _statisticRetentionInMonths;

	string _ffmpegEncoderUser;
	string _ffmpegEncoderPassword;
	string _ffmpegEncoderStatusURI;
	string _ffmpegEncoderInfoURI;
	int _ffmpegEncoderInfoTimeout;

	chrono::system_clock::time_point _lastConnectionStatsReport;
	int _dbConnectionPoolStatsReportPeriodInSeconds;

	string _predefinedWorkflowLibraryDirectoryPath;

	string _predefinedVideoProfilesDirectoryPath;
	string _predefinedAudioProfilesDirectoryPath;
	string _predefinedImageProfilesDirectoryPath;

	vector<string> _adminEmailAddresses;

	int _getIngestionJobsCurrentIndex;
	int _getEncodingJobsCurrentIndex;

	bool _statisticsEnabled;

	bool _geoServiceEnabled;
	int _geoServiceMaxDaysBeforeUpdate;
	string _geoServiceURL;
	string _geoServiceKey;
	int _geoServiceTimeoutInSeconds;

#ifdef __POSTGRES__
	string getPostgresArray(vector<string> &arrayElements, bool emptyElementToBeRemoved, transaction_base *trans);
	string getPostgresArray(json arrayRoot, bool emptyElementToBeRemoved, transaction_base *trans);
	bool isTimezoneValid(string timezone);
#endif

#ifdef __POSTGRES__
	json getStreamList(
		nontransaction &trans, shared_ptr<PostgresConnection> conn, int64_t workspaceKey, int64_t liveURLKey, int start, int rows, string label,
		bool labelLike, string url, string sourceType, string type, string name, string region, string country, string labelOrder
	);
#else
	json getStreamList(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t liveURLKey, int start, int rows, string label, bool labelLike, string url,
		string sourceType, string type, string name, string region, string country, string labelOrder
	);
#endif

#ifdef __POSTGRES__
	string createAPIKeyForActiveDirectoryUser(
		shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t userKey, string userEmailAddress, bool createRemoveWorkspace,
		bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration,
		bool killEncoding, bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder, int64_t workspaceKey,
		int expirationInDaysWorkspaceDefaultValue
	);
#else
	string createAPIKeyForActiveDirectoryUser(
		shared_ptr<MySQLConnection> conn, int64_t userKey, string userEmailAddress, bool createRemoveWorkspace, bool ingestWorkflow,
		bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding,
		bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder, int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
	);
#endif

#ifdef __POSTGRES__
	void addWorkspaceForAdminUsers(
		shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
	);
#else
	void addWorkspaceForAdminUsers(shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue);
#endif

#ifdef __POSTGRES__
	string createCode(
		shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t workspaceKey, int64_t userKey, string userEmail, CodeType codeType,
		bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace,
		bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder
	);
#else
	string createCode(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t userKey, string userEmail, CodeType codeType, bool admin,
		bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder
	);
#endif

#ifdef __POSTGRES__
	tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> isIngestionJobToBeManaged(
		int64_t ingestionJobKey, int64_t workspaceKey, IngestionStatus ingestionStatus, IngestionType ingestionType,
		shared_ptr<PostgresConnection> conn, transaction_base *trans
	);
#else
	tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> isIngestionJobToBeManaged(
		int64_t ingestionJobKey, int64_t workspaceKey, IngestionStatus ingestionStatus, IngestionType ingestionType, shared_ptr<MySQLConnection> conn
	);
#endif

#ifdef __POSTGRES__
	void addIngestionJobOutput(
		shared_ptr<PostgresConnection>, transaction_base *trans, int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey,
		int64_t sourceIngestionJobKey
	);
#else
	void addIngestionJobOutput(
		shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey
	);
#endif

	int getIngestionTypePriority(MMSEngineDBFacade::IngestionType);

	int getEncodingTypePriority(MMSEngineDBFacade::EncodingType);

#ifdef __POSTGRES__
#else
	pair<shared_ptr<sql::ResultSet>, int64_t> getMediaItemsList_withoutTagsCheck(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t mediaItemKey, vector<int64_t> &otherMediaItemsKey, int start, int rows,
		bool contentTypePresent, ContentType contentType,
		// bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate, string title, int liveRecordingChunk, int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, string jsonCondition, string orderBy,
		string jsonOrderBy, bool admin
	);

	pair<shared_ptr<sql::ResultSet>, int64_t> getMediaItemsList_withTagsCheck(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, string temporaryTableName, int64_t mediaItemKey, vector<int64_t> &otherMediaItemsKey,
		int start, int rows, bool contentTypePresent, ContentType contentType,
		// bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate, string title, int liveRecordingChunk, int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, string jsonCondition,
		vector<string> &tagsIn, vector<string> &tagsNotIn, string orderBy, string jsonOrderBy, bool admin
	);
#endif

#ifdef __POSTGRES__
	void updateIngestionJob(
		shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t ingestionJobKey, IngestionStatus newIngestionStatus,
		string errorMessage, string processorMMS = "noToBeUpdated"
	);
#else
	void updateIngestionJob(
		shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, IngestionStatus newIngestionStatus, string errorMessage,
		string processorMMS = "noToBeUpdated"
	);
#endif

#ifdef __POSTGRES__
	pair<int64_t, string> addWorkspace(
		shared_ptr<PostgresConnection> conn, work &trans, int64_t userKey, bool admin, bool createRemoveWorkspace, bool ingestWorkflow,
		bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding,
		bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder, string workspaceName, WorkspaceType workspaceType,
		string deliveryURL, EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod, long maxIngestionsNumber, long maxStorageInMB,
		string languageCode, chrono::system_clock::time_point userExpirationDate
	);
#else
	pair<int64_t, string> addWorkspace(
		shared_ptr<MySQLConnection> conn, int64_t userKey, bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles,
		bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob,
		bool editEncodersPool, bool applicationRecorder, string workspaceName, WorkspaceType workspaceType, string deliveryURL,
		EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod, long maxIngestionsNumber, long maxStorageInMB, string languageCode,
		chrono::system_clock::time_point userExpirationDate
	);
#endif

#ifdef __POSTGRES__
	void manageExternalUniqueName(
		shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t workspaceKey, int64_t mediaItemKey, bool allowUniqueNameOverride,
		string uniqueName
	);
#else
	void manageExternalUniqueName(
		shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t mediaItemKey, bool allowUniqueNameOverride, string uniqueName
	);
#endif

#ifdef __POSTGRES__
	int64_t saveVariantContentMetadata(
		shared_ptr<PostgresConnection> conn, work &trans,

		int64_t workspaceKey, int64_t ingestionJobKey, int64_t liveRecordingIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
		string externalDeliveryTechnology, string externalDeliveryURL, string encodedFileName, string relativePath, int mmsPartitionIndexUsed,
		unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

		// video-audio
		tuple<int64_t, long, json> &mediaInfoDetails, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks,

		// image
		int imageWidth, int imageHeight, string imageFormat, int imageQuality
	);
#else
	int64_t saveVariantContentMetadata(
		shared_ptr<MySQLConnection> conn,

		int64_t workspaceKey, int64_t ingestionJobKey, int64_t liveRecordingIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
		string externalDeliveryTechnology, string externalDeliveryURL, string encodedFileName, string relativePath, int mmsPartitionIndexUsed,
		unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

		// video-audio
		tuple<int64_t, long, json> &mediaInfoDetails, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks,

		// image
		int imageWidth, int imageHeight, string imageFormat, int imageQuality
	);
#endif

#ifdef __POSTGRES__
	int64_t addUpdateWorkflowAsLibrary(
		shared_ptr<PostgresConnection> conn, nontransaction &trans, int64_t userKey, int64_t workspaceKey, string label,
		int64_t thumbnailMediaItemKey, string jsonWorkflow, bool admin
	);
#else
	int64_t addUpdateWorkflowAsLibrary(
		shared_ptr<MySQLConnection> conn, int64_t userKey, int64_t workspaceKey, string label, int64_t thumbnailMediaItemKey, string jsonWorkflow,
		bool admin
	);
#endif

#ifdef __POSTGRES__
	void manageCrossReferences(
		shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t ingestionJobKey, int64_t workspaceKey, int64_t mediaItemKey,
		json crossReferencesRoot
	);
#endif

#ifdef __POSTGRES__
	void addCrossReference(
		shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t ingestionJobKey, int64_t sourceMediaItemKey,
		CrossReferenceType crossReferenceType, int64_t targetMediaItemKey, json crossReferenceParametersRoot
	);
#else
	void addCrossReference(
		shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType,
		int64_t targetMediaItemKey, json crossReferenceParametersRoot
	);
#endif

#ifdef __POSTGRES__
	json getIngestionJobRoot(
		shared_ptr<Workspace> workspace, row &row,
		bool dependencyInfo,	  // added for performance issue
		bool ingestionJobOutputs, // added because output could be thousands of
								  // entries
		shared_ptr<PostgresConnection> conn, nontransaction &trans
	);
#else
	json getIngestionJobRoot(
		shared_ptr<Workspace> workspace, shared_ptr<sql::ResultSet> resultSet, bool dependencyInfo, bool ingestionJobOutputs,
		shared_ptr<MySQLConnection> conn
	);
#endif

#ifdef __POSTGRES__
	void manageIngestionJobStatusUpdate(
		int64_t ingestionJobKey, IngestionStatus newIngestionStatus, bool updateIngestionRootStatus, shared_ptr<PostgresConnection> conn,
		transaction_base *trans
	);
#else
	void manageIngestionJobStatusUpdate(
		int64_t ingestionJobKey, IngestionStatus newIngestionStatus, bool updateIngestionRootStatus, shared_ptr<MySQLConnection> conn
	);
#endif

#ifdef __POSTGRES__
	pair<int64_t, int64_t> getWorkspaceUsage(shared_ptr<PostgresConnection> conn, nontransaction &trans, int64_t workspaceKey);
#else
	pair<int64_t, int64_t> getWorkspaceUsage(shared_ptr<MySQLConnection> conn, int64_t workspaceKey);
#endif

#ifdef __POSTGRES__
	json getWorkspaceDetailsRoot(shared_ptr<PostgresConnection> conn, nontransaction &trans, row &row, bool userAPIKeyInfo, bool costDetails);
#else
	json getWorkspaceDetailsRoot(shared_ptr<MySQLConnection> conn, shared_ptr<sql::ResultSet> resultSet, bool userAPIKeyInfo);
#endif

#ifdef __POSTGRES__
	void addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey, shared_ptr<PostgresConnection> conn, nontransaction &trans);
#else
	void addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey, shared_ptr<MySQLConnection> conn);
#endif

#ifdef __POSTGRES__
	json getEncoderRoot(bool admin, bool runningInfo, row &row);
#else
	json getEncoderRoot(bool admin, bool runningInfo, shared_ptr<sql::ResultSet> resultSet);
#endif

	void createTablesIfNeeded();

	bool isRealDBError(string exceptionMessage);

	bool isJsonTypeSupported(shared_ptr<sql::Statement> statement);

#ifdef __POSTGRES__
#else
	void addTags(shared_ptr<MySQLConnection> conn, int64_t mediaItemKey, json tagsRoot);
#endif

#ifdef __POSTGRES__
#else
	void removeTags(shared_ptr<MySQLConnection> conn, int64_t mediaItemKey);
#endif

#ifdef __POSTGRES__
#else
	int64_t getLastInsertId(shared_ptr<MySQLConnection> conn);
#endif

	/*
	int64_t addTerritory (
	shared_ptr<MySQLConnection> conn,
		int64_t workspaceKey,
		string territoryName
	);
	*/

	bool isMMSAdministratorUser(long lUserType) { return (lUserType & 0x1) != 0 ? true : false; }

	bool isMMSUser(long lUserType) { return (lUserType & 0x2) != 0 ? true : false; }

	bool isEndUser(long lUserType) { return (lUserType & 0x4) != 0 ? true : false; }

	bool isMMSEditorialUser(long lUserType) { return (lUserType & 0x8) != 0 ? true : false; }

	bool isBillingAdministratorUser(long lUserType) { return (lUserType & 0x10) != 0 ? true : false; }

	int getMMSAdministratorUser() { return ((int)0x1); }

	int getMMSUser() { return ((int)0x2); }

	int getEndUser() { return ((int)0x4); }

	int getMMSEditorialUser() { return ((int)0x8); }

	pair<long, string> getLastYouTubeURLDetails(shared_ptr<Workspace> workspace, int64_t ingestionKey, int64_t confKey);

	void
	updateChannelDataWithNewYouTubeURL(shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t confKey, string streamingYouTubeLiveURL);
};

#endif /* MMSEngineDBFacade_h */
