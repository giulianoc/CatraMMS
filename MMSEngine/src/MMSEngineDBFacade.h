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

#pragma once

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
#ifdef DBCONNECTION_MYSQL
	#include "MySQLConnection.h"
#endif
#include "PostgresConnection.h"
#include "PostgresHelper.h"

namespace fs = std::filesystem;

/*
using nlohmann::json = nlohmann::nlohmann::json;
using orderd_nlohmann::json = nlohmann::ordered_nlohmann::json;
using namespace nlohmann::literals;
*/

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename((char *)__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

// using namespace std;

#define SQLQUERYLOG(queryLabel, elapsed, ...)                                                                                                        \
	if (elapsed > maxQueryElapsed(queryLabel))                                                                                                       \
	{                                                                                                                                                \
		SPDLOG_INFO(__VA_ARGS__);                                                                                                                    \
		auto logger = spdlog::get("slow-query");                                                                                                     \
		if (logger != nullptr)                                                                                                                       \
			SPDLOG_LOGGER_WARN(logger, __VA_ARGS__);                                                                                                 \
	}                                                                                                                                                \
	else                                                                                                                                             \
		SPDLOG_DEBUG(__VA_ARGS__);

struct LoginFailed : public std::exception
{
	char const *what() const throw() { return "email and/or password are wrong"; };
};

struct APIKeyNotFoundOrExpired : public std::exception
{
	char const *what() const throw() { return "APIKey was not found or it is expired"; };
};

struct DBRecordNotFound : public std::exception
{

	std::string _errorMessage;

	DBRecordNotFound(std::string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct MediaItemKeyNotFound : public std::exception
{

	std::string _errorMessage;

	MediaItemKeyNotFound(std::string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct DeadlockFound : public std::exception
{

	std::string _errorMessage;

	DeadlockFound(std::string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct EncoderNotFound : public std::exception
{

	std::string _errorMessage;

	EncoderNotFound(std::string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct AlreadyLocked : public std::exception
{
	std::string _errorMessage;

	AlreadyLocked(std::string label, std::string owner, int currentLockDuration)
	{
		_errorMessage =
			std::string("Already locked") + ", label: " + label + ", owner: " + owner + ", currentLockDuration (secs): " + std::to_string(currentLockDuration);
	}

	char const *what() const throw() { return _errorMessage.c_str(); };
};

struct YouTubeURLNotRetrieved : public std::exception
{
	char const *what() const throw() { return "YouTube URL was not retrieved"; };
};

struct NoMoreSpaceInMMSPartition : public std::exception
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
			throw std::runtime_error(fmt::format("toString with a wrong CodeType: {}", static_cast<int>(codeType)));
		}
	}
	static CodeType toCodeType(const std::string &codeType)
	{
		std::string lowerCase;
		lowerCase.resize(codeType.size());
		transform(codeType.begin(), codeType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "userregistration")
			return CodeType::UserRegistration;
		else if (lowerCase == "shareworkspace")
			return CodeType::ShareWorkspace;
		else if (lowerCase == "userregistrationcomingfromshareworkspace")
			return CodeType::UserRegistrationComingFromShareWorkspace;
		else
			throw std::runtime_error(fmt::format(
				"toCodeType with a wrong CodeType"
				", codeType: {}",
				codeType
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong LockType: {}", static_cast<int>(lockType)));
		}
	}
	static LockType toLockType(const std::string &lockType)
	{
		std::string lowerCase;
		lowerCase.resize(lockType.size());
		transform(lockType.begin(), lockType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "ingestion")
			return LockType::Ingestion;
		else if (lowerCase == "encoding")
			return LockType::Encoding;
		else
			throw std::runtime_error(fmt::format(
				"toLockType with a wrong LockType"
				", lockType: {}",
				lockType
			));
	}

	enum class OnceType
	{
		DBDataRetention = 0,
		GEOInfo = 1
	};
	static const char *toString(const OnceType &onceType)
	{
		switch (onceType)
		{
		case OnceType::DBDataRetention:
			return "DBDataRetention";
		case OnceType::GEOInfo:
			return "GEOInfo";
		default:
			throw std::runtime_error(fmt::format("toString with a wrong onceType: {}", static_cast<int>(onceType)));
		}
	}
	static OnceType toOnceType(const std::string &onceType)
	{
		std::string lowerCase;
		lowerCase.resize(onceType.size());
		transform(onceType.begin(), onceType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "dbdataretention")
			return OnceType::DBDataRetention;
		if (lowerCase == "geoinfo")
			return OnceType::GEOInfo;
		else
			throw std::runtime_error(fmt::format(
				"toOnceType with a wrong OnceType"
				", onceType: {}",
				onceType
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong contentType: {}", static_cast<int>(contentType)));
		}
	}
	static ContentType toContentType(const std::string &contentType)
	{
		std::string lowerCase;
		lowerCase.resize(contentType.size());
		transform(contentType.begin(), contentType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "video")
			return ContentType::Video;
		else if (lowerCase == "audio")
			return ContentType::Audio;
		else if (lowerCase == "image")
			return ContentType::Image;
		else
			throw std::runtime_error(fmt::format(
				"toContentType with a wrong ContentType"
				", contentType: {}",
				contentType
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong encodingPriority: {}", static_cast<int>(priority)));
		}
	}
	static EncodingPriority toEncodingPriority(const std::string &priority)
	{
		std::string lowerCase;
		lowerCase.resize(priority.size());
		transform(priority.begin(), priority.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "low")
			return EncodingPriority::Low;
		else if (lowerCase == "medium")
			return EncodingPriority::Medium;
		else if (lowerCase == "high")
			return EncodingPriority::High;
		else
			throw std::runtime_error(fmt::format(
				"toEncodingPriority with a wrong EncodingPriority"
				", encodingPriority: {}",
				priority
			));
	}

	// se si aggiungono/eliminano status, verificare le query dove compare status in (...)
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
			throw std::runtime_error(fmt::format("toString with a wrong EncodingStatus: {}", static_cast<int>(encodingStatus)));
		}
	}
	static EncodingStatus toEncodingStatus(const std::string &encodingStatus)
	{
		std::string lowerCase;
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
			throw std::runtime_error(fmt::format(
				"toEncodingStatus with a wrong EncodingStatus"
				", encodingStatus: {}",
				encodingStatus
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong encodingType: {}", static_cast<int>(encodingType)));
		}
	}
	static EncodingType toEncodingType(const std::string &encodingType)
	{
		std::string lowerCase;
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
			throw std::runtime_error(fmt::format(
				"toEncodingType with a wrong EncodingType"
				", encodingType: {}",
				encodingType
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong encodingError: {}", static_cast<int>(encodingError)));
		}
	}
	static EncodingError toEncodingError(const std::string &encodingError)
	{
		std::string lowerCase;
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
			throw std::runtime_error(fmt::format(
				"toEncodingError with a wrong EncodingError"
				", encodingError: {}",
				encodingError
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong deliveryTechnology: {}", static_cast<int>(deliveryTechnology)));
		}
	}
	static DeliveryTechnology toDeliveryTechnology(const std::string &deliveryTechnology)
	{
		std::string lowerCase;
		lowerCase.resize(deliveryTechnology.size());
		transform(deliveryTechnology.begin(), deliveryTechnology.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "download")
			return DeliveryTechnology::Download;
		else if (lowerCase == "downloadandstreaming")
			return DeliveryTechnology::DownloadAndStreaming;
		else if (lowerCase == "httpstreaming")
			return DeliveryTechnology::HTTPStreaming;
		else
			throw std::runtime_error(fmt::format(
				"toDeliveryTechnology with a wrong DeliveryTechnology"
				", deliveryTechnology: {}",
				deliveryTechnology
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong encodingPeriod: {}", static_cast<int>(encodingPeriod)));
		}
	}
	static EncodingPeriod toEncodingPeriod(const std::string &encodingPeriod)
	{
		std::string lowerCase;
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
			throw std::runtime_error(fmt::format(
				"toEncodingPeriod with a wrong EncodingPeriod"
				", encodingPeriod: {}",
				encodingPeriod
			));
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
			throw std::runtime_error(fmt::format("toString with a wrong videoSpeedType: {}", static_cast<int>(videoSpeedType)));
		}
	}
	static VideoSpeedType toVideoSpeedType(const std::string &videoSpeedType)
	{
		std::string lowerCase;
		lowerCase.resize(videoSpeedType.size());
		transform(videoSpeedType.begin(), videoSpeedType.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "slowdown")
			return VideoSpeedType::SlowDown;
		else if (lowerCase == "speedup")
			return VideoSpeedType::SpeedUp;
		else
			throw std::runtime_error(fmt::format(
				"toVideoSpeedType with a wrong VideoSpeedType"
				", videoSpeedType: {}",
				videoSpeedType
			));
	}

	struct EncodingItem
	{
		long long _encodingJobKey;
		long long _ingestionJobKey;
		EncodingPriority _encodingPriority;
		EncodingType _encodingType;

		// Key of the Encoder used by this job:
		int64_t _encoderKey;
		std::string _stagingEncodedAssetPathName;

		nlohmann::json _ingestedParametersRoot;

		nlohmann::json _encodingParametersRoot;

		std::shared_ptr<Workspace> _workspace;
	};

	enum class WorkspaceType
	{
		LiveSessionOnly = 0,
		IngestionAndDelivery = 1,
		EncodingOnly = 2
	};

	enum class IngestionType
	{
		Unknown = 0, // in case nlohmann::json was not able to be parsed
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
			throw std::runtime_error(fmt::format("toString with a wrong ingestionType: {}", static_cast<int>(ingestionType)));
		}
	}
	static IngestionType toIngestionType(const std::string &ingestionType)
	{
		std::string lowerCase;
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
			throw std::runtime_error(fmt::format(
				"toIngestionType with a wrong IngestionType"
				", ingestionType: {}",
				ingestionType
			));
	}

	// se si aggiungono/eliminano status, verificare le query dove compare status in (...)
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
			throw std::runtime_error(fmt::format("toString with a wrong IngestionStatus: {}", static_cast<int>(ingestionStatus)));
		}
	}
	static IngestionStatus toIngestionStatus(const std::string &ingestionStatus)
	{
		std::string lowerCase;
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
			throw std::runtime_error(std::string("Wrong IngestionStatus") + ", ingestionStatus: " + ingestionStatus);
	}
	static bool isIngestionStatusFinalState(const IngestionStatus &ingestionStatus)
	{
		std::string prefix = "End";
		std::string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);

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
			throw std::runtime_error(std::string("Wrong IngestionRootStatus"));
		}
	}
	static IngestionRootStatus toIngestionRootStatus(const std::string &ingestionRootStatus)
	{
		std::string lowerCase;
		lowerCase.resize(ingestionRootStatus.size());
		transform(ingestionRootStatus.begin(), ingestionRootStatus.end(), lowerCase.begin(), [](unsigned char c) { return tolower(c); });

		if (lowerCase == "notcompleted")
			return IngestionRootStatus::NotCompleted;
		else if (lowerCase == "completedsuccessful")
			return IngestionRootStatus::CompletedSuccessful;
		else if (lowerCase == "completedwithfailures")
			return IngestionRootStatus::CompletedWithFailures;
		else
			throw std::runtime_error(std::string("Wrong IngestionRootStatus") + ", ingestionRootStatus: " + ingestionRootStatus);
	}

	enum class CrossReferenceType
	{
		ImageOfVideo,
		VideoOfImage, // will be converted to ImageOfVideo
		ImageOfAudio,
		AudioOfImage, // will be converted to ImageOfAudio
		FaceOfVideo,
		VideoOfFace, // will be converted to FaceOfVideo
		SlideShowOfImage,
		ImageForSlideShow, // will be converted to SlideShowOfImage
		SlideShowOfAudio,
		AudioForSlideShow, // will be converted to SlideShowOfAudio
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
		case CrossReferenceType::SlideShowOfImage:
			return "SlideShowOfImage";
		case CrossReferenceType::ImageForSlideShow:
			return "ImageForSlideShow";
		case CrossReferenceType::SlideShowOfAudio:
			return "SlideShowOfAudio";
		case CrossReferenceType::AudioForSlideShow:
			return "AudioForSlideShow";
		case CrossReferenceType::CutOfVideo:
			return "CutOfVideo";
		case CrossReferenceType::CutOfAudio:
			return "CutOfAudio";
		case CrossReferenceType::PosterOfVideo:
			return "PosterOfVideo";
		case CrossReferenceType::VideoOfPoster:
			return "VideoOfPoster";
		default:
			throw std::runtime_error(std::string("Wrong CrossReferenceType"));
		}
	}
	static CrossReferenceType toCrossReferenceType(const std::string &crossReferenceType)
	{
		std::string lowerCase;
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
		else if (lowerCase == "slideshowofimage")
			return CrossReferenceType::SlideShowOfImage;
		else if (lowerCase == "imageforslideshow")
			return CrossReferenceType::ImageForSlideShow;
		else if (lowerCase == "slideshowofaudio")
			return CrossReferenceType::SlideShowOfAudio;
		else if (lowerCase == "audioforslideshow")
			return CrossReferenceType::AudioForSlideShow;
		else if (lowerCase == "cutofvideo")
			return CrossReferenceType::CutOfVideo;
		else if (lowerCase == "cutofaudio")
			return CrossReferenceType::CutOfAudio;
		else if (lowerCase == "posterofvideo")
			return CrossReferenceType::PosterOfVideo;
		else if (lowerCase == "videoofposter")
			return CrossReferenceType::VideoOfPoster;
		else
			throw std::runtime_error(std::string("Wrong CrossReferenceType") + ", crossReferenceType: " + crossReferenceType);
	}

  public:
	MMSEngineDBFacade(
		const nlohmann::json &configuration, nlohmann::json slowQueryConfigurationRoot, size_t masterDbPoolSize, size_t slaveDbPoolSize, std::shared_ptr<spdlog::logger> logger
	);

	~MMSEngineDBFacade();

	// std::vector<std::shared_ptr<Customer>> getCustomers();

	std::shared_ptr<Workspace> getWorkspace(int64_t workspaceKey);

	// std::shared_ptr<Workspace> getWorkspace(std::string workspaceName);

	nlohmann::json getWorkspaceList(int64_t userKey, bool admin, bool costDetails);

	nlohmann::json getLoginWorkspace(int64_t userKey, bool fromMaster);

	int64_t addInvoice(int64_t userKey, std::string description, int amount, std::string expirationDate);

	nlohmann::json getInvoicesList(int64_t userKey, bool admin, int start, int rows);

	std::string createCode(
		int64_t workspaceKey, int64_t userKey, const std::string &userEmail, CodeType codeType, bool admin, bool createRemoveWorkspace,
		bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration,
		bool killEncoding, bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder, bool createRemoveLiveChannel,
		bool updateEncoderStats
	);

#ifdef __POSTGRES__
	std::tuple<int64_t, int64_t, std::string> registerUserAndAddWorkspace(
		const std::string &userName, const std::string &userEmailAddress, const std::string &userPassword, const std::string &userCountry,
		std::string userTimezone, const std::string &workspaceName, const std::string &notes, WorkspaceType workspaceType,
		const std::string &deliveryURL, EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod, long maxIngestionsNumber,
		long maxStorageInMB, const std::string &languageCode, const std::string &workspaceTimezone,
		std::chrono::system_clock::time_point userExpirationLocalDate
	);
#else
	std::tuple<int64_t, int64_t, std::string> registerUserAndAddWorkspace(
		std::string userName, std::string userEmailAddress, std::string userPassword, std::string userCountry, std::string workspaceName, WorkspaceType workspaceType,
		std::string deliveryURL, EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod, long maxIngestionsNumber, long maxStorageInMB,
		std::string languageCode, std::chrono::system_clock::time_point userExpirationDate
	);
#endif

#ifdef __POSTGRES__
	std::tuple<int64_t, int64_t, std::string> registerUserAndShareWorkspace(
		std::string userName, std::string userEmailAddress, std::string userPassword, std::string userCountry, std::string userTimezone, std::string shareWorkspaceCode,
		std::chrono::system_clock::time_point userExpirationDate
	);
#else
	std::tuple<int64_t, int64_t, std::string> registerUserAndShareWorkspace(
		std::string userName, std::string userEmailAddress, std::string userPassword, std::string userCountry, std::string shareWorkspaceCode,
		std::chrono::system_clock::time_point userExpirationDate
	);
#endif

	std::pair<int64_t, std::string> createWorkspace(
		int64_t userKey, const std::string& workspaceName, const std::string& notes, WorkspaceType workspaceType, const std::string& deliveryURL, EncodingPriority maxEncodingPriority,
		EncodingPeriod encodingPeriod, long maxIngestionsNumber, long maxStorageInMB, const std::string& languageCode, const std::string& workspaceTimezone, bool admin,
		std::chrono::system_clock::time_point userExpirationDate
	);

	std::vector<std::tuple<int64_t, std::string, std::string>> deleteWorkspace(int64_t userKey, int64_t workspaceKey);
#ifdef __POSTGRES__
	std::tuple<bool, std::string, std::string> unshareWorkspace(int64_t userKey, int64_t workspaceKey);
#endif

	std::tuple<std::string, std::string, std::string> confirmRegistration(std::string confirmationCode, int expirationInDaysWorkspaceDefaultValue);

#ifdef __POSTGRES__
	std::pair<int64_t, std::string> registerActiveDirectoryUser(
		const std::string &userName, const std::string &userEmailAddress, const std::string &userCountry, std::string userTimezone,
		bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder,
		bool createRemoveLiveChannel, bool updateEncoderStats, const std::string &defaultWorkspaceKeys, int expirationInDaysWorkspaceDefaultValue,
		std::chrono::system_clock::time_point userExpirationLocalDate
	);
#else
	std::pair<int64_t, std::string> registerActiveDirectoryUser(
		std::string userName, std::string userEmailAddress, std::string userCountry, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles,
		bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob,
		bool editEncodersPool, bool applicationRecorder, std::string defaultWorkspaceKeys, int expirationInDaysWorkspaceDefaultValue,
		std::chrono::system_clock::time_point userExpirationDate
	);
#endif

	std::string createAPIKeyForActiveDirectoryUser(
		int64_t userKey, const std::string& userEmailAddress, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles,
		bool deliveryAuthorization,
		bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
		bool applicationRecorder, bool createRemoveLiveChannel, bool updateEncoderStats, int64_t workspaceKey,
		int expirationInDaysWorkspaceDefaultValue
	);

	std::pair<std::string, std::string> getUserDetails(int64_t userKey, std::chrono::milliseconds *sqlDuration = nullptr);
#ifdef __POSTGRES__
	std::pair<int64_t, std::string> getUserDetailsByEmail(std::string email, bool warningIfError);
#else
	std::pair<int64_t, std::string> getUserDetailsByEmail(std::string email);
#endif

	std::tuple<int64_t, std::shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>
		checkAPIKey(const std::string_view &apiKey, bool fromMaster);

	nlohmann::json login(std::string eMailAddress, std::string password);

	int64_t saveLoginStatistics(int userKey, std::string ip);

#ifdef __POSTGRES__
	// void saveGEOInfo(std::string ipAddress, transaction_base *trans, std::shared_ptr<PostgresConnection> conn);
	nlohmann::json getGEOInfo(int64_t geoInfoKey);
	void updateRequestStatisticGEOInfo();
	void updateLoginStatisticGEOInfo();
	std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string>> getGEOInfo_ipAPI(std::vector<std::string> &ips);
	std::vector<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string>> getGEOInfo_ipwhois(std::vector<std::string> &ips);
#else
#endif

	std::pair<int64_t, int64_t> getWorkspaceUsage(int64_t workspaceKey);

#ifdef __POSTGRES__
	nlohmann::json getWorkspaceCost(PostgresConnTrans &trans, int64_t workspaceKey);
#else
#endif

#ifdef __POSTGRES__
	nlohmann::json updateWorkspaceDetails(
		int64_t userKey, int64_t workspaceKey, bool notesChanged, const std::string& newNotes, bool enabledChanged, bool newEnabled, bool nameChanged,
		const std::string& newName, bool maxEncodingPriorityChanged, const std::string& newMaxEncodingPriority, bool encodingPeriodChanged,
		const std::string& newEncodingPeriod,
		bool maxIngestionsNumberChanged, int64_t newMaxIngestionsNumber, bool languageCodeChanged, const std::string& newLanguageCode,
		bool timezoneChanged,
		const std::string& newTimezone, bool preferencesChanged, const std::string& newPreferences, bool externalDeliveriesChanged,
		const std::string& newExternalDeliveries,
		bool expirationDateChanged, const std::string& newExpirationDate,

		bool maxStorageInGBChanged, int64_t maxStorageInGB, bool currentCostForStorageChanged, int64_t currentCostForStorage,
		bool dedicatedEncoder_power_1Changed, int64_t dedicatedEncoder_power_1, bool currentCostForDedicatedEncoder_power_1Changed,
		int64_t currentCostForDedicatedEncoder_power_1, bool dedicatedEncoder_power_2Changed, int64_t dedicatedEncoder_power_2,
		bool currentCostForDedicatedEncoder_power_2Changed, int64_t currentCostForDedicatedEncoder_power_2, bool dedicatedEncoder_power_3Changed,
		int64_t dedicatedEncoder_power_3, bool currentCostForDedicatedEncoder_power_3Changed, int64_t currentCostForDedicatedEncoder_power_3,
		bool CDN_type_1Changed, int64_t CDN_type_1, bool currentCostForCDN_type_1Changed, int64_t currentCostForCDN_type_1,
		bool support_type_1Changed, bool support_type_1, bool currentCostForSupport_type_1Changed, int64_t currentCostForSupport_type_1,

		bool newCreateRemoveWorkspace, bool newIngestWorkflow, bool newCreateProfiles, bool newDeliveryAuthorization, bool newShareWorkspace,
		bool newEditMedia, bool newEditConfiguration, bool newKillEncoding, bool newCancelIngestionJob, bool newEditEncodersPool,
		bool newApplicationRecorder, bool newCreateRemoveLiveChannel, bool newUpdateEncoderStats
	);
#else
	nlohmann::json updateWorkspaceDetails(
		int64_t userKey, int64_t workspaceKey, bool enabledChanged, bool newEnabled, bool nameChanged, std::string newName,
		bool maxEncodingPriorityChanged, std::string newMaxEncodingPriority, bool encodingPeriodChanged, std::string newEncodingPeriod,
		bool maxIngestionsNumberChanged, int64_t newMaxIngestionsNumber, bool maxStorageInMBChanged, int64_t newMaxStorageInMB,
		bool languageCodeChanged, std::string newLanguageCode, bool expirationDateChanged, std::string newExpirationDate, bool newCreateRemoveWorkspace,
		bool newIngestWorkflow, bool newCreateProfiles, bool newDeliveryAuthorization, bool newShareWorkspace, bool newEditMedia,
		bool newEditConfiguration, bool newKillEncoding, bool newCancelIngestionJob, bool newEditEncodersPool, bool newApplicationRecorder
	);
#endif

	nlohmann::json setWorkspaceAsDefault(int64_t userKey, int64_t workspaceKey, int64_t workspaceKeyToBeSetAsDefault);

#ifdef __POSTGRES__
	nlohmann::json updateUser(
		bool admin, bool ldapEnabled, int64_t userKey, bool nameChanged, std::string name, bool emailChanged, std::string email, bool countryChanged,
		std::string country, bool timezoneChanged, std::string timezone, bool insolventChanged, bool insolvent, bool expirationDateChanged,
		std::string expirationDate, bool passwordChanged, std::string newPassword, std::string oldPassword
	);
#else
	nlohmann::json updateUser(
		bool admin, bool ldapEnabled, int64_t userKey, bool nameChanged, std::string name, bool emailChanged, std::string email, bool countryChanged,
		std::string country, bool insolventChanged, bool insolvent, bool expirationDateChanged, std::string expirationDate, bool passwordChanged,
		std::string newPassword, std::string oldPassword
	);
#endif

	std::string createResetPasswordToken(int64_t userKey);

	std::pair<std::string, std::string> resetPassword(std::string resetPassworkToken, std::string newPassword);

	void addIngestionJobOutput(int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey);

	long getIngestionJobOutputsCount(int64_t ingestionJobKey, bool fromMaster);

#ifdef __POSTGRES__
	int64_t addEncodingProfilesSetIfNotAlreadyPresent(
		int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, std::string label, bool removeEncodingProfilesIfPresent
	);
#else
	int64_t addEncodingProfilesSetIfNotAlreadyPresent(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, std::string label,
		bool removeEncodingProfilesIfPresent
	);
#endif

#ifdef __POSTGRES__
	int64_t addEncodingProfile(
		PostgresConnTrans &trans, int64_t workspaceKey, std::string label, MMSEngineDBFacade::ContentType contentType,
		DeliveryTechnology deliveryTechnology, std::string jsonProfile, int64_t encodingProfilesSetKey
	);
#else
	int64_t addEncodingProfile(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, std::string label, MMSEngineDBFacade::ContentType contentType,
		DeliveryTechnology deliveryTechnology, std::string nlohmann::jsonProfile,
		int64_t encodingProfilesSetKey // -1 if it is not associated to any Set
	);
#endif

	int64_t addEncodingProfile(
		int64_t workspaceKey, std::string label, MMSEngineDBFacade::ContentType contentType, DeliveryTechnology deliveryTechnology,
		std::string jsonEncodingProfile
	);

	void removeEncodingProfile(int64_t workspaceKey, int64_t encodingProfileKey);

#ifdef __POSTGRES__
	int64_t addEncodingProfileIntoSetIfNotAlreadyPresent(
		int64_t workspaceKey, std::string label, MMSEngineDBFacade::ContentType contentType, int64_t encodingProfilesSetKey
	);
#else
	int64_t addEncodingProfileIntoSetIfNotAlreadyPresent(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, std::string label, MMSEngineDBFacade::ContentType contentType,
		int64_t encodingProfilesSetKey
	);
#endif

	void removeEncodingProfilesSet(int64_t workspaceKey, int64_t encodingProfilesSetKey);

	void getExpiredMediaItemKeysCheckingDependencies(
		std::string processorMMS, std::vector<std::tuple<std::shared_ptr<Workspace>, int64_t, int64_t>> &mediaItemKeyOrPhysicalPathKeyToBeRemoved,
		int maxMediaItemKeysNumber
	);

	int getNotFinishedIngestionDependenciesNumberByIngestionJobKey(int64_t ingestionJobKey, bool fromMaster);

#ifdef __POSTGRES__
	int getNotFinishedIngestionDependenciesNumberByIngestionJobKey(PostgresConnTrans &trans, int64_t ingestionJobKey);
#else
	int getNotFinishedIngestionDependenciesNumberByIngestionJobKey(std::shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey);
#endif

	void getIngestionsToBeManaged(
		std::vector<std::tuple<int64_t, std::string, std::shared_ptr<Workspace>, std::string, std::string, IngestionType, IngestionStatus>> &ingestionsToBeManaged,
		std::string processorMMS, int maxIngestionJobs, int timeBeforeToPrepareResourcesInMinutes, bool onlyTasksNotInvolvingMMSEngineThreads
	);

	void setNotToBeExecutedStartingFromBecauseChunkNotSelected(int64_t ingestionJobKey, std::string processorMMS);

	// void manageMainAndBackupOfRunnungLiveRecordingHA(std::string processorMMS);

	// bool liveRecorderMainAndBackupChunksManagementCompleted(
	// 	int64_t ingestionJobKey);

	// void getRunningLiveRecorderVirtualVODsDetails(
	// 	std::vector<std::tuple<int64_t, int64_t, int, std::string, int, std::string, std::string,
	// int64_t, 	std::string>>& runningLiveRecordersDetails
	// );

#ifdef __POSTGRES__
	// std::shared_ptr<PostgresConnection> beginWorkflow();
#else
	std::shared_ptr<MySQLConnection> beginIngestionJobs();
#endif

#ifdef __POSTGRES__
	int64_t addWorkflow(
		PostgresConnTrans &trans, int64_t workspaceKey, int64_t userKey, std::string rootType, std::string rootLabel, bool rootHidden,
		const std::string_view& metaDataContent
	);
#else
	int64_t addIngestionRoot(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t userKey, std::string rootType, std::string rootLabel, std::string metaDataContent
	);
#endif

#ifdef __POSTGRES__
	void addIngestionJobDependency(
		PostgresConnTrans &trans, int64_t ingestionJobKey, int dependOnSuccess, int64_t dependOnIngestionJobKey, int orderNumber,
		bool referenceOutputDependency
	);
#else
	void addIngestionJobDependency(
		std::shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int dependOnSuccess, int64_t dependOnIngestionJobKey, int orderNumber,
		bool referenceOutputDependency
	);
#endif

	void changeIngestionJobDependency(int64_t previousDependOnIngestionJobKey, int64_t newDependOnIngestionJobKey);

#ifdef __POSTGRES__
	int64_t addIngestionJob(
		PostgresConnTrans &trans, int64_t workspaceKey, int64_t ingestionRootKey, std::string label, std::string metadataContent,
		MMSEngineDBFacade::IngestionType ingestionType, std::string processingStartingFrom, std::vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
		std::vector<int64_t> waitForGlobalIngestionJobKeys
	);
#else
	int64_t addIngestionJob(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t ingestionRootKey, std::string label, std::string metadataContent,
		MMSEngineDBFacade::IngestionType ingestionType, std::string processingStartingFrom, std::vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
		std::vector<int64_t> waitForGlobalIngestionJobKeys
	);
#endif

	void updateIngestionJobMetadataContent(int64_t ingestionJobKey, std::string metadataContent);

#ifdef __POSTGRES__
	void updateIngestionJobMetadataContent(PostgresConnTrans &trans, int64_t ingestionJobKey, std::string metadataContent);
#else
	void updateIngestionJobMetadataContent(std::shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, std::string metadataContent);
#endif

#ifdef __POSTGRES__
	void updateIngestionJobParentGroupOfTasks(PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t parentGroupOfTasksIngestionJobKey);
#else
	void updateIngestionJobParentGroupOfTasks(std::shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int64_t parentGroupOfTasksIngestionJobKey);
#endif

	void updateIngestionJob_LiveRecorder(
		int64_t workspaceKey, int64_t ingestionJobKey, bool ingestionJobLabelModified, std::string newIngestionJobLabel, bool channelLabelModified,
		std::string newChannerLabel, bool recordingPeriodStartModified, std::string newRecordingPeriodStart, bool recordingPeriodEndModified,
		std::string newRecordingPeriodEnd, bool recordingVirtualVODModified, bool newRecordingVirtualVOD, bool admin
	);

	/*
	void getGroupOfTasksChildrenStatus(
		int64_t groupOfTasksIngestionJobKey,
		bool fromMaster,
		std::vector<std::pair<int64_t, MMSEngineDBFacade::IngestionStatus>>&
	groupOfTasksChildrenStatus);
	*/

#ifdef __POSTGRES__
	void endWorkflow(PostgresConnTrans &trans, bool commit, int64_t ingestionRootKey, std::string processedMetadataContent);
#else
	std::shared_ptr<MySQLConnection>
	endIngestionJobs(std::shared_ptr<MySQLConnection> conn, bool commit, int64_t ingestionRootKey, std::string processedMetadataContent);
#endif

	void updateIngestionJob(int64_t ingestionJobKey, IngestionStatus newIngestionStatus, std::string errorMessage, std::string processorMMS = "noToBeUpdated");

	void addIngestionJobErrorMessages(int64_t ingestionJobKey, nlohmann::json &newErrorMessagesRoot);
	// void updateIngestionJobErrorMessages(int64_t ingestionJobKey, std::string errorMessages);

	bool updateIngestionJobSourceDownloadingInProgress(int64_t ingestionJobKey, double downloadingPercentage);

	bool updateIngestionJobSourceUploadingInProgress(int64_t ingestionJobKey, double uploadingPercentage);

	void updateIngestionJobSourceBinaryTransferred(int64_t ingestionJobKey, bool sourceBinaryTransferred);

	std::string ingestionRoot_columnAsString(int64_t workspaceKey, std::string columnName, int64_t ingestionRootKey, bool fromMaster);
	std::pair<int64_t, std::string> workflowQuery_WorkspaceKeyIngestionDate(int64_t ingestionRootKey, bool fromMaster);
	std::shared_ptr<PostgresHelper::SqlResultSet> workflowQuery(
		std::vector<std::string> &requestedColumns, int64_t workspaceKey, int64_t ingestionRootKey, bool fromMaster, int startIndex = -1, int rows = -1,
		std::string orderBy = "", bool notFoundAsException = true
	);

	// std::tuple<std::string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, std::string, std::string>
	// getIngestionJobDetails(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);

	std::pair<MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>
	ingestionJob_IngestionTypeStatus(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);
	std::string ingestionJob_columnAsString(int64_t workspaceKey, std::string columnName, int64_t ingestionJobKey, bool fromMaster);
	MMSEngineDBFacade::IngestionStatus ingestionJob_Status(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);
	std::tuple<std::string, MMSEngineDBFacade::IngestionType, nlohmann::json, std::string>
	ingestionJob_LabelIngestionTypeMetadataContentErrorMessage(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);
	std::pair<MMSEngineDBFacade::IngestionType, nlohmann::json>
	ingestionJob_IngestionTypeMetadataContent(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);
	MMSEngineDBFacade::IngestionType ingestionJob_IngestionType(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);
	nlohmann::json ingestionJob_columnAsJson(int64_t workspaceKey, std::string columnName, int64_t ingestionJobKey, bool fromMaster);
	std::tuple<MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, nlohmann::json>
	ingestionJob_IngestionTypeStatusMetadataContent(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);
	std::pair<MMSEngineDBFacade::IngestionStatus, nlohmann::json> ingestionJob_StatusMetadataContent(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster);
	void ingestionJob_IngestionJobKeys(int64_t workspaceKey, std::string label, bool fromMaster, std::vector<int64_t> &ingestionJobsKey);
	std::shared_ptr<PostgresHelper::SqlResultSet> ingestionJobQuery(
		std::vector<std::string> &requestedColumns, int64_t workspaceKey, int64_t ingestionJobKey, std::string label, bool fromMaster, int startIndex = -1,
		int rows = -1, std::string orderBy = "", bool notFoundAsException = true
	);

	nlohmann::json getIngestionRootsStatus(
		std::shared_ptr<Workspace> workspace, int64_t ingestionRootKey, int64_t mediaItemKey, int start, int rows,
		// bool startAndEndIngestionDatePresent,
		std::string startIngestionDate, std::string endIngestionDate, std::string label, std::string status, bool asc, bool dependencyInfo, bool ingestionJobOutputs,
		bool hiddenToo, bool fromMaster
	);

	nlohmann::json getIngestionJobsStatus(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, int start, int rows, const std::string &label, bool labelLike,
		/* bool startAndEndIngestionDatePresent, */
		const std::string &startIngestionDate, const std::string &endIngestionDate, const std::string &startScheduleDate,
		const std::string &ingestionType, const std::string &configurationLabel, const std::string &outputChannelLabel, int64_t recordingCode,
		bool broadcastIngestionJobKeyNotNull, std::string jsonParametersCondition, bool asc, const std::string &status, bool dependencyInfo,
		bool ingestionJobOutputs, bool fromMaster
	);

	nlohmann::json getEncodingJobsStatus(
		std::shared_ptr<Workspace> workspace, int64_t encodingJobKey, int start, int rows,
		// bool startAndEndIngestionDatePresent,
		std::string startIngestionDate, std::string endIngestionDate,
		// bool startAndEndEncodingDatePresent,
		std::string startEncodingDate, std::string endEncodingDate, int64_t encoderKey, bool alsoEncodingJobsFromOtherWorkspaces, bool asc, std::string status,
		std::string types, bool fromMaster
	);

#ifdef __POSTGRES__
	nlohmann::json updateMediaItem(
		int64_t workspaceKey, int64_t mediaItemKey, bool titleModified, std::string newTitle, bool userDataModified, std::string newUserData,
		bool retentionInMinutesModified, int64_t newRetentionInMinutes, bool tagsModified, nlohmann::json tagsRoot, bool uniqueNameModified,
		std::string newUniqueName, nlohmann::json crossReferencesRoot, bool admin
	);
#else
	nlohmann::json updateMediaItem(
		int64_t workspaceKey, int64_t mediaItemKey, bool titleModified, std::string newTitle, bool userDataModified, std::string newUserData,
		bool retentionInMinutesModified, int64_t newRetentionInMinutes, bool tagsModified, nlohmann::json tagsRoot, bool uniqueNameModified,
		std::string newUniqueName, bool admin
	);
#endif

	nlohmann::json updatePhysicalPath(int64_t workspaceKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t newRetentionInMinutes, bool admin);

	nlohmann::json getMediaItemsList(
		int64_t workspaceKey, int64_t mediaItemKey, std::string uniqueName, int64_t physicalPathKey, std::vector<int64_t> &otherMediaItemsKey, int start,
		int rows, bool contentTypePresent, ContentType contentType,
		// bool startAndEndIngestionDatePresent,
		std::string startIngestionDate, std::string endIngestionDate, std::string title, int liveRecordingChunk, int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, std::string jsonCondition,
		std::vector<std::string> &tagsIn, std::vector<std::string> &tagsNotIn, std::string orderBy, std::string jsonOrderBy, std::set<std::string> &responseFields, bool admin,
		bool fromMaster
	);

	nlohmann::json getTagsList(
		int64_t workspaceKey, int start, int rows, int liveRecordingChunk, std::optional<ContentType> contentType, std::string tagNameFilter, bool fromMaster
	);

	void updateMediaItem(int64_t mediaItemKey, std::string processorMMSForRetention);

	int64_t
	addUpdateWorkflowAsLibrary(int64_t userKey, int64_t workspaceKey, std::string label, int64_t thumbnailMediaItemKey,
		const std::string_view& jsonWorkflow, bool admin);

	void removeWorkflowAsLibrary(int64_t userKey, int64_t workspaceKey, int64_t workflowLibraryKey, bool admin);

	nlohmann::json getWorkflowsAsLibraryList(int64_t workspaceKey);

	std::string getWorkflowAsLibraryContent(int64_t workspaceKey, int64_t workflowLibraryKey);

	std::string getWorkflowAsLibraryContent(int64_t workspaceKey, std::string label);

	nlohmann::json getEncodingProfilesSetList(int64_t workspaceKey, int64_t encodingProfilesSetKey, std::optional<ContentType> contentType);

	nlohmann::json getEncodingProfileList(int64_t workspaceKey, int64_t encodingProfileKey, std::optional<ContentType> contentType, std::string label);

	int64_t getPhysicalPathDetails(int64_t referenceMediaItemKey, int64_t encodingProfileKey, bool warningIfMissing, bool fromMaster);
	int64_t
	physicalPath_columnAsInt64(std::string columnName, int64_t physicalPathKey, std::chrono::milliseconds *sqlDuration = nullptr, bool fromMaster = false);
	nlohmann::json physicalPath_columnAsJson(std::string columnName, int64_t physicalPathKey, std::chrono::milliseconds *sqlDuration = nullptr, bool fromMaster = false);
	std::shared_ptr<PostgresHelper::SqlResultSet> physicalPathQuery(
		std::vector<std::string> &requestedColumns, int64_t physicalPathKey, bool fromMaster, int startIndex = -1, int rows = -1, std::string orderBy = "",
		bool notFoundAsException = true
	);

	std::string externalUniqueName_columnAsString(
		int64_t workspaceKey, std::string columnName, std::string uniqueName, int64_t mediaItemKey, std::chrono::milliseconds *sqlDuration, bool fromMaster
	);
	int64_t externalUniqueName_columnAsInt64(
		int64_t workspaceKey, std::string columnName, std::string uniqueName, int64_t mediaItemKey, std::chrono::milliseconds *sqlDuration, bool fromMaster
	);
	std::shared_ptr<PostgresHelper::SqlResultSet> externalUniqueNameQuery(
		std::vector<std::string> &requestedColumns, int64_t workspaceKey, std::string uniqueName, int64_t mediaItemKey, bool fromMaster, int startIndex = -1,
		int rows = -1, std::string orderBy = "", bool notFoundAsException = true
	);

	int64_t getPhysicalPathDetails(
		int64_t workspaceKey, int64_t mediaItemKey, ContentType contentType, std::string encodingProfileLabel, bool warningIfMissing, bool fromMaster
	);

	std::string getPhysicalPathDetails(int64_t physicalPathKey, bool warningIfMissing, bool fromMaster);

	std::tuple<int64_t, int, std::string, std::string, uint64_t, bool, int64_t> getSourcePhysicalPath(int64_t mediaItemKey, bool fromMaster);

	std::tuple<MMSEngineDBFacade::ContentType, std::string, std::string, std::string, int64_t, int64_t>
	getMediaItemKeyDetails(int64_t workspaceKey, int64_t mediaItemKey, bool warningIfMissing, bool fromMaster);

	std::tuple<int64_t, MMSEngineDBFacade::ContentType, std::string, std::string, std::string, int64_t, std::string, std::string, int64_t>
	getMediaItemKeyDetailsByPhysicalPathKey(int64_t workspaceKey, int64_t physicalPathKey, bool warningIfMissing, bool fromMaster);

	void getMediaItemDetailsByIngestionJobKey(
		int64_t workspaceKey, int64_t referenceIngestionJobKey, int maxLastMediaItemsToBeReturned,
		std::vector<std::tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType>> &mediaItemsDetails, bool warningIfMissing, bool fromMaster
	);

	std::pair<int64_t, MMSEngineDBFacade::ContentType>
	getMediaItemKeyDetailsByUniqueName(int64_t workspaceKey, std::string referenceUniqueName, bool warningIfMissing, bool fromMaster);

	int64_t getMediaDurationInMilliseconds(int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster);

	nlohmann::json mediaItem_columnAsJson(std::string columnName, int64_t mediaItemKey, std::chrono::milliseconds *sqlDuration = nullptr, bool fromMaster = false);
	std::shared_ptr<PostgresHelper::SqlResultSet> mediaItemQuery(
		std::vector<std::string> &requestedColumns, int64_t mediaItemKey, bool fromMaster = false, int startIndex = -1, int rows = -1, std::string orderBy = "",
		bool notFoundAsException = true
	);

	// std::tuple<int64_t,long,std::string,std::string,int,int,std::string,long,std::string,long,int,long>
	// getVideoDetails(
	//     int64_t mediaItemKey, int64_t physicalpathKey);
	void getVideoDetails(
		int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster,
		std::vector<std::tuple<int64_t, int, int64_t, int, int, std::string, std::string, long, std::string>> &videoTracks,
		std::vector<std::tuple<int64_t, int, int64_t, long, std::string, long, int, std::string>> &audioTracks
	);

	// std::tuple<int64_t,std::string,long,long,int> getAudioDetails(
	//     int64_t mediaItemKey, int64_t physicalpathKey);
	void getAudioDetails(
		int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster,
		std::vector<std::tuple<int64_t, int, int64_t, long, std::string, long, int, std::string>> &audioTracks
	);

	std::tuple<int, int, std::string, int> getImageDetails(int64_t mediaItemKey, int64_t physicalpathKey, bool fromMaster);

	std::vector<int64_t> getEncodingProfileKeysBySetKey(int64_t workspaceKey, int64_t encodingProfilesSetKey);

	std::vector<int64_t> getEncodingProfileKeysBySetLabel(int64_t workspaceKey, std::string label);

	std::tuple<std::string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, std::string>
	getEncodingProfileDetailsByKey(int64_t workspaceKey, int64_t encodingProfileKey);

	std::tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, std::shared_ptr<Workspace>, std::string, std::string, std::string, std::string, uint64_t, bool>
	getStorageDetails(int64_t physicalPathKey, bool fromMaster);

	std::tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, std::shared_ptr<Workspace>, std::string, std::string, std::string, std::string, uint64_t, bool>
	getStorageDetails(int64_t mediaItemKey, int64_t encodingProfileKey, bool fromMaster);

	void getAllStorageDetails(
		int64_t mediaItemKey, bool fromMaster,
		std::vector<std::tuple<MMSEngineDBFacade::DeliveryTechnology, int, std::string, std::string, std::string, int64_t, bool>> &allStorageDetails
	);

	int64_t createDeliveryAuthorization(
		int64_t userKey, std::string clientIPAddress, int64_t physicalPathKey, int64_t liveDeliveryKey, std::string deliveryURI, int ttlInSeconds,
		int maxRetries, bool reuseAuthIfPresent
	);

	std::shared_ptr<PostgresHelper::SqlResultSet> deliveryAuthorizationQuery(
		std::vector<std::string> &requestedColumns, int64_t deliveryAuthorizationKey, std::string contentType, int64_t contentKey, std::string deliveryURI,
		bool notExpiredCheck, bool fromMaster = false, int startIndex = -1, int rows = -1, std::string orderBy = "", bool notFoundAsException = true
	);

	bool checkDeliveryAuthorization(int64_t deliveryAuthorizationKey, std::string contentURI);

	void resetProcessingJobsIfNeeded(std::string processorMMS);

	void retentionOfIngestionData();

	void retentionOfStatisticData();

	void retentionOfDeliveryAuthorization();

	void fixEncodingJobsHavingWrongStatus();
	void fixIngestionJobsHavingWrongStatus();

	void getToBeProcessedEncodingJobs(
		std::string processorMMS, std::vector<std::shared_ptr<MMSEngineDBFacade::EncodingItem>> &encodingItems, int timeBeforeToPrepareResourcesInMinutes,
		int maxEncodingsNumber
	);
	void recoverEncodingsNotCompleted(std::string processorMMS, std::vector<std::shared_ptr<MMSEngineDBFacade::EncodingItem>> &encodingItems);

	int64_t getEncodingProfileKeyByLabel(
		int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, std::string encodingProfileLabel, bool contentTypeToBeUsed = true
	);

	void addEncodingJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, MMSEngineDBFacade::ContentType contentType,
		EncodingPriority encodingPriority, int64_t encodingProfileKey, nlohmann::json encodingProfileDetailsRoot,

		nlohmann::json sourcesToBeEncodedRoot
	);

	void addEncoding_OverlayImageOnVideoJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, int64_t encodingProfileKey, nlohmann::json encodingProfileDetailsRoot,
		int64_t sourceVideoMediaItemKey, int64_t sourceVideoPhysicalPathKey, int64_t videoDurationInMilliSeconds,
		const std::string &mmsSourceVideoAssetPathName, const std::string &sourceVideoPhysicalDeliveryURL,
		const std::string &sourceVideoFileExtension, int64_t sourceImageMediaItemKey, int64_t sourceImagePhysicalPathKey,
		const std::string &mmsSourceImageAssetPathName, const std::string &sourceImagePhysicalDeliveryURL,
		const std::string &sourceVideoTranscoderStagingAssetPathName, const std::string &encodedTranscoderStagingAssetPathName,
		const std::string &encodedNFSStagingAssetPathName, EncodingPriority encodingPriority
	);

	void addEncoding_OverlayTextOnVideoJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, EncodingPriority encodingPriority,

		int64_t encodingProfileKey, nlohmann::json encodingProfileDetailsRoot,

		const std::string &sourceAssetPathName, int64_t sourceDurationInMilliSeconds, const std::string &sourcePhysicalDeliveryURL,
		const std::string &sourceFileExtension,

		const std::string &sourceTranscoderStagingAssetPathName, const std::string &encodedTranscoderStagingAssetPathName,
		const std::string &encodedNFSStagingAssetPathName
	);

	void addEncoding_GenerateFramesJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, EncodingPriority encodingPriority,
		const std::string &nfsImagesDirectory, const std::string &transcoderStagingImagesDirectory, const std::string &sourcePhysicalDeliveryURL,
		const std::string &sourceTranscoderStagingAssetPathName, const std::string &sourceAssetPathName, int64_t sourceVideoPhysicalPathKey,
		const std::string &sourceFileExtension, const std::string &sourceFileName, int64_t videoDurationInMilliSeconds, double startTimeInSeconds,
		int maxFramesNumber, const std::string &videoFilter, int periodInSeconds, bool mjpeg, int imageWidth, int imageHeight
	);

	void addEncoding_SlideShowJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, int64_t encodingProfileKey, nlohmann::json encodingProfileDetailsRoot,
		const std::string &targetFileFormat, nlohmann::json imagesRoot, nlohmann::json audiosRoot, float shortestAudioDurationInSeconds,
		const std::string &encodedTranscoderStagingAssetPathName, const std::string &encodedNFSStagingAssetPathName, EncodingPriority encodingPriority
	);

	void addEncoding_FaceRecognitionJob(
		std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t sourceMediaItemKey, int64_t sourceVideoPhysicalPathKey,
		std::string sourcePhysicalPath, std::string faceRecognitionCascadeName, std::string faceRecognitionOutput, EncodingPriority encodingPriority,
		long initialFramesNumberToBeSkipped, bool oneFramePerSecond
	);

	void addEncoding_FaceIdentificationJob(
		std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey, std::string sourcePhysicalPath, std::string faceIdentificationCascadeName,
		std::string deepLearnedModelTagsCommaSeparated, EncodingPriority encodingPriority
	);

	void addEncoding_LiveRecorderJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, std::string ingestionJobLabel, std::string streamSourceType,
		// bool highAvailability,
		std::string configurationLabel, int64_t confKey, std::string liveURL, std::string encodersPoolLabel, EncodingPriority encodingPriority,

		int pushListenTimeout, int64_t pushEncoderKey, nlohmann::json captureRoot, nlohmann::json tvRoot,

		bool monitorHLS, bool liveRecorderVirtualVOD, int monitorVirtualVODOutputRootIndex,

		const nlohmann::json &outputsRoot, nlohmann::json framesToBeDetectedRoot,

		const std::string &chunksTranscoderStagingContentsPath, const std::string &chunksNFSStagingContentsPath,
		const std::string &segmentListFileName, const std::string &recordedFileNamePrefix, const std::string &virtualVODStagingContentsPath,
		const std::string &virtualVODTranscoderStagingContentsPath, int64_t liveRecorderVirtualVODImageMediaItemKey
	);

	void addEncoding_LiveProxyJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, nlohmann::json inputsRoot, std::string streamSourceType,
		int64_t utcProxyPeriodStart,
		// int64_t utcProxyPeriodEnd,
		// long maxAttemptsNumberInCaseOfErrors,
		long waitingSecondsBetweenAttemptsInCaseOfErrors, const nlohmann::json &outputsRoot
	);

	void addEncoding_VODProxyJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, nlohmann::json inputsRoot, int64_t utcProxyPeriodStart,
		const nlohmann::json &outputsRoot, long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors
	);

	void addEncoding_CountdownJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, nlohmann::json inputsRoot, int64_t utcProxyPeriodStart,
		nlohmann::json outputsRoot, long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors
	);

	void addEncoding_LiveGridJob(std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey, nlohmann::json inputChannelsRoot, nlohmann::json outputsRoot);

	void addEncoding_VideoSpeed(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,
		const std::string &sourceAssetPathName, int64_t sourceDurationInMilliSeconds, const std::string &sourceFileExtension,
		const std::string &sourcePhysicalDeliveryURL, const std::string &sourceTranscoderStagingAssetPathName, int64_t encodingProfileKey,
		nlohmann::json encodingProfileDetailsRoot, const std::string &encodedTranscoderStagingAssetPathName,
		const std::string &encodedNFSStagingAssetPathName, EncodingPriority encodingPriority
	);

	void addEncoding_AddSilentAudio(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, nlohmann::json sourcesRoot, int64_t encodingProfileKey,
		nlohmann::json encodingProfileDetailsRoot, EncodingPriority encodingPriority
	);

	void addEncoding_PictureInPictureJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, int64_t mainSourceMediaItemKey, int64_t mainSourcePhysicalPathKey,
		const std::string &mainSourceAssetPathName, int64_t mainSourceDurationInMilliSeconds, const std::string &mainSourceFileExtension,
		const std::string &mainSourcePhysicalDeliveryURL, const std::string &mainSourceTranscoderStagingAssetPathName,
		int64_t overlaySourceMediaItemKey, int64_t overlaySourcePhysicalPathKey, const std::string &overlaySourceAssetPathName,
		int64_t overlaySourceDurationInMilliSeconds, const std::string &overlaySourceFileExtension,
		const std::string &overlaySourcePhysicalDeliveryURL, const std::string &overlaySourceTranscoderStagingAssetPathName, bool soundOfMain,
		int64_t encodingProfileKey, nlohmann::json encodingProfileDetailsRoot, const std::string &encodedTranscoderStagingAssetPathName,
		const std::string &encodedNFSStagingAssetPathName, EncodingPriority encodingPriority
	);

	void addEncoding_IntroOutroOverlayJob(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey,

		int64_t encodingProfileKey, nlohmann::json encodingProfileDetailsRoot,

		int64_t introSourcePhysicalPathKey, const std::string &introSourceAssetPathName, const std::string &introSourceFileExtension,
		int64_t introSourceDurationInMilliSeconds, const std::string &introSourcePhysicalDeliveryURL,
		const std::string &introSourceTranscoderStagingAssetPathName,

		int64_t mainSourcePhysicalPathKey, const std::string &mainSourceAssetPathName, const std::string &mainSourceFileExtension,
		int64_t mainSourceDurationInMilliSeconds, const std::string &mainSourcePhysicalDeliveryURL,
		const std::string &mainSourceTranscoderStagingAssetPathName,

		int64_t outroSourcePhysicalPathKey, const std::string &outroSourceAssetPathName, const std::string &outroSourceFileExtension,
		int64_t outroSourceDurationInMilliSeconds, const std::string &outroSourcePhysicalDeliveryURL,
		const std::string &outroSourceTranscoderStagingAssetPathName,

		const std::string &encodedTranscoderStagingAssetPathName, const std::string &encodedNFSStagingAssetPathName,

		EncodingPriority encodingPriority
	);

	void addEncoding_CutFrameAccurate(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey,

		int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey, const std::string &sourceAssetPathName, int64_t sourceDurationInMilliSeconds,
		const std::string &sourceFileExtension, const std::string &sourcePhysicalDeliveryURL, const std::string &sourceTranscoderStagingAssetPathName,
		const std::string &endTime,

		int64_t encodingProfileKey, nlohmann::json encodingProfileDetailsRoot,

		const std::string &encodedTranscoderStagingAssetPathName, const std::string &encodedNFSStagingAssetPathName,

		EncodingPriority encodingPriority, int64_t newUtcStartTimeInMilliSecs, int64_t newUtcEndTimeInMilliSecs
	);

	void updateIngestionAndEncodingLiveRecordingPeriod(
		int64_t ingestionJobKey, int64_t encodingJobKey, time_t utcRecordingPeriodStart, time_t utcRecordingPeriodEnd
	);

	int updateEncodingJob(
		int64_t encodingJobKey, EncodingError encodingError, bool isIngestionJobFinished, int64_t ingestionJobKey, std::string ingestionErrorMessage = "",
		bool forceEncodingToBeFailed = false
	);

	void forceCancelEncodingJob(int64_t ingestionJobKey);

	void updateEncodingJobPriority(std::shared_ptr<Workspace> workspace, int64_t encodingJobKey, EncodingPriority newEncodingPriority);

	void updateEncodingJobTryAgain(std::shared_ptr<Workspace> workspace, int64_t encodingJobKey);

	void updateEncodingJobProgressAndRealTimeInfo(int64_t encodingJobKey, std::optional<double> encodingPercentage, const nlohmann::json &realTimeInfoRoot);

	void updateEncodingPid(int64_t encodingJobKey, int encodingPid, long numberOfRestartBecauseOfFailure);

	bool updateEncodingJobFailuresNumber(int64_t encodingJobKey, long failuresNumber);

	void updateEncodingJobIsKilled(int64_t encodingJobKey, bool isKilled);

	void updateEncodingJobTranscoder(int64_t encodingJobKey, int64_t encoderKey, std::string stagingEncodedAssetPathName);

	void updateEncodingJobParameters(int64_t encodingJobKey, std::string parameters);

	void updateOutputURL(int64_t ingestionJobKey, int64_t encodingJobKey, int outputIndex, bool srtFeed, std::string outputURL);

	void updateOutputHLSDetails(
		int64_t ingestionJobKey, int64_t encodingJobKey, int outputIndex, int64_t deliveryCode, int segmentDurationInSeconds,
		int playlistEntriesNumber, std::string manifestDirectoryPath, std::string manifestFileName, std::string otherOutputOptions
	);

	nlohmann::json encodingJob_columnAsJson(std::string columnName, int64_t encodingJobKey, bool fromMaster);
	std::pair<int64_t, nlohmann::json> encodingJob_EncodingJobKeyParameters(int64_t ingestionJobKey, bool fromMaster);
	std::tuple<int64_t, std::string, int64_t, MMSEngineDBFacade::EncodingStatus>
	encodingJob_IngestionJobKeyTypeEncoderKeyStatus(int64_t encodingJobKey, bool fromMaster);
	std::tuple<int64_t, int64_t, nlohmann::json> encodingJob_EncodingJobKeyEncoderKeyParameters(int64_t ingestionJobKey, bool fromMaster);
	std::pair<int64_t, int64_t> encodingJob_EncodingJobKeyEncoderKey(int64_t ingestionJobKey, bool fromMaster);
	std::shared_ptr<PostgresHelper::SqlResultSet> encodingJobQuery(
		std::vector<std::string> &requestedColumns, int64_t encodingJobKey, int64_t ingestionJobKey, bool fromMaster, int startIndex = -1, int rows = -1,
		std::string orderBy = "", bool notFoundAsException = true
	);

	void checkWorkspaceStorageAndMaxIngestionNumber(int64_t workspaceKey);

	std::string nextRelativePathToBeUsed(int64_t workspaceKey);

	std::pair<int64_t, int64_t> saveSourceContentMetadata(
		std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey, bool ingestionRowToBeUpdatedAsSuccess, MMSEngineDBFacade::ContentType contentType,
		int64_t encodingProfileKey, nlohmann::json parametersRoot, bool externalReadOnlyStorage, std::string relativePath, std::string mediaSourceFileName,
		int mmsPartitionIndexUsed, unsigned long sizeInBytes,

		// video-audio
		std::tuple<int64_t, long, nlohmann::json> &mediaInfoDetails, std::vector<std::tuple<int, int64_t, std::string, std::string, int, int, std::string, long>> &videoTracks,
		std::vector<std::tuple<int, int64_t, std::string, long, int, long, std::string>> &audioTracks,

		// image
		int imageWidth, int imageHeight, std::string imageFormat, int imageQuality
	);

	int64_t saveVariantContentMetadata(
		int64_t workspaceKey, int64_t ingestionJobKey, int64_t liveRecordingIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
		std::string externalDeliveryTechnology, std::string externalDeliveryURL, std::string encodedFileName, std::string relativePath, int mmsPartitionIndexUsed,
		unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

		// video-audio
		std::tuple<int64_t, long, nlohmann::json> &mediaInfoDetails, std::vector<std::tuple<int, int64_t, std::string, std::string, int, int, std::string, long>> &videoTracks,
		std::vector<std::tuple<int, int64_t, std::string, long, int, long, std::string>> &audioTracks,
		/*
		int64_t durationInMilliSeconds,
		long bitRate,
		std::string videoCodecName,
		std::string videoProfile,
		int videoWidth,
		int videoHeight,
		std::string videoAvgFrameRate,
		long videoBitRate,
		std::string audioCodecName,
		long audioSampleRate,
		int audioChannels,
		long audioBitRate,
		*/

		// image
		int imageWidth, int imageHeight, std::string imageFormat, int imageQuality
	);

	/*
	void updateLiveRecorderVirtualVOD (
		int64_t workspaceKey,
		std::string liveRecorderVirtualVODUniqueName,
		int64_t mediaItemKey,
		int64_t physicalPathKey,

		int newRetentionInMinutes,

		int64_t firstUtcChunkStartTime,
		std::string sFirstUtcChunkStartTime,
		int64_t lastUtcChunkEndTime,
		std::string sLastUtcChunkEndTime,
		std::string title,
		int64_t durationInMilliSeconds,
		long bitRate,
		unsigned long long sizeInBytes,

		std::vector<std::tuple<int, int64_t, std::string, std::string, int, int, std::string, long>>&
	videoTracks, std::vector<std::tuple<int, int64_t, std::string, long, int, long, std::string>>&
	audioTracks
	);
	*/

	void addCrossReference(
		int64_t ingestionJobKey, int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
		nlohmann::json crossReferenceParametersRoot
	);

	void removePhysicalPath(int64_t physicalPathKey);

	void removeMediaItem(int64_t mediaItemKey);

	nlohmann::json addYouTubeConf(int64_t workspaceKey, std::string label, std::string tokenType, std::string refreshToken, std::string accessToken);

	nlohmann::json modifyYouTubeConf(
		int64_t confKey, int64_t workspaceKey, std::string label, bool labelModified, std::string tokenType, bool tokenTypeModified, std::string refreshToken,
		bool refreshTokenModified, std::string accessToken, bool accessTokenModified
	);

	void removeYouTubeConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getYouTubeConfList(int64_t workspaceKey, std::string label);

	std::tuple<std::string, std::string, std::string> getYouTubeDetailsByConfigurationLabel(int64_t workspaceKey, std::string youTubeConfigurationLabel);

	int64_t addFacebookConf(int64_t workspaceKey, std::string label, std::string userAccessToken);

	void modifyFacebookConf(int64_t confKey, int64_t workspaceKey, std::string label, std::string userAccessToken);

	void removeFacebookConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getFacebookConfList(int64_t workspaceKey, int64_t confKey, std::string label);

	std::string getFacebookUserAccessTokenByConfigurationLabel(int64_t workspaceKey, std::string facebookConfigurationLabel);

	int64_t addTwitchConf(int64_t workspaceKey, std::string label, std::string refreshToken);

	void modifyTwitchConf(int64_t confKey, int64_t workspaceKey, std::string label, std::string refreshToken);

	void removeTwitchConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getTwitchConfList(int64_t workspaceKey, int64_t confKey, std::string label);

	std::string getTwitchUserAccessTokenByConfigurationLabel(int64_t workspaceKey, std::string twitchConfigurationLabel);

	int64_t addTiktokConf(int64_t workspaceKey, std::string label, std::string token);

	void modifyTiktokConf(int64_t confKey, int64_t workspaceKey, std::string label, std::string token);

	void removeTiktokConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getTiktokConfList(int64_t workspaceKey, int64_t confKey, std::string label);

	std::string getTiktokTokenByConfigurationLabel(int64_t workspaceKey, std::string tiktokConfigurationLabel);

	nlohmann::json addStream(
		int64_t workspaceKey, std::string label, std::string sourceType, int64_t encodersPoolKey, std::string url, std::string pushProtocol, int64_t pushEncoderKey,
		bool pushPublicEncoderName, int pushServerPort, std::string pushUri, int pushListenTimeout, int captureVideoDeviceNumber,
		std::string captureVideoInputFormat, int captureFrameRate, int captureWidth, int captureHeight, int captureAudioDeviceNumber,
		int captureChannelsNumber, int64_t tvSourceTVConfKey, std::string type, std::string description, std::string name, std::string region, std::string country,
		int64_t imageMediaItemKey, std::string imageUniqueName, int position, nlohmann::json userData
	);

	nlohmann::json modifyStream(
		int64_t confKey, std::string labelKey, int64_t workspaceKey, bool labelToBeModified, std::string label, bool sourceTypeToBeModified, std::string sourceType,
		bool encodersPoolKeyToBeModified, int64_t encodersPoolKey, bool urlToBeModified, std::string url, bool pushProtocolToBeModified,
		std::string pushProtocol, bool pushEncoderKeyToBeModified, int64_t pushEncoderKey, bool pushPublicEncoderNameToBeModified,
		bool pushPublicEncoderName, bool pushServerPortToBeModified, int pushServerPort, bool pushUriToBeModified, std::string pushUri,
		bool pushListenTimeoutToBeModified, int pushListenTimeout, bool captureVideoDeviceNumberToBeModified, int captureVideoDeviceNumber,
		bool captureVideoInputFormatToBeModified, std::string captureVideoInputFormat, bool captureFrameRateToBeModified, int captureFrameRate,
		bool captureWidthToBeModified, int captureWidth, bool captureHeightToBeModified, int captureHeight, bool captureAudioDeviceNumberToBeModified,
		int captureAudioDeviceNumber, bool captureChannelsNumberToBeModified, int captureChannelsNumber, bool tvSourceTVConfKeyToBeModified,
		int64_t tvSourceTVConfKey, bool typeToBeModified, std::string type, bool descriptionToBeModified, std::string description, bool nameToBeModified,
		std::string name, bool regionToBeModified, std::string region, bool countryToBeModified, std::string country, bool imageToBeModified,
		int64_t imageMediaItemKey, std::string imageUniqueName, bool positionToBeModified, int position, bool userDataToBeModified, nlohmann::json userData
	);

	void removeStream(int64_t workspaceKey, int64_t confKey, std::string label);

#ifdef __POSTGRES__
	nlohmann::json getStreamList(
		int64_t workspaceKey, int64_t liveURLKey, int start, int rows, std::string label, bool labelLike, std::string sourceType, std::string type, std::string name,
		std::string region, std::string country, std::string url, std::string labelOrder, bool fromMaster = false
	);
#else
	nlohmann::json getStreamList(
		int64_t workspaceKey, int64_t liveURLKey, int start, int rows, std::string label, bool labelLike, std::string sourceType, std::string type, std::string name,
		std::string region, std::string country, std::string url, std::string labelOrder
	);
#endif

#ifdef __POSTGRES__
	nlohmann::json getStreamFreePushEncoderPort(int64_t encoderKey, bool fromMaster = false);
#endif

	std::tuple<int64_t, std::string, std::string, std::string, int64_t, bool, int, int, std::string, int, int, int, int, int, int64_t>
	stream_aLot(int64_t workspaceKey, std::string label);
	std::tuple<std::string, std::string, int64_t, bool, int, std::string> stream_pushInfo(int64_t workspaceKey, std::string label);
	std::string stream_columnAsString(int64_t workspaceKey, std::string columnName, int64_t confKey = -1, std::string label = "");
	int64_t stream_columnAsInt64(int64_t workspaceKey, std::string columnName, int64_t confKey = -1, std::string label = "");
	nlohmann::json stream_columnAsJson(int64_t workspaceKey, std::string columnName, int64_t confKey = -1, std::string label = "");
	std::tuple<std::string, std::string, int64_t, bool> stream_sourceTypeEncodersPoolPushEncoderKeyPushPublicEncoderName(int64_t workspaceKey, std::string label);
	std::pair<std::string, std::string> stream_sourceTypeUrl(int64_t workspaceKey, std::string label);
	std::tuple<int64_t, std::string, std::string> stream_confKeySourceTypeUrl(int64_t workspaceKey, std::string label);
	std::shared_ptr<PostgresHelper::SqlResultSet> streamQuery(
		std::vector<std::string> &requestedColumns, int64_t workspaceKey, int64_t confKey = -1, std::string label = "", bool fromMaster = false, int startIndex = -1,
		int rows = -1, std::string orderBy = "", bool notFoundAsException = true
	);
	// std::tuple<int64_t, std::string, std::string, std::string, std::string, int64_t, bool, int, std::string, int, int, std::string, int, int, int, int, int, int64_t>
	// getStreamDetails(int64_t workspaceKey, std::string label, bool warningIfMissing);
	// std::tuple<std::string, std::string, std::string> getStreamDetails(int64_t workspaceKey, int64_t confKey);

	nlohmann::json addSourceTVStream(
		std::string type, int64_t serviceId, int64_t networkId, int64_t transportStreamId, std::string name, std::string satellite, int64_t frequency, std::string lnb,
		int videoPid, std::string audioPids, int audioItalianPid, int audioEnglishPid, int teletextPid, std::string modulation, std::string polarization,
		int64_t symbolRate, int64_t bandwidthInHz, std::string country, std::string deliverySystem
	);

	nlohmann::json modifySourceTVStream(
		int64_t confKey,

		bool typeToBeModified, std::string type, bool serviceIdToBeModified, int64_t serviceId, bool networkIdToBeModified, int64_t networkId,
		bool transportStreamIdToBeModified, int64_t transportStreamId, bool nameToBeModified, std::string name, bool satelliteToBeModified,
		std::string satellite, bool frequencyToBeModified, int64_t frequency, bool lnbToBeModified, std::string lnb, bool videoPidToBeModified, int videoPid,
		bool audioPidsToBeModified, std::string audioPids, bool audioItalianPidToBeModified, int audioItalianPid, bool audioEnglishPidToBeModified,
		int audioEnglishPid, bool teletextPidToBeModified, int teletextPid, bool modulationToBeModified, std::string modulation,
		bool polarizationToBeModified, std::string polarization, bool symbolRateToBeModified, int64_t symbolRate, bool bandwidthInHzToBeModified,
		int64_t bandwidthInHz, bool countryToBeModified, std::string country, bool deliverySystemToBeModified, std::string deliverySystem
	);

	void removeSourceTVStream(int64_t confKey);

#ifdef __POSTGRES__
	nlohmann::json getSourceTVStreamList(
		int64_t confKey, int start, int rows, std::string type, int64_t serviceId, std::string name, int64_t frequency, std::string lnb, int videoPid,
		std::string audioPids, std::string nameOrder, bool fromMaster = false
	);
#else
	nlohmann::json getSourceTVStreamList(
		int64_t confKey, int start, int rows, std::string type, int64_t serviceId, std::string name, int64_t frequency, std::string lnb, int videoPid,
		std::string audioPids, std::string nameOrder
	);
#endif

	std::tuple<std::string, int64_t, int64_t, int64_t, int64_t, std::string, int, int> getSourceTVStreamDetails(int64_t confKey, bool warningIfMissing);

	int64_t addRTMPChannelConf(
		int64_t workspaceKey, const std::string &label, const std::string &rtmpURL, const std::string &streamName, const std::string &userName, const std::string &password,
		const nlohmann::json &playURLDetailsRoot, const std::string &type
	);

	void modifyRTMPChannelConf(
		int64_t confKey, int64_t workspaceKey, const std::string &label, const std::string &rtmpURL, const std::string &streamName, const std::string &userName,
		const std::string &password, const nlohmann::json &playURLDetailsRoot, const std::string &type
	);

	void removeRTMPChannelConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getRTMPChannelConfList(int64_t workspaceKey, int64_t confKey, std::string label, bool labelLike,
								int type); // 0: all, 1: SHARED, 2: DEDICATED

	int64_t getRTMPChannelDetails(int64_t workspaceKey, std::string label, bool warningIfMissing);

	std::tuple<std::string, std::string, std::string, std::string, std::string, bool, nlohmann::json>
		reserveRTMPChannel(int64_t workspaceKey, std::string label, int outputIndex, int64_t ingestionJobKey);
	nlohmann::json rtmp_reservationDetails(int64_t reservedIngestionJobKey, int16_t outputIndex);

	nlohmann::json releaseRTMPChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey);
	int64_t addSRTChannelConf(
		int64_t workspaceKey, std::string label, std::string srtURL, std::string mode, std::string streamId, std::string passphrase, std::string playURL, std::string type
	);

	void modifySRTChannelConf(
		int64_t confKey, int64_t workspaceKey, std::string label, std::string srtURL, std::string mode, std::string streamId, std::string passphrase, std::string playURL,
		std::string type
	);

	void removeSRTChannelConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getSRTChannelConfList(int64_t workspaceKey, int64_t confKey, std::string label, bool labelLike,
							   int type); // 0: all, 1: SHARED, 2: DEDICATED

	std::tuple<int64_t, std::string, std::string, std::string, std::string, std::string> getSRTChannelDetails(int64_t workspaceKey, std::string label, bool warningIfMissing);

	std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, bool>
	reserveSRTChannel(int64_t workspaceKey, std::string label, int outputIndex, int64_t ingestionJobKey);
	std::string srt_reservationDetails(int64_t reservedIngestionJobKey, int16_t outputIndex);

	void releaseSRTChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey);

	int64_t addHLSChannelConf(int64_t workspaceKey, std::string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, std::string type);

	void modifyHLSChannelConf(
		int64_t confKey, int64_t workspaceKey, std::string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, std::string type
	);

	void removeHLSChannelConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getHLSChannelConfList(int64_t workspaceKey, int64_t confKey, std::string label, bool labelLike,
							   int type); // 0: all, 1: SHARED, 2: DEDICATED

	std::tuple<int64_t, int64_t, int, int> getHLSChannelDetails(int64_t workspaceKey, std::string label, bool warningIfMissing);

	std::tuple<std::string, int64_t, int, int, bool> reserveHLSChannel(int64_t workspaceKey, std::string label, int outputIndex, int64_t ingestionJobKey);

	void releaseHLSChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey);

	int64_t addFTPConf(int64_t workspaceKey, std::string label, std::string server, int port, std::string userName, std::string password, std::string remoteDirectory);

	void modifyFTPConf(
		int64_t confKey, int64_t workspaceKey, std::string label, std::string server, int port, std::string userName, std::string password, std::string remoteDirectory
	);

	void removeFTPConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getFTPConfList(int64_t workspaceKey);

	std::tuple<std::string, int, std::string, std::string, std::string> getFTPByConfigurationLabel(int64_t workspaceKey, std::string liveURLConfigurationLabel);

	int64_t addEMailConf(int64_t workspaceKey, std::string label, std::string addresses, std::string subject, std::string message);

	void modifyEMailConf(int64_t confKey, int64_t workspaceKey, std::string label, std::string addresses, std::string subject, std::string message);

	void removeEMailConf(int64_t workspaceKey, int64_t confKey);

	nlohmann::json getEMailConfList(int64_t workspaceKey);

	std::tuple<std::string, std::string, std::string> getEMailByConfigurationLabel(int64_t workspaceKey, std::string liveURLConfigurationLabel);

	nlohmann::json addRequestStatistic(int64_t workspaceKey, std::string ipAddress, std::string userId, int64_t physicalPathKey, int64_t confStreamKey, std::string title);

	nlohmann::json getRequestStatisticList(int64_t workspaceKey, std::string userId, std::string title, std::string startDate, std::string endDate, int start, int rows);

	nlohmann::json getRequestStatisticPerContentList(
		int64_t workspaceKey, std::string title, std::string userId, std::string startDate, std::string endDate, int64_t minimalNextRequestDistanceInSeconds,
		bool totalNumFoundToBeCalculated, int start, int rows
	);

	nlohmann::json getRequestStatisticPerUserList(
		int64_t workspaceKey, std::string title, std::string userId, std::string startDate, std::string endDate, int64_t minimalNextRequestDistanceInSeconds,
		bool totalNumFoundToBeCalculated, int start, int rows
	);

	nlohmann::json getRequestStatisticPerMonthList(
		int64_t workspaceKey, std::string title, std::string userId, std::string startStatisticDate, std::string endStatisticDate,
		int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
	);

	nlohmann::json getRequestStatisticPerDayList(
		int64_t workspaceKey, std::string title, std::string userId, std::string startStatisticDate, std::string endStatisticDate,
		int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
	);

	nlohmann::json getRequestStatisticPerHourList(
		int64_t workspaceKey, std::string title, std::string userId, std::string startStatisticDate, std::string endStatisticDate,
		int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
	);

	nlohmann::json getRequestStatisticPerCountryList(
		int64_t workspaceKey, std::string title, std::string userId, std::string startStatisticDate, std::string endStatisticDate,
		int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
	);

	nlohmann::json getLoginStatisticList(std::string startDate, std::string endDate, int start, int rows);

	void setLock(
		LockType lockType, int waitingTimeoutInSecondsIfLocked, std::string owner, std::string label, int milliSecondsToSleepWaitingLock = 500,
		std::string data = "no data"
	);

	void releaseLock(LockType lockType, std::string label, std::string data = "no data");

	int64_t addEncoder(
		const std::string &label, bool external, bool enabled, const std::string &protocol, const std::string &publicServerName,
		const std::string &internalServerName, int port
	);

	void modifyEncoder(
		int64_t encoderKey, bool labelToBeModified, const std::string &label, bool externalToBeModified, bool external, bool enabledToBeModified,
		bool enabled, bool protocolToBeModified, const std::string &protocol, bool publicServerNameToBeModified, const std::string &publicServerName,
		bool internalServerNameToBeModified, const std::string &internalServerName, bool portToBeModified, int port
		// bool maxTranscodingCapabilityToBeModified, int
		// maxTranscodingCapability, bool
		// maxLiveProxiesCapabilitiesToBeModified, int
		// maxLiveProxiesCapabilities, bool
		// maxLiveRecordingCapabilitiesToBeModified, int
		// maxLiveRecordingCapabilities
	);

	void removeEncoder(int64_t encoderKey);

	// std::tuple<std::string, std::string, std::string> getEncoderDetails(int64_t encoderKey);
	std::tuple<std::string, std::string, std::string>
	encoder_LabelPublicServerNameInternalServerName(int64_t encoderKey, bool fromMaster = false, std::chrono::milliseconds *sqlDuration = nullptr);
	std::string encoder_columnAsString(std::string columnName, int64_t encoderKey, bool fromMaster = false);
	std::shared_ptr<PostgresHelper::SqlResultSet> encoderQuery(
		std::vector<std::string> &requestedColumns, int64_t encoderKey, bool fromMaster, int startIndex = -1, int rows = -1, std::string orderBy = "",
		bool notFoundAsException = true, std::chrono::milliseconds *sqlDuration = nullptr
	);

	bool isEncoderRunning(
		bool external, const std::string &protocol, const std::string &publicServerName, const std::string &internalServerName, int port
	) const;

	std::tuple<bool, int32_t, int64_t> getEncoderInfo(
		bool external, const std::string &protocol, const std::string &publicServerName, const std::string &internalServerName, int port,
		std::chrono::milliseconds *duration = nullptr
	) const;

	void addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey);
	void addAssociationWorkspaceEncoder(int64_t workspaceKey, std::string sharedEncodersPoolLabel, nlohmann::json sharedEncodersLabel);
	void removeAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey);
	bool encoderWorkspaceMapping_isPresent(int64_t workspaceKey, int64_t encoderKey);

	nlohmann::json getEncoderWorkspacesAssociation(int64_t encoderKey, std::chrono::milliseconds *sqlDuration = nullptr);

	nlohmann::json getEncoderList(
		bool admin, int start, int rows, bool allEncoders, int64_t workspaceKey, bool runningInfo, int64_t encoderKey, std::string label,
		std::string serverName, int port,
		std::string labelOrder // "" or "asc" or "desc"
	);

	std::string getEncodersPoolDetails(int64_t encodersPoolKey, std::chrono::milliseconds *sqlDuration = nullptr);

	nlohmann::json getEncodersPoolList(
		int start, int rows, int64_t workspaceKey, int64_t encodersPoolKey, std::string label,
		std::string labelOrder // "" or "asc" or "desc"
	);

	std::tuple<int64_t, bool, std::string, std::string, std::string, int>
	getRunningEncoderByEncodersPool(int64_t workspaceKey, std::string encodersPoolLabel, int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed);

	int getEncodersNumberByEncodersPool(int64_t workspaceKey, std::string encodersPoolLabel);

	std::pair<std::string, bool> getEncoderURL(int64_t encoderKey, std::string serverName = "");

	int64_t addEncodersPool(int64_t workspaceKey, const std::string &label, std::vector<int64_t> &encoderKeys);

	int64_t modifyEncodersPool(int64_t encodersPoolKey, int64_t workspaceKey, std::string newLabel, std::vector<int64_t> &newEncoderKeys);

	void removeEncodersPool(int64_t encodersPoolKey);

	void addUpdatePartitionInfo(int partitionKey, std::string partitionName, uint64_t currentFreeSizeInBytes, int64_t freeSpaceToLeaveInMB);

#ifdef __POSTGRES__
	std::pair<int, uint64_t> getPartitionToBeUsedAndUpdateFreeSpace(int64_t ingestionJobKey, uint64_t ullFSEntrySizeInBytes);
#else
	std::pair<int, uint64_t> getPartitionToBeUsedAndUpdateFreeSpace(uint64_t ullFSEntrySizeInBytes);
#endif

	fs::path getPartitionPathName(int partitionKey);

	uint64_t updatePartitionBecauseOfDeletion(int partitionKey, uint64_t ullFSEntrySizeInBytes);

	void getPartitionsInfo(std::vector<std::pair<int, uint64_t>> &partitionsInfo);

	static int64_t parseRetention(std::string retention);

	nlohmann::json getStreamInputRoot(
		const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, const std::string &configurationLabel,
		const std::string &useVideoTrackFromPhysicalPathName, const std::string &useVideoTrackFromPhysicalDeliveryURL, int maxWidth, const std::string &userAgent,
		const std::string &otherInputOptions, const std::string &taskEncodersPoolLabel, const nlohmann::json &filtersRoot
	);
	std::pair<int64_t, std::string> getStreamInputPushDetails(int64_t workspaceKey, int64_t ingestionJobKey, std::string configurationLabel);
	std::string getStreamPushServerUrl(
		int64_t workspaceKey, int64_t ingestionJobKey, std::string streamConfigurationLabel, int64_t pushEncoderKey, bool pushPublicEncoderName,
		bool pushUriToBeAdded
	);

	nlohmann::json getVodInputRoot(
		MMSEngineDBFacade::ContentType vodContentType, std::vector<std::tuple<int64_t, std::string, std::string, std::string>> &sources, nlohmann::json filtersRoot,
		std::string otherInputOptions
	);

	nlohmann::json getCountdownInputRoot(
		std::string mmsSourceVideoAssetPathName, std::string mmsSourceVideoAssetDeliveryURL, int64_t physicalPathKey, int64_t videoDurationInMilliSeconds,
		nlohmann::json filtersRoot
	);

	nlohmann::json getDirectURLInputRoot(std::string url, nlohmann::json filtersRoot);

	std::string getStreamingYouTubeLiveURL(std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t confKey, std::string liveURL);

	bool onceExecution(OnceType onceType);

	static DeliveryTechnology fileFormatToDeliveryTechnology(std::string fileFormat);

	std::shared_ptr<DBConnectionPool<PostgresConnection>> masterPostgresConnectionPool() { return _masterPostgresConnectionPool; }

  private:
	std::shared_ptr<spdlog::logger> _logger;
	nlohmann::json _configuration;
	PostgresHelper _postgresHelper;
	int _maxRows;

	long _defaultMaxQueryElapsed;
	std::map<std::string, long> _maxQueryElapsed;
	void loadMaxQueryElapsedConfiguration(nlohmann::json slowQueryConfigurationRoot);
	long maxQueryElapsed(const std::string queryLabel);

#ifdef __POSTGRES__
#else
	std::shared_ptr<MySQLConnectionFactory> _mySQLMasterConnectionFactory;
	std::shared_ptr<DBConnectionPool<MySQLConnection>> _masterConnectionPool;
	std::shared_ptr<MySQLConnectionFactory> _mySQLSlaveConnectionFactory;
	std::shared_ptr<DBConnectionPool<MySQLConnection>> _slaveConnectionPool;
#endif

	std::shared_ptr<PostgresConnectionFactory> _postgresMasterConnectionFactory;
	std::shared_ptr<DBConnectionPool<PostgresConnection>> _masterPostgresConnectionPool;
	std::shared_ptr<PostgresConnectionFactory> _postgresSlaveConnectionFactory;
	std::shared_ptr<DBConnectionPool<PostgresConnection>> _slavePostgresConnectionPool;

#ifdef __POSTGRES__
#else
	std::string _defaultContentProviderName;
#endif
	// std::string                          _defaultTerritoryName;
	int _ingestionJobsSelectPageSize;
	int _maxEncodingFailures;
	int _confirmationCodeExpirationInDays;
	int _contentRetentionInMinutesDefaultValue;
	// int _addContentIngestionJobsNotCompletedRetentionInDays;

	int _maxSecondsToWaitUpdateIngestionJobLock;
	int _maxSecondsToWaitUpdateEncodingJobLock;
	int _maxSecondsToWaitCheckIngestionLock;
	int _maxSecondsToWaitCheckEncodingJobLock;
	int _maxSecondsToWaitMainAndBackupLiveChunkLock;
	int _maxSecondsToWaitSetNotToBeExecutedLock;

	int _doNotManageIngestionsOlderThanDays;
	int _ingestionWorkflowCompletedRetentionInDays;
	int _statisticRetentionInMonths;

	std::string _ffmpegEncoderUser;
	std::string _ffmpegEncoderPassword;
	std::string _ffmpegEncoderStatusURI;
	std::string _ffmpegEncoderInfoURI;
	int _ffmpegEncoderInfoTimeout;

	std::chrono::system_clock::time_point _lastConnectionStatsReport;
	int _dbConnectionPoolStatsReportPeriodInSeconds;

	std::string _predefinedWorkflowLibraryDirectoryPath;

	std::string _predefinedVideoProfilesDirectoryPath;
	std::string _predefinedAudioProfilesDirectoryPath;
	std::string _predefinedImageProfilesDirectoryPath;

	std::vector<std::string> _adminEmailAddresses;

	int _getIngestionJobsCurrentIndex;
	int _getEncodingJobsCurrentIndex;

	bool _statisticsEnabled;

	bool _geoServiceEnabled;
	int _geoServiceMaxDaysBeforeUpdate;
	std::string _geoServiceURL;
	std::string _geoServiceKey;
	int _geoServiceTimeoutInSeconds;

#ifdef __POSTGRES__
	void loadSqlColumnsSchema();
	static std::string getPostgresArray(const std::vector<std::string> &arrayElements, bool emptyElementToBeRemoved, const PostgresConnTrans &trans);
	static std::string getPostgresArray(const nlohmann::json &arrayRoot, bool emptyElementToBeRemoved, const PostgresConnTrans &trans);
	bool isTimezoneValid(std::string timezone);
#endif

#ifdef __POSTGRES__
	nlohmann::json getStreamList(
		PostgresConnTrans &trans, int64_t workspaceKey, int64_t confKey, int start, int rows, std::string label, bool labelLike, std::string url,
		std::string sourceType, std::string type, std::string name, std::string region, std::string country, std::string labelOrder, bool fromMaster = false
	);
#else
	nlohmann::json getStreamList(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t liveURLKey, int start, int rows, std::string label, bool labelLike, std::string url,
		std::string sourceType, std::string type, std::string name, std::string region, std::string country, std::string labelOrder
	);
#endif

#ifdef __POSTGRES__
	std::string createAPIKeyForActiveDirectoryUser(
		PostgresConnTrans &trans, int64_t userKey, const std::string& userEmailAddress, bool createRemoveWorkspace, bool ingestWorkflow,
		bool createProfiles,
		bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob,
		bool editEncodersPool, bool applicationRecorder, bool createRemoveLiveChannel, bool updateEncoderStats, int64_t workspaceKey,
		int expirationInDaysWorkspaceDefaultValue
	);
#else
	std::string createAPIKeyForActiveDirectoryUser(
		std::shared_ptr<MySQLConnection> conn, int64_t userKey, std::string userEmailAddress, bool createRemoveWorkspace, bool ingestWorkflow,
		bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding,
		bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder, int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
	);
#endif

#ifdef __POSTGRES__
	void addWorkspaceForAdminUsers(PostgresConnTrans &trans, int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue);
#else
	void addWorkspaceForAdminUsers(std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue);
#endif

#ifdef __POSTGRES__
	std::string createCode(
		PostgresConnTrans &trans, int64_t workspaceKey, int64_t userKey, const std::string &userEmail, CodeType codeType, bool admin,
		bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder,
		bool createRemoveLiveChannel, bool updateEncoderStats
	);
#else
	std::string createCode(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t userKey, std::string userEmail, CodeType codeType, bool admin,
		bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia,
		bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool, bool applicationRecorder
	);
#endif

#ifdef __POSTGRES__
	std::tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> isIngestionJobToBeManaged(
		int64_t ingestionJobKey, int64_t workspaceKey, IngestionStatus ingestionStatus, IngestionType ingestionType, PostgresConnTrans &trans,
		std::chrono::milliseconds *sqlDuration = nullptr
	);
#else
	std::tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> isIngestionJobToBeManaged(
		int64_t ingestionJobKey, int64_t workspaceKey, IngestionStatus ingestionStatus, IngestionType ingestionType, std::shared_ptr<MySQLConnection> conn
	);
#endif

#ifdef __POSTGRES__
	void addIngestionJobOutput(
		PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey
	);
#else
	void addIngestionJobOutput(
		std::shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey
	);
#endif

	int getIngestionTypePriority(MMSEngineDBFacade::IngestionType);

	int getEncodingTypePriority(MMSEngineDBFacade::EncodingType);

#ifdef __POSTGRES__
#else
	std::pair<std::shared_ptr<sql::ResultSet>, int64_t> getMediaItemsList_withoutTagsCheck(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t mediaItemKey, std::vector<int64_t> &otherMediaItemsKey, int start, int rows,
		bool contentTypePresent, ContentType contentType,
		// bool startAndEndIngestionDatePresent,
		std::string startIngestionDate, std::string endIngestionDate, std::string title, int liveRecordingChunk, int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, std::string nlohmann::jsonCondition, std::string orderBy,
		std::string nlohmann::jsonOrderBy, bool admin
	);

	std::pair<std::shared_ptr<sql::ResultSet>, int64_t> getMediaItemsList_withTagsCheck(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, std::string temporaryTableName, int64_t mediaItemKey, std::vector<int64_t> &otherMediaItemsKey,
		int start, int rows, bool contentTypePresent, ContentType contentType,
		// bool startAndEndIngestionDatePresent,
		std::string startIngestionDate, std::string endIngestionDate, std::string title, int liveRecordingChunk, int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, std::string nlohmann::jsonCondition,
		std::vector<std::string> &tagsIn, std::vector<std::string> &tagsNotIn, std::string orderBy, std::string nlohmann::jsonOrderBy, bool admin
	);
#endif

#ifdef __POSTGRES__
	void updateIngestionJob(
		PostgresConnTrans &trans, int64_t ingestionJobKey, IngestionStatus newIngestionStatus, std::string errorMessage,
		std::string processorMMS = "noToBeUpdated"
	);
#else
	void updateIngestionJob(
		std::shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, IngestionStatus newIngestionStatus, std::string errorMessage,
		std::string processorMMS = "noToBeUpdated"
	);
#endif

#ifdef __POSTGRES__
	std::pair<int64_t, std::string> addWorkspace(
		PostgresConnTrans &trans, int64_t userKey, bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles,
		bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob,
		bool editEncodersPool, bool applicationRecorder, bool createRemoveLiveChannel, bool updateEncoderStats,
		const std::string &workspaceName, const std::string &notes,
		WorkspaceType workspaceType, const std::string &deliveryURL, EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod,
		long maxIngestionsNumber, long maxStorageInMB, const std::string &languageCode, std::string workspaceTimezone,
		std::chrono::system_clock::time_point userExpirationLocalDate
	);
#else
	std::pair<int64_t, std::string> addWorkspace(
		std::shared_ptr<MySQLConnection> conn, int64_t userKey, bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles,
		bool deliveryAuthorization, bool shareWorkspace, bool editMedia, bool editConfiguration, bool killEncoding, bool cancelIngestionJob,
		bool editEncodersPool, bool applicationRecorder, std::string workspaceName, WorkspaceType workspaceType, std::string deliveryURL,
		EncodingPriority maxEncodingPriority, EncodingPeriod encodingPeriod, long maxIngestionsNumber, long maxStorageInMB, std::string languageCode,
		std::chrono::system_clock::time_point userExpirationDate
	);
#endif

#ifdef __POSTGRES__
	void
	manageExternalUniqueName(PostgresConnTrans &trans, int64_t workspaceKey, int64_t mediaItemKey, bool allowUniqueNameOverride, std::string uniqueName);
#else
	void manageExternalUniqueName(
		std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey, int64_t mediaItemKey, bool allowUniqueNameOverride, std::string uniqueName
	);
#endif

#ifdef __POSTGRES__
	int64_t saveVariantContentMetadata(
		PostgresConnTrans &trans,

		int64_t workspaceKey, int64_t ingestionJobKey, int64_t liveRecordingIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
		std::string externalDeliveryTechnology, std::string externalDeliveryURL, std::string encodedFileName, std::string relativePath, int mmsPartitionIndexUsed,
		unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

		// video-audio
		std::tuple<int64_t, long, nlohmann::json> &mediaInfoDetails, std::vector<std::tuple<int, int64_t, std::string, std::string, int, int, std::string, long>> &videoTracks,
		std::vector<std::tuple<int, int64_t, std::string, long, int, long, std::string>> &audioTracks,

		// image
		int imageWidth, int imageHeight, std::string imageFormat, int imageQuality
	);
#else
	int64_t saveVariantContentMetadata(
		std::shared_ptr<MySQLConnection> conn,

		int64_t workspaceKey, int64_t ingestionJobKey, int64_t liveRecordingIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
		std::string externalDeliveryTechnology, std::string externalDeliveryURL, std::string encodedFileName, std::string relativePath, int mmsPartitionIndexUsed,
		unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

		// video-audio
		std::tuple<int64_t, long, nlohmann::json> &mediaInfoDetails, std::vector<std::tuple<int, int64_t, std::string, std::string, int, int, std::string, long>> &videoTracks,
		std::vector<std::tuple<int, int64_t, std::string, long, int, long, std::string>> &audioTracks,

		// image
		int imageWidth, int imageHeight, std::string imageFormat, int imageQuality
	);
#endif

#ifdef __POSTGRES__
	int64_t addUpdateWorkflowAsLibrary(
		PostgresConnTrans &trans, int64_t userKey, int64_t workspaceKey, std::string label, int64_t thumbnailMediaItemKey,
		const std::string_view& jsonWorkflow, bool admin
	);
#else
	int64_t addUpdateWorkflowAsLibrary(
		std::shared_ptr<MySQLConnection> conn, int64_t userKey, int64_t workspaceKey, std::string label, int64_t thumbnailMediaItemKey, std::string nlohmann::jsonWorkflow,
		bool admin
	);
#endif

#ifdef __POSTGRES__
	void
	manageCrossReferences(PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t workspaceKey, int64_t mediaItemKey, nlohmann::json crossReferencesRoot);
#endif

#ifdef __POSTGRES__
	void addCrossReference(
		PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType,
		int64_t targetMediaItemKey, nlohmann::json crossReferenceParametersRoot
	);
#else
	void addCrossReference(
		std::shared_ptr<MySQLConnection> conn, int64_t ingestionJobKey, int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType,
		int64_t targetMediaItemKey, nlohmann::json crossReferenceParametersRoot
	);
#endif

#ifdef __POSTGRES__
	nlohmann::json getIngestionJobRoot(
		const std::shared_ptr<Workspace>& workspace, pqxx::row &row,
		bool dependencyInfo,	  // added for performance issue
		bool ingestionJobOutputs, // added because output could be thousands of entries
		PostgresConnTrans &trans, std::chrono::milliseconds *sqlDuration = nullptr
	);
#else
	nlohmann::json getIngestionJobRoot(
		std::shared_ptr<Workspace> workspace, std::shared_ptr<sql::ResultSet> resultSet, bool dependencyInfo, bool ingestionJobOutputs,
		std::shared_ptr<MySQLConnection> conn
	);
#endif

#ifdef __POSTGRES__
	void manageIngestionJobStatusUpdate(
		int64_t ingestionJobKey, IngestionStatus newIngestionStatus, bool updateIngestionRootStatus, PostgresConnTrans &trans
	);
#else
	void manageIngestionJobStatusUpdate(
		int64_t ingestionJobKey, IngestionStatus newIngestionStatus, bool updateIngestionRootStatus, std::shared_ptr<MySQLConnection> conn
	);
#endif

#ifdef __POSTGRES__
	std::pair<int64_t, int64_t> getWorkspaceUsage(const PostgresConnTrans &trans, int64_t workspaceKey);
#else
	std::pair<int64_t, int64_t> getWorkspaceUsage(std::shared_ptr<MySQLConnection> conn, int64_t workspaceKey);
#endif

#ifdef __POSTGRES__
	nlohmann::json getWorkspaceDetailsRoot(PostgresConnTrans &trans, pqxx::row &row, bool userAPIKeyInfo, bool costDetails);
#else
	nlohmann::json getWorkspaceDetailsRoot(std::shared_ptr<MySQLConnection> conn, std::shared_ptr<sql::ResultSet> resultSet, bool userAPIKeyInfo);
#endif

#ifdef __POSTGRES__
	void addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey, PostgresConnTrans &trans);
#else
	void addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey, std::shared_ptr<MySQLConnection> conn);
#endif

#ifdef __POSTGRES__
	nlohmann::json getEncoderRoot(bool admin, bool runningInfo, pqxx::row &row, std::chrono::milliseconds *extraDuration = nullptr);
#else
	nlohmann::json getEncoderRoot(bool admin, bool runningInfo, std::shared_ptr<sql::ResultSet> resultSet);
#endif

	void createTablesIfNeeded();

	bool isRealDBError(std::string exceptionMessage);

#ifdef __POSTGRES__
#else
	bool isJsonTypeSupported(std::shared_ptr<sql::Statement> statement);
#endif

#ifdef __POSTGRES__
#else
	void addTags(std::shared_ptr<MySQLConnection> conn, int64_t mediaItemKey, nlohmann::json tagsRoot);
#endif

#ifdef __POSTGRES__
#else
	void removeTags(std::shared_ptr<MySQLConnection> conn, int64_t mediaItemKey);
#endif

#ifdef __POSTGRES__
#else
	int64_t getLastInsertId(std::shared_ptr<MySQLConnection> conn);
#endif

	/*
	int64_t addTerritory (
	std::shared_ptr<MySQLConnection> conn,
		int64_t workspaceKey,
		std::string territoryName
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

	std::pair<long, std::string> getLastYouTubeURLDetails(std::shared_ptr<Workspace> workspace, int64_t ingestionKey, int64_t confKey);

	void
	updateChannelDataWithNewYouTubeURL(std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t confKey, std::string streamingYouTubeLiveURL);
};
