/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FFMPEGEncoder.cpp
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */
#include "FFMpeg.h"
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include "catralibraries/ProcessUtility.h"
#include "spdlog/spdlog.h"

void FFMpeg::liveRecorder(
	int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder, string segmentListPathName, string recordedFileNamePrefix,

	string otherInputOptions,

	// if streamSourceType is IP_PUSH means the liveURL should be like
	//		rtmp://<local transcoder IP to bind>:<port>
	//		listening for an incoming connection
	// else if streamSourceType is CaptureLive, liveURL is not used
	// else means the liveURL is "any thing" referring a stream
	string streamSourceType, // IP_PULL, TV, IP_PUSH, CaptureLive
	string liveURL,
	// Used only in case streamSourceType is IP_PUSH, Maximum time to wait for the incoming connection
	int pushListenTimeout,

	// parameters used only in case streamSourceType is CaptureLive
	int captureLive_videoDeviceNumber, string captureLive_videoInputFormat, int captureLive_frameRate, int captureLive_width, int captureLive_height,
	int captureLive_audioDeviceNumber, int captureLive_channelsNumber,

	string userAgent, time_t utcRecordingPeriodStart, time_t utcRecordingPeriodEnd,

	int segmentDurationInSeconds, string outputFileFormat,
	string segmenterType, // streamSegmenter or hlsSegmenter

	json outputsRoot,

	json framesToBeDetectedRoot,

	pid_t *pChildPid, chrono::system_clock::time_point *pRecordingStart, long *numberOfRestartBecauseOfFailure
)
{
	_currentApiName = APIName::LiveRecorder;

	SPDLOG_INFO(
		"Received {}"
		", ingestionJobKey: {}"
		", encodingJobKey: {}",
		toString(_currentApiName), ingestionJobKey, encodingJobKey
	);

	setStatus(
		ingestionJobKey, encodingJobKey
		/*
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	string segmentListPath;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;
	time_t utcNow;

	try
	{
		size_t segmentListPathIndex = segmentListPathName.find_last_of("/");
		if (segmentListPathIndex == string::npos)
		{
			string errorMessage = std::format(
				"No segmentListPath find in the segment path name"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", segmentListPathName: {}",
				ingestionJobKey, encodingJobKey, segmentListPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		segmentListPath = segmentListPathName.substr(0, segmentListPathIndex);

		// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
		// I saw just once that the directory was not created and the liveencoder remains in the loop
		// where:
		//	1. the encoder returns an error becaise of the missing directory
		//	2. EncoderVideoAudioProxy calls again the encoder
		// So, for this reason, the below check is done
		if (!fs::exists(segmentListPath))
		{
			SPDLOG_WARN(
				"segmentListPath does not exist!!! It will be created"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", segmentListPath: {}",
				ingestionJobKey, encodingJobKey, segmentListPath
			);

			SPDLOG_INFO(
				"Create directory"
				", segmentListPath: {}",
				segmentListPath
			);
			fs::create_directories(segmentListPath);
			fs::permissions(
				segmentListPath,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
					fs::perms::others_read | fs::perms::others_exec,
				fs::perm_options::replace
			);
		}

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		// 2021-03-06: I saw even if ffmpeg starts exactly at utcRecordingPeriodStart, the segments start time
		//	is about utcRecordingPeriodStart + 5 seconds.
		//	So, to be sure we have the recording at utcRecordingPeriodStart, we have to start ffmpeg
		//	at lease 5 seconds ahead
		int secondsAheadToStartFfmpeg = 10;
		time_t utcRecordingPeriodStartFixed = utcRecordingPeriodStart - secondsAheadToStartFfmpeg;
		if (utcNow < utcRecordingPeriodStartFixed)
		{
			// 2019-12-19: since the first chunk is removed, we will start a bit early
			// than utcRecordingPeriodStart (50% less than segmentDurationInSeconds)
			long secondsToStartEarly;
			if (segmenterType == "streamSegmenter")
				secondsToStartEarly = segmentDurationInSeconds * 50 / 100;
			else
				secondsToStartEarly = 0;

			while (utcNow + secondsToStartEarly < utcRecordingPeriodStartFixed)
			{
				time_t sleepTime = utcRecordingPeriodStartFixed - (utcNow + secondsToStartEarly);

				SPDLOG_INFO(
					"LiveRecorder timing. Too early to start the LiveRecorder, just sleep {} seconds"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", utcNow: {}"
					", secondsToStartEarly: {}"
					", utcRecordingPeriodStartFixed: {}",
					sleepTime, ingestionJobKey, encodingJobKey, utcNow, secondsToStartEarly, utcRecordingPeriodStartFixed
				);

				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}
			}
		}
		else if (utcRecordingPeriodEnd <= utcNow)
		{
			time_t tooLateTime = utcNow - utcRecordingPeriodEnd;

			string errorMessage = std::format(
				"LiveRecorder timing. Too late to start the LiveRecorder"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", utcNow: {}"
				", utcRecordingPeriodStartFixed: {}"
				", utcRecordingPeriodEnd: {}"
				", tooLateTime: {}",
				ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStartFixed, utcRecordingPeriodEnd, tooLateTime
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		else
		{
			time_t delayTime = utcNow - utcRecordingPeriodStartFixed;

			SPDLOG_WARN(
				"LiveRecorder timing. We are a bit late to start the LiveRecorder, let's start it"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", utcNow: {}"
				", utcRecordingPeriodStartFixed: {}"
				", utcRecordingPeriodEnd: {}"
				", delayTime: {}",
				ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStartFixed, utcRecordingPeriodEnd, delayTime
			);
		}

		{
			// char sUtcTimestamp[64];
			tm tmUtcTimestamp;
			time_t utcTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now());

			localtime_r(&utcTimestamp, &tmUtcTimestamp);
			/*
			sprintf(
				sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d", tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);

			_outputFfmpegPathFileName =
				std::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "liveRecorder", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
				*/
			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "liveRecorder", _currentIngestionJobKey,
				_currentEncodingJobKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);
		}

		string recordedFileNameTemplate = recordedFileNamePrefix;
		if (segmenterType == "streamSegmenter")
			recordedFileNameTemplate += "_%Y-%m-%d_%H-%M-%S_%s."; // viene letto il timestamp dal nome del file
		else													  // if (segmenterType == "hlsSegmenter")
			recordedFileNameTemplate += "_%04d.";				  // non viene letto il timestamp dal nome del file
		recordedFileNameTemplate += outputFileFormat;

		time_t streamingDuration = utcRecordingPeriodEnd - utcNow;

		SPDLOG_INFO(
			"LiveRecording timing. Streaming duration"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", utcNow: {}"
			", utcRecordingPeriodStart: {}"
			", utcRecordingPeriodEnd: {}"
			", streamingDuration: {}",
			ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStart, utcRecordingPeriodEnd, streamingDuration
		);

		int localPushListenTimeout = pushListenTimeout;
		if (streamSourceType == "IP_PUSH" || streamSourceType == "TV")
		{
			if (localPushListenTimeout > 0 && localPushListenTimeout > streamingDuration)
			{
				// 2021-02-02: sceanrio:
				//	streaming duration is 25 seconds
				//	timeout: 3600 seconds
				//	The result is that the process will finish after 3600 seconds, not after 25 seconds
				//	To avoid that, in this scenario, we will set the timeout equals to streamingDuration
				SPDLOG_INFO(
					"LiveRecorder timing. Listen timeout in seconds is reduced because max after 'streamingDuration' the process has to finish"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", utcNow: {}"
					", utcRecordingPeriodStart: {}"
					", utcRecordingPeriodEnd: {}"
					", streamingDuration: {}"
					", localPushListenTimeout: {}",
					ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStart, utcRecordingPeriodEnd, streamingDuration, localPushListenTimeout
				);

				localPushListenTimeout = streamingDuration;
			}
		}

		// user agent is an HTTP header and can be used only in case of http request
		bool userAgentToBeUsed = false;
		if (streamSourceType == "IP_PULL" && userAgent != "")
		{
			string httpPrefix = "http"; // it includes also https
			if (liveURL.size() >= httpPrefix.size() && liveURL.compare(0, httpPrefix.size(), httpPrefix) == 0)
			{
				userAgentToBeUsed = true;
			}
			else
			{
				SPDLOG_WARN(
					"user agent cannot be used if not http"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", liveURL: {}",
					ingestionJobKey, encodingJobKey, liveURL
				);
			}
		}

		ffmpegArgumentList.push_back("ffmpeg");
		// FFMpegEncodingParameters::addToArguments("-loglevel repeat+level+trace", ffmpegArgumentList);
		if (userAgentToBeUsed)
		{
			ffmpegArgumentList.push_back("-user_agent");
			ffmpegArgumentList.push_back(userAgent);
		}

		if (otherInputOptions != "")
		{
			FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegArgumentList);
		}

		if (framesToBeDetectedRoot != nullptr && framesToBeDetectedRoot.size() > 0)
		{
			// 2022-05-28; in caso di framedetection, we will "fix" PTS
			//	otherwise the one logged seems are not correct.
			//	Using +genpts are OK
			ffmpegArgumentList.push_back("-fflags");
			ffmpegArgumentList.push_back("+genpts");
		}

		if (streamSourceType == "IP_PUSH")
		{
			// listen/timeout depend on the protocol (https://ffmpeg.org/ffmpeg-protocols.html)
			if (liveURL.find("http://") != string::npos || liveURL.find("rtmp://") != string::npos)
			{
				ffmpegArgumentList.push_back("-listen");
				ffmpegArgumentList.push_back("1");
				if (localPushListenTimeout > 0)
				{
					// no timeout means it will listen infinitely
					ffmpegArgumentList.push_back("-timeout");
					ffmpegArgumentList.push_back(to_string(localPushListenTimeout));
				}
			}
			else if (liveURL.find("udp://") != string::npos)
			{
				if (localPushListenTimeout > 0)
				{
					int64_t listenTimeoutInMicroSeconds = localPushListenTimeout;
					listenTimeoutInMicroSeconds *= 1000000;
					liveURL += "?timeout=" + to_string(listenTimeoutInMicroSeconds);
				}
			}
			else
			{
				SPDLOG_ERROR(
					"listen/timeout not managed yet for the current protocol"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", liveURL: {}",
					ingestionJobKey, encodingJobKey, liveURL
				);
			}

			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(liveURL);
		}
		else if (streamSourceType == "IP_PULL" || streamSourceType == "TV")
		{
			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(liveURL);
		}
		else if (streamSourceType == "CaptureLive")
		{
			// video
			{
				// -f v4l2 -framerate 25 -video_size 640x480 -i /dev/video0
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("v4l2");

				ffmpegArgumentList.push_back("-thread_queue_size");
				ffmpegArgumentList.push_back("4096");

				if (captureLive_videoInputFormat != "")
				{
					ffmpegArgumentList.push_back("-input_format");
					ffmpegArgumentList.push_back(captureLive_videoInputFormat);
				}

				if (captureLive_frameRate != -1)
				{
					ffmpegArgumentList.push_back("-framerate");
					ffmpegArgumentList.push_back(to_string(captureLive_frameRate));
				}

				if (captureLive_width != -1 && captureLive_height != -1)
				{
					ffmpegArgumentList.push_back("-video_size");
					ffmpegArgumentList.push_back(to_string(captureLive_width) + "x" + to_string(captureLive_height));
				}

				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(string("/dev/video") + to_string(captureLive_videoDeviceNumber));
			}

			// audio
			{
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("alsa");

				ffmpegArgumentList.push_back("-thread_queue_size");
				ffmpegArgumentList.push_back("2048");

				if (captureLive_channelsNumber != -1)
				{
					ffmpegArgumentList.push_back("-ac");
					ffmpegArgumentList.push_back(to_string(captureLive_channelsNumber));
				}

				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(string("hw:") + to_string(captureLive_audioDeviceNumber));
			}
		}

		// to detect a frame we have to:
		// 1. add -r 1 -loop 1 -i <picture path name of the frame to be detected>
		// 2. add: -filter_complex "[0:v][1:v]blend=difference:shortest=1,blackframe=99:32[f]" -map "[f]" -f null -
		if (framesToBeDetectedRoot != nullptr && framesToBeDetectedRoot.size() > 0)
		{
			for (int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size(); pictureIndex++)
			{
				json frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

				if (JSONUtils::isMetadataPresent(frameToBeDetectedRoot, "picturePathName"))
				{
					string picturePathName = JSONUtils::asString(frameToBeDetectedRoot, "picturePathName", "");

					ffmpegArgumentList.push_back("-r");
					ffmpegArgumentList.push_back("1");

					ffmpegArgumentList.push_back("-loop");
					ffmpegArgumentList.push_back("1");

					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(picturePathName);
				}
			}
		}

		int streamingDurationIndex;
		{
			ffmpegArgumentList.push_back("-t");
			streamingDurationIndex = ffmpegArgumentList.size();
			ffmpegArgumentList.push_back(to_string(streamingDuration));
		}

		// this is to get all video tracks
		ffmpegArgumentList.push_back("-map");
		ffmpegArgumentList.push_back("0:v");

		ffmpegArgumentList.push_back("-c:v");
		ffmpegArgumentList.push_back("copy");

		// this is to get all audio tracks
		ffmpegArgumentList.push_back("-map");
		ffmpegArgumentList.push_back("0:a");

		ffmpegArgumentList.push_back("-c:a");
		ffmpegArgumentList.push_back("copy");

		if (segmenterType == "streamSegmenter")
		{
			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("segment");
			ffmpegArgumentList.push_back("-segment_list");
			ffmpegArgumentList.push_back(segmentListPathName);
			ffmpegArgumentList.push_back("-segment_time");
			ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
			ffmpegArgumentList.push_back("-segment_atclocktime");
			ffmpegArgumentList.push_back("1");
			ffmpegArgumentList.push_back("-strftime");
			ffmpegArgumentList.push_back("1");
			ffmpegArgumentList.push_back(segmentListPath + "/" + recordedFileNameTemplate);
		}
		else // if (segmenterType == "hlsSegmenter")
		{
			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("append_list");
			ffmpegArgumentList.push_back("-hls_time");
			ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
			ffmpegArgumentList.push_back("-hls_list_size");
			ffmpegArgumentList.push_back("10");

			// Segment files removed from the playlist are deleted after a period of time
			// equal to the duration of the segment plus the duration of the playlist
			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("delete_segments");

			// Set the number of unreferenced segments to keep on disk
			// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
			// to download segments which were recently referenced in the playlist.
			// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
			ffmpegArgumentList.push_back("-hls_delete_threshold");
			ffmpegArgumentList.push_back(to_string(1));

			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("program_date_time");

			ffmpegArgumentList.push_back("-hls_segment_filename");
			ffmpegArgumentList.push_back(segmentListPath + "/" + recordedFileNameTemplate);

			// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
			// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
			// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
			// 2020-07-19: commented, if it is needed just test it
			// ffmpegArgumentList.push_back("-hls_start_number_source");
			// ffmpegArgumentList.push_back("datetime");

			// 2020-07-19: commented, if it is needed just test it
			// ffmpegArgumentList.push_back("-start_number");
			// ffmpegArgumentList.push_back(to_string(10));

			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("hls");
			ffmpegArgumentList.push_back(segmentListPathName);
		}

		for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
		{
			json outputRoot = outputsRoot[outputIndex];

			string outputType = JSONUtils::asString(outputRoot, "outputType", "");

			json filtersRoot = nullptr;
			if (JSONUtils::isMetadataPresent(outputRoot, "filters"))
				filtersRoot = outputRoot["filters"];

			json encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];

			string encodingProfileContentType = JSONUtils::asString(outputRoot, "encodingProfileContentType", "Video");
			bool isVideo = encodingProfileContentType == "Video" ? true : false;
			string otherOutputOptions = JSONUtils::asString(outputRoot, "otherOutputOptions", "");

			int videoTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, "videoTrackIndexToBeUsed", -1);
			int audioTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, "audioTrackIndexToBeUsed", -1);

			FFMpegFilters ffmpegFilters(_ffmpegTtfFontDir);

			// if (outputType == "HLS" || outputType == "DASH")
			if (outputType == "HLS_Channel")
			{
				// this is to get all video tracks
				ffmpegArgumentList.push_back("-map");
				if (videoTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:v");
				else
					ffmpegArgumentList.push_back(string("0:v:") + to_string(videoTrackIndexToBeUsed));

				// this is to get all audio tracks
				ffmpegArgumentList.push_back("-map");
				if (audioTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:a");
				else
					ffmpegArgumentList.push_back(string("0:a:") + to_string(videoTrackIndexToBeUsed));

				string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
				string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");
				int playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);
				int localSegmentDurationInSeconds = JSONUtils::asInt(outputRoot, "segmentDurationInSeconds", 10);

				// filter to be managed with the others
				string ffmpegVideoResolutionParameter;

				vector<string> ffmpegEncodingProfileArgumentList;
				if (encodingProfileDetailsRoot != nullptr)
				{
					try
					{
						string httpStreamingFileFormat;
						string ffmpegHttpStreamingParameter = "";

						string ffmpegFileFormatParameter = "";

						string ffmpegVideoCodecParameter = "";
						string ffmpegVideoProfileParameter = "";
						int videoBitRateInKbps = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoOtherParameters = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegVideoFrameRateParameter = "";
						string ffmpegVideoKeyFramesRateParameter = "";
						bool twoPasses;
						vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

						string ffmpegAudioCodecParameter = "";
						string ffmpegAudioBitRateParameter = "";
						string ffmpegAudioOtherParameters = "";
						string ffmpegAudioChannelsParameter = "";
						string ffmpegAudioSampleRateParameter = "";
						vector<string> audioBitRatesInfo;

						FFMpegEncodingParameters::settingFfmpegParameters(
							encodingProfileDetailsRoot, isVideo,

							httpStreamingFileFormat, ffmpegHttpStreamingParameter,

							ffmpegFileFormatParameter,

							ffmpegVideoCodecParameter, ffmpegVideoProfileParameter, ffmpegVideoOtherParameters, twoPasses,
							ffmpegVideoFrameRateParameter, ffmpegVideoKeyFramesRateParameter, videoBitRatesInfo,

							ffmpegAudioCodecParameter, ffmpegAudioOtherParameters, ffmpegAudioChannelsParameter, ffmpegAudioSampleRateParameter,
							audioBitRatesInfo
						);

						tuple<string, int, int, int, string, string, string> videoBitRateInfo = videoBitRatesInfo[0];
						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore, ffmpegVideoBitRateParameter,
							ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

						if (twoPasses)
						{
							twoPasses = false;

							SPDLOG_WARN(
								"in case of proxy it is not possible to have a two passes encoding. Change it to false"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", twoPasses: {}",
								ingestionJobKey, encodingJobKey, twoPasses
							);
						}

						FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegEncodingProfileArgumentList);
						// FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 	ffmpegEncodingProfileArgumentList);
						ffmpegEncodingProfileArgumentList.push_back("-threads");
						ffmpegEncodingProfileArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
					}
					catch (runtime_error &e)
					{
						string errorMessage = std::format(
							"encodingProfileParameter retrieving failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
						SPDLOG_ERROR(errorMessage);

						throw e;
					}
				}

				tuple<string, string, string> allFilters = ffmpegFilters.addFilters(filtersRoot, ffmpegVideoResolutionParameter, "", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;

				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter : ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}
				else
				{
					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					else if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}

					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					else if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}

					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}

				{
					string manifestFilePathName = manifestDirectoryPath + "/" + manifestFileName;

					SPDLOG_INFO(
						"Checking manifestDirectoryPath directory"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", manifestDirectoryPath: {}",
						ingestionJobKey, encodingJobKey, manifestDirectoryPath
					);

					// directory is created by EncoderVideoAudioProxy
					//	using MMSStorage::getStagingAssetPathName
					// I saw just once that the directory was not created and
					//	the liveencoder remains in the loop
					// where:
					//	1. the encoder returns an error because of the missing directory
					//	2. EncoderVideoAudioProxy calls again the encoder
					// So, for this reason, the below check is done
					if (!fs::exists(manifestDirectoryPath))
					{
						SPDLOG_INFO(
							"Create directory"
							", manifestDirectoryPath: {}",
							manifestDirectoryPath
						);
						fs::create_directories(manifestDirectoryPath);
						fs::permissions(
							manifestDirectoryPath,
							fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
								fs::perms::others_read | fs::perms::others_exec,
							fs::perm_options::replace
						);
					}

					if (externalEncoder)
						addToIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

					// if (outputType == "HLS")
					{
						ffmpegArgumentList.push_back("-hls_flags");
						ffmpegArgumentList.push_back("append_list");
						ffmpegArgumentList.push_back("-hls_time");
						ffmpegArgumentList.push_back(to_string(localSegmentDurationInSeconds));
						ffmpegArgumentList.push_back("-hls_list_size");
						ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

						// Segment files removed from the playlist are deleted after a period of time
						// equal to the duration of the segment plus the duration of the playlist
						ffmpegArgumentList.push_back("-hls_flags");
						ffmpegArgumentList.push_back("delete_segments");

						// Set the number of unreferenced segments to keep on disk
						// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
						// to download segments which were recently referenced in the playlist.
						// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
						ffmpegArgumentList.push_back("-hls_delete_threshold");
						ffmpegArgumentList.push_back(to_string(1));

						// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
						// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
						// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
						// 2020-07-19: commented, if it is needed just test it
						// ffmpegArgumentList.push_back("-hls_start_number_source");
						// ffmpegArgumentList.push_back("datetime");

						// 2020-07-19: commented, if it is needed just test it
						// ffmpegArgumentList.push_back("-start_number");
						// ffmpegArgumentList.push_back(to_string(10));
					}
					/*
					else if (outputType == "DASH")
					{
						ffmpegArgumentList.push_back("-seg_duration");
						ffmpegArgumentList.push_back(to_string(localSegmentDurationInSeconds));
						ffmpegArgumentList.push_back("-window_size");
						ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

						// it is important to specify -init_seg_name because those files
						// will not be removed in EncoderVideoAudioProxy.cpp
						ffmpegArgumentList.push_back("-init_seg_name");
						ffmpegArgumentList.push_back("init-stream$RepresentationID$.$ext$");

						// the only difference with the ffmpeg default is that default is $Number%05d$
						// We had to change it to $Number%01d$ because otherwise the generated file containing
						// 00001 00002 ... but the videojs player generates file name like 1 2 ...
						// and the streaming was not working
						ffmpegArgumentList.push_back("-media_seg_name");
						ffmpegArgumentList.push_back("chunk-stream$RepresentationID$-$Number%01d$.$ext$");
					}
					*/

					FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegArgumentList);

					ffmpegArgumentList.push_back(manifestFilePathName);
				}
			}
			else if (outputType == "CDN_AWS" || outputType == "CDN_CDN77" || outputType == "RTMP_Channel")
			{
				// 2022-09-01: scenario: mando un m3u8 multi tracce ricevuto da HWM (serie C)
				//	verso un rtmp della CDN77, mi fallisce perchè un flv/rtmp non puo' essere
				//	multi traccia.
				//	Quindi mi assicuro di mandare una sola traccia (la prima)
				ffmpegArgumentList.push_back("-map");
				if (videoTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:v:0");
				else
					ffmpegArgumentList.push_back(string("0:v:") + to_string(videoTrackIndexToBeUsed));
				ffmpegArgumentList.push_back("-map");
				if (audioTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:a:0");
				else
					ffmpegArgumentList.push_back(string("0:a:") + to_string(videoTrackIndexToBeUsed));

				string rtmpUrl = JSONUtils::asString(outputRoot, "rtmpUrl", "");
				if (rtmpUrl == "")
				{
					string errorMessage = std::format(
						"rtmpUrl cannot be empty"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", rtmpUrl: {}",
						ingestionJobKey, encodingJobKey, rtmpUrl
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				// filter to be managed with the others
				string ffmpegVideoResolutionParameter;

				vector<string> ffmpegEncodingProfileArgumentList;
				if (encodingProfileDetailsRoot != nullptr)
				{
					try
					{
						string httpStreamingFileFormat;
						string ffmpegHttpStreamingParameter = "";

						string ffmpegFileFormatParameter = "";

						string ffmpegVideoCodecParameter = "";
						string ffmpegVideoProfileParameter = "";
						// string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoOtherParameters = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegVideoFrameRateParameter = "";
						string ffmpegVideoKeyFramesRateParameter = "";
						bool twoPasses;
						vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

						string ffmpegAudioCodecParameter = "";
						string ffmpegAudioBitRateParameter = "";
						string ffmpegAudioOtherParameters = "";
						string ffmpegAudioChannelsParameter = "";
						string ffmpegAudioSampleRateParameter = "";
						vector<string> audioBitRatesInfo;

						FFMpegEncodingParameters::settingFfmpegParameters(
							encodingProfileDetailsRoot, isVideo,

							httpStreamingFileFormat, ffmpegHttpStreamingParameter,

							ffmpegFileFormatParameter,

							ffmpegVideoCodecParameter, ffmpegVideoProfileParameter, ffmpegVideoOtherParameters, twoPasses,
							ffmpegVideoFrameRateParameter, ffmpegVideoKeyFramesRateParameter, videoBitRatesInfo,

							ffmpegAudioCodecParameter, ffmpegAudioOtherParameters, ffmpegAudioChannelsParameter, ffmpegAudioSampleRateParameter,
							audioBitRatesInfo
						);

						tuple<string, int, int, int, string, string, string> videoBitRateInfo = videoBitRatesInfo[0];
						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore, ffmpegVideoBitRateParameter,
							ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

						/*
						if (httpStreamingFileFormat != "")
						{
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have an httpStreaming encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
						else */
						if (twoPasses)
						{
							/*
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
							*/
							twoPasses = false;

							SPDLOG_WARN(
								"in case of proxy it is not possible to have a two passes encoding. Change it to false"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", twoPasses: {}",
								ingestionJobKey, encodingJobKey, twoPasses
							);
						}

						FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegEncodingProfileArgumentList);
						// FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 	ffmpegEncodingProfileArgumentList);
						ffmpegEncodingProfileArgumentList.push_back("-threads");
						ffmpegEncodingProfileArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
					}
					catch (runtime_error &e)
					{
						string errorMessage = std::format(
							"encodingProfileParameter retrieving failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
						SPDLOG_ERROR(errorMessage);

						throw e;
					}
				}

				tuple<string, string, string> allFilters = ffmpegFilters.addFilters(filtersRoot, ffmpegVideoResolutionParameter, "", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;

				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter : ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}
				else
				{
					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					else if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}

					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					else if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}

					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}

				FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegArgumentList);

				// 2023-01-14
				// the aac_adtstoasc filter is needed only in case of an AAC input, otherwise
				// it will generate an error
				// Questo filtro è sempre stato aggiunto. Ora abbiamo il caso di un codec audio mp2 che
				// genera un errore.
				// Per essere conservativo ed evitare problemi, il controllo sotto viene fatto in 'logica negata'.
				// cioè invece di abilitare il filtro per i codec aac, lo disabilitiamo per il codec mp2
				// 2023-01-15
				// Il problema 'sopra' che, a seguito del codec mp2 veniva generato un errore,
				// è stato risolto aggiungendo un encoding in uscita.
				// Per questo motivo sotto viene commentato
				if (streamSourceType == "IP_PUSH" || streamSourceType == "IP_PULL" || streamSourceType == "TV")
				{
					/*
					vector<tuple<int, int64_t, string, string, int, int, string, long>> inputVideoTracks;
					vector<tuple<int, int64_t, string, long, int, long, string>> inputAudioTracks;

					try
					{
						getMediaInfo(ingestionJobKey, false, liveURL, inputVideoTracks, inputAudioTracks);
					}
					catch(runtime_error& e)
					{
						string errorMessage = __FILEREF__ + "ffmpeg: getMediaInfo failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						SPDLOG_ERROR(errorMessage);

						// throw e;
					}

					bool aacFilterToBeAdded = true;
					for(tuple<int, int64_t, string, long, int, long, string> inputAudioTrack: inputAudioTracks)
					{
						// trackIndex, audioDurationInMilliSeconds, audioCodecName,
						// audioSampleRate, audioChannels, audioBitRate, language));
						string audioCodecName;

						tie(ignore, ignore, audioCodecName, ignore, ignore, ignore, ignore) = inputAudioTrack;

						string audioCodecNameLowerCase;
						audioCodecNameLowerCase.resize(audioCodecName.size());
						transform(audioCodecName.begin(), audioCodecName.end(), audioCodecNameLowerCase.begin(),
							[](unsigned char c){return tolower(c); } );

						if (audioCodecNameLowerCase.find("mp2") != string::npos
						)
							aacFilterToBeAdded = false;

						info(__FILEREF__ + "aac check"
							+ ", audioCodecName: " + audioCodecName
							+ ", aacFilterToBeAdded: " + to_string(aacFilterToBeAdded)
						);
					}

					if (aacFilterToBeAdded)
					*/
					{
						ffmpegArgumentList.push_back("-bsf:a");
						ffmpegArgumentList.push_back("aac_adtstoasc");
					}
				}

				// 2020-08-13: commented bacause -c:v copy is already present
				// ffmpegArgumentList.push_back("-vcodec");
				// ffmpegArgumentList.push_back("copy");

				// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("flv");
				ffmpegArgumentList.push_back(rtmpUrl);
			}
			else if (outputType == "HLS_Channel")
			{
				// this is to get all video tracks
				ffmpegArgumentList.push_back("-map");
				if (videoTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:v");
				else
					ffmpegArgumentList.push_back(string("0:v:") + to_string(videoTrackIndexToBeUsed));

				// this is to get all audio tracks
				ffmpegArgumentList.push_back("-map");
				if (audioTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:a");
				else
					ffmpegArgumentList.push_back(string("0:a:") + to_string(videoTrackIndexToBeUsed));

				string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
				string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");
				int playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);
				int localSegmentDurationInSeconds = JSONUtils::asInt(outputRoot, "segmentDurationInSeconds", 10);

				// filter to be managed with the others
				string ffmpegVideoResolutionParameter;

				vector<string> ffmpegEncodingProfileArgumentList;
				if (encodingProfileDetailsRoot != nullptr)
				{
					try
					{
						string httpStreamingFileFormat;
						string ffmpegHttpStreamingParameter = "";

						string ffmpegFileFormatParameter = "";

						string ffmpegVideoCodecParameter = "";
						string ffmpegVideoProfileParameter = "";
						int videoBitRateInKbps = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoOtherParameters = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegVideoFrameRateParameter = "";
						string ffmpegVideoKeyFramesRateParameter = "";
						bool twoPasses;
						vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

						string ffmpegAudioCodecParameter = "";
						string ffmpegAudioBitRateParameter = "";
						string ffmpegAudioOtherParameters = "";
						string ffmpegAudioChannelsParameter = "";
						string ffmpegAudioSampleRateParameter = "";
						vector<string> audioBitRatesInfo;

						FFMpegEncodingParameters::settingFfmpegParameters(
							encodingProfileDetailsRoot, isVideo,

							httpStreamingFileFormat, ffmpegHttpStreamingParameter,

							ffmpegFileFormatParameter,

							ffmpegVideoCodecParameter, ffmpegVideoProfileParameter, ffmpegVideoOtherParameters, twoPasses,
							ffmpegVideoFrameRateParameter, ffmpegVideoKeyFramesRateParameter, videoBitRatesInfo,

							ffmpegAudioCodecParameter, ffmpegAudioOtherParameters, ffmpegAudioChannelsParameter, ffmpegAudioSampleRateParameter,
							audioBitRatesInfo
						);

						tuple<string, int, int, int, string, string, string> videoBitRateInfo = videoBitRatesInfo[0];
						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore, ffmpegVideoBitRateParameter,
							ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

						if (twoPasses)
						{
							twoPasses = false;

							SPDLOG_WARN(
								"in case of proxy it is not possible to have a two passes encoding. Change it to false"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", twoPasses: {}",
								ingestionJobKey, encodingJobKey, twoPasses
							);
						}

						FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegEncodingProfileArgumentList);
						// FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 	ffmpegEncodingProfileArgumentList);
						ffmpegEncodingProfileArgumentList.push_back("-threads");
						ffmpegEncodingProfileArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
					}
					catch (runtime_error &e)
					{
						string errorMessage = std::format(
							"encodingProfileParameter retrieving failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
						SPDLOG_ERROR(errorMessage);

						throw e;
					}
				}

				tuple<string, string, string> allFilters = ffmpegFilters.addFilters(filtersRoot, ffmpegVideoResolutionParameter, "", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;

				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter : ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}
				else
				{
					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					else if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}

					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					else if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}

					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}

				{
					string manifestFilePathName = manifestDirectoryPath + "/" + manifestFileName;

					SPDLOG_INFO(
						"Checking manifestDirectoryPath directory"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", manifestDirectoryPath: {}",
						ingestionJobKey, encodingJobKey, manifestDirectoryPath
					);

					// directory is created by EncoderVideoAudioProxy
					//	using MMSStorage::getStagingAssetPathName
					// I saw just once that the directory was not created and
					//	the liveencoder remains in the loop
					// where:
					//	1. the encoder returns an error because of the missing directory
					//	2. EncoderVideoAudioProxy calls again the encoder
					// So, for this reason, the below check is done
					if (!fs::exists(manifestDirectoryPath))
					{
						SPDLOG_INFO(
							"Create directory"
							", manifestDirectoryPath: {}",
							manifestDirectoryPath
						);
						fs::create_directories(manifestDirectoryPath);
						fs::permissions(
							manifestDirectoryPath,
							fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
								fs::perms::others_read | fs::perms::others_exec,
							fs::perm_options::replace
						);
					}

					if (externalEncoder)
						addToIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

					// if (outputType == "HLS")
					{
						ffmpegArgumentList.push_back("-hls_flags");
						ffmpegArgumentList.push_back("append_list");
						ffmpegArgumentList.push_back("-hls_time");
						ffmpegArgumentList.push_back(to_string(localSegmentDurationInSeconds));
						ffmpegArgumentList.push_back("-hls_list_size");
						ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

						// Segment files removed from the playlist are deleted after a period of time
						// equal to the duration of the segment plus the duration of the playlist
						ffmpegArgumentList.push_back("-hls_flags");
						ffmpegArgumentList.push_back("delete_segments");

						// Set the number of unreferenced segments to keep on disk
						// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
						// to download segments which were recently referenced in the playlist.
						// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
						ffmpegArgumentList.push_back("-hls_delete_threshold");
						ffmpegArgumentList.push_back(to_string(1));

						// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
						// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
						// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
						// 2020-07-19: commented, if it is needed just test it
						// ffmpegArgumentList.push_back("-hls_start_number_source");
						// ffmpegArgumentList.push_back("datetime");

						// 2020-07-19: commented, if it is needed just test it
						// ffmpegArgumentList.push_back("-start_number");
						// ffmpegArgumentList.push_back(to_string(10));
					}
					/*
					else if (outputType == "DASH")
					{
						ffmpegArgumentList.push_back("-seg_duration");
						ffmpegArgumentList.push_back(to_string(localSegmentDurationInSeconds));
						ffmpegArgumentList.push_back("-window_size");
						ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

						// it is important to specify -init_seg_name because those files
						// will not be removed in EncoderVideoAudioProxy.cpp
						ffmpegArgumentList.push_back("-init_seg_name");
						ffmpegArgumentList.push_back("init-stream$RepresentationID$.$ext$");

						// the only difference with the ffmpeg default is that default is $Number%05d$
						// We had to change it to $Number%01d$ because otherwise the generated file containing
						// 00001 00002 ... but the videojs player generates file name like 1 2 ...
						// and the streaming was not working
						ffmpegArgumentList.push_back("-media_seg_name");
						ffmpegArgumentList.push_back("chunk-stream$RepresentationID$-$Number%01d$.$ext$");
					}
					*/

					FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegArgumentList);

					ffmpegArgumentList.push_back(manifestFilePathName);
				}
			}
			else if (outputType == "UDP_Stream")
			{
				// this is to get all video tracks
				ffmpegArgumentList.push_back("-map");
				if (videoTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:v");
				else
					ffmpegArgumentList.push_back(string("0:v:") + to_string(videoTrackIndexToBeUsed));

				// this is to get all audio tracks
				ffmpegArgumentList.push_back("-map");
				if (audioTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:a");
				else
					ffmpegArgumentList.push_back(string("0:a:") + to_string(videoTrackIndexToBeUsed));

				string udpUrl = JSONUtils::asString(outputRoot, "udpUrl", "");

				if (udpUrl == "")
				{
					string errorMessage = std::format(
						"udpUrl cannot be empty"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", udpUrl: {}",
						ingestionJobKey, encodingJobKey, udpUrl
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				// filter to be managed with the others
				string ffmpegVideoResolutionParameter;

				vector<string> ffmpegEncodingProfileArgumentList;
				if (encodingProfileDetailsRoot != nullptr)
				{
					try
					{
						string httpStreamingFileFormat;
						string ffmpegHttpStreamingParameter = "";

						string ffmpegFileFormatParameter = "";

						string ffmpegVideoCodecParameter = "";
						string ffmpegVideoProfileParameter = "";
						// string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoOtherParameters = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegVideoFrameRateParameter = "";
						string ffmpegVideoKeyFramesRateParameter = "";
						bool twoPasses;
						vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

						string ffmpegAudioCodecParameter = "";
						string ffmpegAudioBitRateParameter = "";
						string ffmpegAudioOtherParameters = "";
						string ffmpegAudioChannelsParameter = "";
						string ffmpegAudioSampleRateParameter = "";
						vector<string> audioBitRatesInfo;

						FFMpegEncodingParameters::settingFfmpegParameters(
							encodingProfileDetailsRoot, isVideo,

							httpStreamingFileFormat, ffmpegHttpStreamingParameter,

							ffmpegFileFormatParameter,

							ffmpegVideoCodecParameter, ffmpegVideoProfileParameter, ffmpegVideoOtherParameters, twoPasses,
							ffmpegVideoFrameRateParameter, ffmpegVideoKeyFramesRateParameter, videoBitRatesInfo,

							ffmpegAudioCodecParameter, ffmpegAudioOtherParameters, ffmpegAudioChannelsParameter, ffmpegAudioSampleRateParameter,
							audioBitRatesInfo
						);

						tuple<string, int, int, int, string, string, string> videoBitRateInfo = videoBitRatesInfo[0];
						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore, ffmpegVideoBitRateParameter,
							ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

						/*
						if (httpStreamingFileFormat != "")
						{
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have an httpStreaming encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
						else */
						if (twoPasses)
						{
							/*
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
							*/
							twoPasses = false;

							SPDLOG_WARN(
								"in case of proxy it is not possible to have a two passes encoding. Change it to false"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", twoPasses: {}",
								ingestionJobKey, encodingJobKey, twoPasses
							);
						}

						FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegEncodingProfileArgumentList);
						// FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 	ffmpegEncodingProfileArgumentList);
						ffmpegEncodingProfileArgumentList.push_back("-threads");
						ffmpegEncodingProfileArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
					}
					catch (runtime_error &e)
					{
						string errorMessage = std::format(
							"encodingProfileParameter retrieving failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
						SPDLOG_ERROR(errorMessage);

						throw e;
					}
				}

				tuple<string, string, string> allFilters = ffmpegFilters.addFilters(filtersRoot, ffmpegVideoResolutionParameter, "", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;

				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter : ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}
				else
				{
					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					else if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}

					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					else if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}

					if (complexFilters != "")
					{
						ffmpegArgumentList.push_back("-filter_complex");
						ffmpegArgumentList.push_back(complexFilters);
					}
				}

				FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegArgumentList);

				// ffmpegArgumentList.push_back("-bsf:a");
				// ffmpegArgumentList.push_back("aac_adtstoasc");
				// 2020-08-13: commented bacause -c:v copy is already present
				// ffmpegArgumentList.push_back("-vcodec");
				// ffmpegArgumentList.push_back("copy");

				// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("mpegts");
				ffmpegArgumentList.push_back(udpUrl);
			}
			else if (outputType == "NONE")
			{
				tuple<string, string, string> allFilters = ffmpegFilters.addFilters(filtersRoot, "", "", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;

				if (videoFilters != "")
				{
					ffmpegArgumentList.push_back("-filter:v");
					ffmpegArgumentList.push_back(videoFilters);
				}

				if (audioFilters != "")
				{
					ffmpegArgumentList.push_back("-filter:a");
					ffmpegArgumentList.push_back(audioFilters);
				}

				if (complexFilters != "")
				{
					ffmpegArgumentList.push_back("-filter_complex");
					ffmpegArgumentList.push_back(complexFilters);
				}

				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("null");
				ffmpegArgumentList.push_back("-");
			}
			else
			{
				string errorMessage = std::format(
					"liveRecording. Wrong output type"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", outputType: {}",
					ingestionJobKey, encodingJobKey, outputType
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// 2. add: -filter_complex "[0:v][1:v]blend=difference:shortest=1,blackframe=99:32[f]" -map "[f]" -f null -
		if (framesToBeDetectedRoot != nullptr && framesToBeDetectedRoot.size() > 0)
		{
			for (int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size(); pictureIndex++)
			{
				json frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

				if (JSONUtils::isMetadataPresent(frameToBeDetectedRoot, "picturePathName"))
				{
					bool videoFrameToBeCropped = JSONUtils::asBool(frameToBeDetectedRoot, "videoFrameToBeCropped", false);

					ffmpegArgumentList.push_back("-filter_complex");

					int amount = JSONUtils::asInt(frameToBeDetectedRoot, "amount", 99);
					int threshold = JSONUtils::asInt(frameToBeDetectedRoot, "threshold", 32);

					string filter;

					if (videoFrameToBeCropped)
					{
						int width = JSONUtils::asInt(frameToBeDetectedRoot, "width", -1);
						int height = JSONUtils::asInt(frameToBeDetectedRoot, "height", -1);
						int videoCrop_X = JSONUtils::asInt(frameToBeDetectedRoot, "videoCrop_X", -1);
						int videoCrop_Y = JSONUtils::asInt(frameToBeDetectedRoot, "videoCrop_Y", -1);

						filter = "[0:v]crop=w=" + to_string(width) + ":h=" + to_string(height) + ":x=" + to_string(videoCrop_X) +
								 ":y=" + to_string(videoCrop_Y) + "[CROPPED];" + "[CROPPED][" + to_string(pictureIndex + 1) + ":v]" +
								 "blend=difference:shortest=1,blackframe=amount=" + to_string(amount) + ":threshold=" + to_string(threshold) +
								 "[differenceOut_" + to_string(pictureIndex + 1) + "]";
					}
					else
					{
						filter = "[0:v][" + to_string(pictureIndex + 1) + ":v]" +
								 "blend=difference:shortest=1,blackframe=amount=" + to_string(amount) + ":threshold=" + to_string(threshold) +
								 "[differenceOut_" + to_string(pictureIndex + 1) + "]";
					}
					ffmpegArgumentList.push_back(filter);

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back("[differenceOut_" + to_string(pictureIndex + 1) + "]");

					ffmpegArgumentList.push_back("-f");
					ffmpegArgumentList.push_back("null");

					ffmpegArgumentList.push_back("-");
				}
			}
		}

		bool sigQuitOrTermReceived = true;
		while (sigQuitOrTermReceived)
		{
			// inizialmente la variabile è -1, per cui il primo incremento la inizializza a 0
			(*numberOfRestartBecauseOfFailure)++;

			sigQuitOrTermReceived = false;

			if (!ffmpegArgumentList.empty())
				copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

			SPDLOG_INFO(
				"liveRecorder: Executing ffmpeg command"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _outputFfmpegPathFileName: {}"
				", ffmpegArgumentList: {}",
				ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
			);

			bool redirectionStdOutput = true;
			bool redirectionStdError = true;

			startFfmpegCommand = chrono::system_clock::now();

			ProcessUtility::forkAndExec(
				_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
				&iReturnedStatus
			);
			*pChildPid = 0;

			endFfmpegCommand = chrono::system_clock::now();

			int64_t realDuration = chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count();

			if (iReturnedStatus != 0)
			{
				string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				// Exiting normally, received signal 3.
				// 2023-02-18: ho verificato che SIGQUIT non ha funzionato e il processo non si è stoppato,
				//	mentre ha funzionato SIGTERM, per cui ora sto usando SIGTERM
				if (lastPartOfFfmpegOutputFile.find("signal 3") != string::npos // SIGQUIT
					|| lastPartOfFfmpegOutputFile.find("signal: 3") != string::npos ||
					lastPartOfFfmpegOutputFile.find("signal 15") != string::npos // SIGTERM
					|| lastPartOfFfmpegOutputFile.find("signal: 15") != string::npos)
				{
					sigQuitOrTermReceived = true;

					string errorMessage = std::format(
						"liveRecorder: ffmpeg execution command failed because received SIGQUIT/SIGTERM and is called again"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", iReturnedStatus: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", lastPartOfFfmpegOutputFile: {}"
						", difference between real and expected duration: {}",
						ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						lastPartOfFfmpegOutputFile, realDuration - streamingDuration
					);
					SPDLOG_ERROR(errorMessage);

					// in case of IP_PUSH the monitor thread, in case the client does not
					// reconnect istantaneously, kills the process.
					// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
					// we will set again the pRecordingStart variable
					{
						if (chrono::system_clock::from_time_t(utcRecordingPeriodStart) < chrono::system_clock::now())
							*pRecordingStart = chrono::system_clock::now() + chrono::seconds(localPushListenTimeout);
						else
							*pRecordingStart = chrono::system_clock::from_time_t(utcRecordingPeriodStart) + chrono::seconds(localPushListenTimeout);
					}

					{
						chrono::system_clock::time_point now = chrono::system_clock::now();
						utcNow = chrono::system_clock::to_time_t(now);

						if (utcNow < utcRecordingPeriodEnd)
						{
							time_t localStreamingDuration = utcRecordingPeriodEnd - utcNow;
							ffmpegArgumentList[streamingDurationIndex] = to_string(localStreamingDuration);

							SPDLOG_INFO(
								"liveRecorder: ffmpeg execution command failed because received SIGQUIT/SIGTERM, recalculate streaming duration"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", iReturnedStatus: {}"
								", _outputFfmpegPathFileName: {}"
								", localStreamingDuration: {}",
								ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, localStreamingDuration
							);
						}
						else
						{
							// exit from loop even if SIGQUIT/SIGTERM because time period expired
							sigQuitOrTermReceived = false;

							SPDLOG_INFO(
								"liveRecorder: ffmpeg execution command should be called again because received SIGQUIT/SIGTERM but "
								"utcRecordingPeriod expired"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", iReturnedStatus: {}"
								", _outputFfmpegPathFileName: {}"
								", ffmpegArgumentList: {}",
								ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
							);
						}

						continue;
					}
				}

				string errorMessage = std::format(
					"liveRecorder: ffmpeg: ffmpeg execution command failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", iReturnedStatus: {}"
					", _outputFfmpegPathFileName: {}"
					", ffmpegArgumentList: {}"
					", difference between real and expected duration: {}",
					ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
					realDuration - streamingDuration
				);
				SPDLOG_ERROR(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "liveRecorder: command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							   ", encodingJobKey: " + to_string(encodingJobKey);
				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			"liveRecorder: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", ffmpegArgumentList: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, encodingJobKey, ffmpegArgumentListStream.str(),
			chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);

		for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
		{
			json outputRoot = outputsRoot[outputIndex];

			string outputType = JSONUtils::asString(outputRoot, "outputType", "");

			// if (outputType == "HLS" || outputType == "DASH")
			if (outputType == "HLS_Channel")
			{
				string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");

				if (externalEncoder)
					removeFromIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

				if (manifestDirectoryPath != "")
				{
					if (fs::exists(manifestDirectoryPath))
					{
						try
						{
							SPDLOG_INFO(
								"removeDirectory"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", manifestDirectoryPath: {}",
								ingestionJobKey, encodingJobKey, manifestDirectoryPath
							);
							fs::remove_all(manifestDirectoryPath);
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								"remove directory failed"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", manifestDirectoryPath: {}"
								", e.what(): {}",
								ingestionJobKey, encodingJobKey, manifestDirectoryPath, e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"remove directory failed"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", manifestDirectoryPath: {}"
								", e.what(): {}",
								ingestionJobKey, encodingJobKey, manifestDirectoryPath, e.what()
							);

							// throw e;
						}
					}
				}
			}
		}

		// if (segmenterType == "streamSegmenter" || segmenterType == "hlsSegmenter")
		{
			if (segmentListPath != "" && fs::exists(segmentListPath))
			{
				try
				{
					SPDLOG_INFO(
						"removeDirectory"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}",
						ingestionJobKey, encodingJobKey, segmentListPath
					);
					fs::remove_all(segmentListPath);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
			}
		}

		/*
		// liveRecording exit before unexpectly
		if (endFfmpegCommand - startFfmpegCommand < chrono::seconds(utcRecordingPeriodEnd - utcNow - 60))
		{
			char		sEndFfmpegCommand [64];

			time_t	utcEndFfmpegCommand = chrono::system_clock::to_time_t(endFfmpegCommand);
			tm		tmUtcEndFfmpegCommand;
			localtime_r (&utcEndFfmpegCommand, &tmUtcEndFfmpegCommand);
			sprintf (sEndFfmpegCommand, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcEndFfmpegCommand. tm_year + 1900,
				tmUtcEndFfmpegCommand. tm_mon + 1,
				tmUtcEndFfmpegCommand. tm_mday,
				tmUtcEndFfmpegCommand. tm_hour,
				tmUtcEndFfmpegCommand. tm_min,
				tmUtcEndFfmpegCommand. tm_sec);

			string debugOutputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey) + "_"
				+ to_string(encodingJobKey) + "_"
				+ sEndFfmpegCommand
				+ ".liveRecorder.log.debug"
				;

			info(__FILEREF__ + "Coping"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
				);
			fs::copy(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);

			throw runtime_error("liveRecording exit before unexpectly");
		}
		*/
	}
	catch (runtime_error &e)
	{
		*pChildPid = 0;

		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9) // 9 means: SIGKILL
			errorMessage = std::format(
				"ffmpeg: ffmpeg: ffmpeg execution command failed because killed by the user"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _outputFfmpegPathFileName: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
			);
		else
			errorMessage = std::format(
				"ffmpeg: ffmpeg: ffmpeg execution command failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _outputFfmpegPathFileName: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
			);
		SPDLOG_ERROR(errorMessage);

		// copy ffmpeg log file
		/*
		{
			char		sEndFfmpegCommand [64];

			time_t	utcEndFfmpegCommand = chrono::system_clock::to_time_t(
				chrono::system_clock::now());
			tm		tmUtcEndFfmpegCommand;
			localtime_r (&utcEndFfmpegCommand, &tmUtcEndFfmpegCommand);
			sprintf (sEndFfmpegCommand, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcEndFfmpegCommand. tm_year + 1900,
				tmUtcEndFfmpegCommand. tm_mon + 1,
				tmUtcEndFfmpegCommand. tm_mday,
				tmUtcEndFfmpegCommand. tm_hour,
				tmUtcEndFfmpegCommand. tm_min,
				tmUtcEndFfmpegCommand. tm_sec);

			string debugOutputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey) + "_"
				+ to_string(encodingJobKey) + "_"
				+ sEndFfmpegCommand
				+ ".liveRecorder.log.debug"
			;

			info(__FILEREF__ + "Coping"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
				);
			fs::copy(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);
		}
		*/

		/*
		info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
		*/
		renameOutputFfmpegPathFileName(ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName);

		SPDLOG_INFO(
			"remove"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", segmentListPathName: {}",
			ingestionJobKey, encodingJobKey, segmentListPathName
		);
		fs::remove_all(segmentListPathName);

		for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
		{
			json outputRoot = outputsRoot[outputIndex];

			string outputType = JSONUtils::asString(outputRoot, "outputType", "");

			// if (outputType == "HLS" || outputType == "DASH")
			if (outputType == "HLS_Channel")
			{
				string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");

				if (externalEncoder)
					removeFromIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

				if (manifestDirectoryPath != "")
				{
					if (fs::exists(manifestDirectoryPath))
					{
						try
						{
							SPDLOG_INFO(
								"removeDirectory"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", manifestDirectoryPath: {}",
								ingestionJobKey, encodingJobKey, manifestDirectoryPath
							);
							fs::remove_all(manifestDirectoryPath);
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								"remove directory failed"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", manifestDirectoryPath: {}"
								", e.what(): {}",
								ingestionJobKey, encodingJobKey, manifestDirectoryPath, e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"remove directory failed"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", manifestDirectoryPath: {}"
								", e.what(): {}",
								ingestionJobKey, encodingJobKey, manifestDirectoryPath, e.what()
							);

							// throw e;
						}
					}
				}
			}
		}

		// if (segmenterType == "streamSegmenter" || segmenterType == "hlsSegmenter")
		{
			if (segmentListPath != "" && fs::exists(segmentListPath))
			{
				try
				{
					SPDLOG_INFO(
						"removeDirectory"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}",
						ingestionJobKey, encodingJobKey, segmentListPath
					);
					fs::remove_all(segmentListPath);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
			}
		}

		if (iReturnedStatus == 9) // 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
			throw FFMpegURLForbidden();
		else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
			throw FFMpegURLNotFound();
		else
			throw e;
	}

	/*
	info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
	bool exceptionInCaseOfError = false;
	fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
	*/
	renameOutputFfmpegPathFileName(ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName);
}

void FFMpeg::liveRecorder2(
	int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder, string segmentListPathName, string recordedFileNamePrefix,

	string otherInputOptions,

	// if streamSourceType is IP_PUSH means the liveURL should be like
	//		rtmp://<local transcoder IP to bind>:<port>
	//		listening for an incoming connection
	// else if streamSourceType is CaptureLive, liveURL is not used
	// else means the liveURL is "any thing" referring a stream
	string streamSourceType, // IP_PULL, TV, IP_PUSH, CaptureLive
	string liveURL,
	// Used only in case streamSourceType is IP_PUSH, Maximum time to wait for the incoming connection
	int pushListenTimeout,

	// parameters used only in case streamSourceType is CaptureLive
	int captureLive_videoDeviceNumber, string captureLive_videoInputFormat, int captureLive_frameRate, int captureLive_width, int captureLive_height,
	int captureLive_audioDeviceNumber, int captureLive_channelsNumber,

	string userAgent, time_t utcRecordingPeriodStart, time_t utcRecordingPeriodEnd,

	int segmentDurationInSeconds, string outputFileFormat,
	string segmenterType, // streamSegmenter or hlsSegmenter

	json outputsRoot,

	json framesToBeDetectedRoot,

	pid_t *pChildPid, chrono::system_clock::time_point *pRecordingStart, long *numberOfRestartBecauseOfFailure
)
{
	_currentApiName = APIName::LiveRecorder;

	SPDLOG_INFO(
		"Received {}"
		", ingestionJobKey: {}"
		", encodingJobKey: {}",
		toString(_currentApiName), ingestionJobKey, encodingJobKey
	);

	setStatus(
		ingestionJobKey, encodingJobKey
		/*
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	string segmentListPath;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;
	time_t utcNow;

	try
	{
		size_t segmentListPathIndex = segmentListPathName.find_last_of("/");
		if (segmentListPathIndex == string::npos)
		{
			string errorMessage = std::format(
				"No segmentListPath find in the segment path name"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", segmentListPathName: {}",
				ingestionJobKey, encodingJobKey, segmentListPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		segmentListPath = segmentListPathName.substr(0, segmentListPathIndex);

		// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
		// I saw just once that the directory was not created and the liveencoder remains in the loop
		// where:
		//	1. the encoder returns an error becaise of the missing directory
		//	2. EncoderVideoAudioProxy calls again the encoder
		// So, for this reason, the below check is done
		if (!fs::exists(segmentListPath))
		{
			SPDLOG_WARN(
				"segmentListPath does not exist!!! It will be created"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", segmentListPath: {}",
				ingestionJobKey, encodingJobKey, segmentListPath
			);

			SPDLOG_INFO(
				"Create directory"
				", segmentListPath: {}",
				segmentListPath
			);
			fs::create_directories(segmentListPath);
			fs::permissions(
				segmentListPath,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
					fs::perms::others_read | fs::perms::others_exec,
				fs::perm_options::replace
			);
		}

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		// 2021-03-06: I saw even if ffmpeg starts exactly at utcRecordingPeriodStart, the segments start time
		//	is about utcRecordingPeriodStart + 5 seconds.
		//	So, to be sure we have the recording at utcRecordingPeriodStart, we have to start ffmpeg
		//	at lease 5 seconds ahead
		int secondsAheadToStartFfmpeg = 10;
		time_t utcRecordingPeriodStartFixed = utcRecordingPeriodStart - secondsAheadToStartFfmpeg;
		if (utcNow < utcRecordingPeriodStartFixed)
		{
			// 2019-12-19: since the first chunk is removed, we will start a bit early
			// than utcRecordingPeriodStart (50% less than segmentDurationInSeconds)
			long secondsToStartEarly;
			if (segmenterType == "streamSegmenter")
				secondsToStartEarly = segmentDurationInSeconds * 50 / 100;
			else
				secondsToStartEarly = 0;

			while (utcNow + secondsToStartEarly < utcRecordingPeriodStartFixed)
			{
				time_t sleepTime = utcRecordingPeriodStartFixed - (utcNow + secondsToStartEarly);

				SPDLOG_INFO(
					"LiveRecorder timing. Too early to start the LiveRecorder, just sleep {} seconds"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", utcNow: {}"
					", secondsToStartEarly: {}"
					", utcRecordingPeriodStartFixed: {}",
					sleepTime, ingestionJobKey, encodingJobKey, utcNow, secondsToStartEarly, utcRecordingPeriodStartFixed
				);

				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}
			}
		}
		else if (utcRecordingPeriodEnd <= utcNow)
		{
			time_t tooLateTime = utcNow - utcRecordingPeriodEnd;

			string errorMessage = std::format(
				"LiveRecorder timing. Too late to start the LiveRecorder"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", utcNow: {}"
				", utcRecordingPeriodStartFixed: {}"
				", utcRecordingPeriodEnd: {}"
				", tooLateTime: {}",
				ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStartFixed, utcRecordingPeriodEnd, tooLateTime
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		else
		{
			time_t delayTime = utcNow - utcRecordingPeriodStartFixed;

			SPDLOG_WARN(
				"LiveRecorder timing. We are a bit late to start the LiveRecorder, let's start it"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", utcNow: {}"
				", utcRecordingPeriodStartFixed: {}"
				", utcRecordingPeriodEnd: {}"
				", delayTime: {}",
				ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStartFixed, utcRecordingPeriodEnd, delayTime
			);
		}

		{
			// char sUtcTimestamp[64];
			tm tmUtcTimestamp;
			time_t utcTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now());

			localtime_r(&utcTimestamp, &tmUtcTimestamp);
			/*
			sprintf(
				sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d", tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);

			_outputFfmpegPathFileName =
				std::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "liveRecorder", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
				*/
			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "liveRecorder", _currentIngestionJobKey,
				_currentEncodingJobKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);
		}

		string recordedFileNameTemplate = recordedFileNamePrefix;
		if (segmenterType == "streamSegmenter")
			recordedFileNameTemplate += "_%Y-%m-%d_%H-%M-%S_%s."; // viene letto il timestamp dal nome del file
		else													  // if (segmenterType == "hlsSegmenter")
			recordedFileNameTemplate += "_%04d.";				  // non viene letto il timestamp dal nome del file
		recordedFileNameTemplate += outputFileFormat;

		time_t streamingDuration = utcRecordingPeriodEnd - utcNow;

		SPDLOG_INFO(
			"LiveRecording timing. Streaming duration"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", utcNow: {}"
			", utcRecordingPeriodStart: {}"
			", utcRecordingPeriodEnd: {}"
			", streamingDuration: {}",
			ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStart, utcRecordingPeriodEnd, streamingDuration
		);

		int localPushListenTimeout = pushListenTimeout;
		if (streamSourceType == "IP_PUSH" || streamSourceType == "TV")
		{
			if (localPushListenTimeout > 0 && localPushListenTimeout > streamingDuration)
			{
				// 2021-02-02: sceanrio:
				//	streaming duration is 25 seconds
				//	timeout: 3600 seconds
				//	The result is that the process will finish after 3600 seconds, not after 25 seconds
				//	To avoid that, in this scenario, we will set the timeout equals to streamingDuration
				SPDLOG_INFO(
					"LiveRecorder timing. Listen timeout in seconds is reduced because max after 'streamingDuration' the process has to finish"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", utcNow: {}"
					", utcRecordingPeriodStart: {}"
					", utcRecordingPeriodEnd: {}"
					", streamingDuration: {}"
					", localPushListenTimeout: {}",
					ingestionJobKey, encodingJobKey, utcNow, utcRecordingPeriodStart, utcRecordingPeriodEnd, streamingDuration, localPushListenTimeout
				);

				localPushListenTimeout = streamingDuration;
			}
		}

		// user agent is an HTTP header and can be used only in case of http request
		bool userAgentToBeUsed = false;
		if (streamSourceType == "IP_PULL" && userAgent != "")
		{
			string httpPrefix = "http"; // it includes also https
			if (liveURL.size() >= httpPrefix.size() && liveURL.compare(0, httpPrefix.size(), httpPrefix) == 0)
			{
				userAgentToBeUsed = true;
			}
			else
			{
				SPDLOG_WARN(
					"user agent cannot be used if not http"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", liveURL: {}",
					ingestionJobKey, encodingJobKey, liveURL
				);
			}
		}

		ffmpegArgumentList.push_back("ffmpeg");
		// FFMpegEncodingParameters::addToArguments("-loglevel repeat+level+trace", ffmpegArgumentList);
		if (userAgentToBeUsed)
		{
			ffmpegArgumentList.push_back("-user_agent");
			ffmpegArgumentList.push_back(userAgent);
		}

		if (otherInputOptions != "")
		{
			FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegArgumentList);
		}

		if (framesToBeDetectedRoot != nullptr && framesToBeDetectedRoot.size() > 0)
		{
			// 2022-05-28; in caso di framedetection, we will "fix" PTS
			//	otherwise the one logged seems are not correct.
			//	Using +genpts are OK
			ffmpegArgumentList.push_back("-fflags");
			ffmpegArgumentList.push_back("+genpts");
		}

		if (streamSourceType == "IP_PUSH")
		{
			// listen/timeout depend on the protocol (https://ffmpeg.org/ffmpeg-protocols.html)
			if (liveURL.find("http://") != string::npos || liveURL.find("rtmp://") != string::npos)
			{
				ffmpegArgumentList.push_back("-listen");
				ffmpegArgumentList.push_back("1");
				if (localPushListenTimeout > 0)
				{
					// no timeout means it will listen infinitely
					ffmpegArgumentList.push_back("-timeout");
					ffmpegArgumentList.push_back(to_string(localPushListenTimeout));
				}
			}
			else if (liveURL.find("udp://") != string::npos)
			{
				if (localPushListenTimeout > 0)
				{
					int64_t listenTimeoutInMicroSeconds = localPushListenTimeout;
					listenTimeoutInMicroSeconds *= 1000000;
					liveURL += "?timeout=" + to_string(listenTimeoutInMicroSeconds);
				}
			}
			else
			{
				SPDLOG_ERROR(
					"listen/timeout not managed yet for the current protocol"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", liveURL: {}",
					ingestionJobKey, encodingJobKey, liveURL
				);
			}

			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(liveURL);
		}
		else if (streamSourceType == "IP_PULL" || streamSourceType == "TV")
		{
			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(liveURL);
		}
		else if (streamSourceType == "CaptureLive")
		{
			// video
			{
				// -f v4l2 -framerate 25 -video_size 640x480 -i /dev/video0
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("v4l2");

				ffmpegArgumentList.push_back("-thread_queue_size");
				ffmpegArgumentList.push_back("4096");

				if (captureLive_videoInputFormat != "")
				{
					ffmpegArgumentList.push_back("-input_format");
					ffmpegArgumentList.push_back(captureLive_videoInputFormat);
				}

				if (captureLive_frameRate != -1)
				{
					ffmpegArgumentList.push_back("-framerate");
					ffmpegArgumentList.push_back(to_string(captureLive_frameRate));
				}

				if (captureLive_width != -1 && captureLive_height != -1)
				{
					ffmpegArgumentList.push_back("-video_size");
					ffmpegArgumentList.push_back(to_string(captureLive_width) + "x" + to_string(captureLive_height));
				}

				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(string("/dev/video") + to_string(captureLive_videoDeviceNumber));
			}

			// audio
			{
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("alsa");

				ffmpegArgumentList.push_back("-thread_queue_size");
				ffmpegArgumentList.push_back("2048");

				if (captureLive_channelsNumber != -1)
				{
					ffmpegArgumentList.push_back("-ac");
					ffmpegArgumentList.push_back(to_string(captureLive_channelsNumber));
				}

				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(string("hw:") + to_string(captureLive_audioDeviceNumber));
			}
		}

		// to detect a frame we have to:
		// 1. add -r 1 -loop 1 -i <picture path name of the frame to be detected>
		// 2. add: -filter_complex "[0:v][1:v]blend=difference:shortest=1,blackframe=99:32[f]" -map "[f]" -f null -
		if (framesToBeDetectedRoot != nullptr && framesToBeDetectedRoot.size() > 0)
		{
			for (int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size(); pictureIndex++)
			{
				json frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

				if (JSONUtils::isMetadataPresent(frameToBeDetectedRoot, "picturePathName"))
				{
					string picturePathName = JSONUtils::asString(frameToBeDetectedRoot, "picturePathName", "");

					ffmpegArgumentList.push_back("-r");
					ffmpegArgumentList.push_back("1");

					ffmpegArgumentList.push_back("-loop");
					ffmpegArgumentList.push_back("1");

					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(picturePathName);
				}
			}
		}

		int streamingDurationIndex;
		{
			ffmpegArgumentList.push_back("-t");
			streamingDurationIndex = ffmpegArgumentList.size();
			ffmpegArgumentList.push_back(to_string(streamingDuration));
		}

		// this is to get all video tracks
		ffmpegArgumentList.push_back("-map");
		ffmpegArgumentList.push_back("0:v");

		ffmpegArgumentList.push_back("-c:v");
		ffmpegArgumentList.push_back("copy");

		// this is to get all audio tracks
		ffmpegArgumentList.push_back("-map");
		ffmpegArgumentList.push_back("0:a");

		ffmpegArgumentList.push_back("-c:a");
		ffmpegArgumentList.push_back("copy");

		if (segmenterType == "streamSegmenter")
		{
			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("segment");
			ffmpegArgumentList.push_back("-segment_list");
			ffmpegArgumentList.push_back(segmentListPathName);
			ffmpegArgumentList.push_back("-segment_time");
			ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
			ffmpegArgumentList.push_back("-segment_atclocktime");
			ffmpegArgumentList.push_back("1");
			ffmpegArgumentList.push_back("-strftime");
			ffmpegArgumentList.push_back("1");
			ffmpegArgumentList.push_back(segmentListPath + "/" + recordedFileNameTemplate);
		}
		else // if (segmenterType == "hlsSegmenter")
		{
			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("append_list");
			ffmpegArgumentList.push_back("-hls_time");
			ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
			ffmpegArgumentList.push_back("-hls_list_size");
			ffmpegArgumentList.push_back("10");

			// Segment files removed from the playlist are deleted after a period of time
			// equal to the duration of the segment plus the duration of the playlist
			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("delete_segments");

			// Set the number of unreferenced segments to keep on disk
			// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
			// to download segments which were recently referenced in the playlist.
			// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
			ffmpegArgumentList.push_back("-hls_delete_threshold");
			ffmpegArgumentList.push_back(to_string(1));

			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("program_date_time");

			ffmpegArgumentList.push_back("-hls_segment_filename");
			ffmpegArgumentList.push_back(segmentListPath + "/" + recordedFileNameTemplate);

			// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
			// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
			// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
			// 2020-07-19: commented, if it is needed just test it
			// ffmpegArgumentList.push_back("-hls_start_number_source");
			// ffmpegArgumentList.push_back("datetime");

			// 2020-07-19: commented, if it is needed just test it
			// ffmpegArgumentList.push_back("-start_number");
			// ffmpegArgumentList.push_back(to_string(10));

			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("hls");
			ffmpegArgumentList.push_back(segmentListPathName);
		}

		SPDLOG_INFO(
			"outputsRootToFfmpeg..."
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", outputsRoot.size: {}",
			ingestionJobKey, encodingJobKey, outputsRoot.size()
		);
		outputsRootToFfmpeg(
			ingestionJobKey, encodingJobKey, externalEncoder,
			"",		 // otherOutputOptionsBecauseOfMaxWidth,
			nullptr, // inputDrawTextDetailsRoot,
			-1,		 // streamingDurationInSeconds,
			outputsRoot, ffmpegArgumentList
		);

		// 2. add: -filter_complex "[0:v][1:v]blend=difference:shortest=1,blackframe=99:32[f]" -map "[f]" -f null -
		if (framesToBeDetectedRoot != nullptr && framesToBeDetectedRoot.size() > 0)
		{
			for (int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size(); pictureIndex++)
			{
				json frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

				if (JSONUtils::isMetadataPresent(frameToBeDetectedRoot, "picturePathName"))
				{
					bool videoFrameToBeCropped = JSONUtils::asBool(frameToBeDetectedRoot, "videoFrameToBeCropped", false);

					ffmpegArgumentList.push_back("-filter_complex");

					int amount = JSONUtils::asInt(frameToBeDetectedRoot, "amount", 99);
					int threshold = JSONUtils::asInt(frameToBeDetectedRoot, "threshold", 32);

					string filter;

					if (videoFrameToBeCropped)
					{
						int width = JSONUtils::asInt(frameToBeDetectedRoot, "width", -1);
						int height = JSONUtils::asInt(frameToBeDetectedRoot, "height", -1);
						int videoCrop_X = JSONUtils::asInt(frameToBeDetectedRoot, "videoCrop_X", -1);
						int videoCrop_Y = JSONUtils::asInt(frameToBeDetectedRoot, "videoCrop_Y", -1);

						filter = "[0:v]crop=w=" + to_string(width) + ":h=" + to_string(height) + ":x=" + to_string(videoCrop_X) +
								 ":y=" + to_string(videoCrop_Y) + "[CROPPED];" + "[CROPPED][" + to_string(pictureIndex + 1) + ":v]" +
								 "blend=difference:shortest=1,blackframe=amount=" + to_string(amount) + ":threshold=" + to_string(threshold) +
								 "[differenceOut_" + to_string(pictureIndex + 1) + "]";
					}
					else
					{
						filter = "[0:v][" + to_string(pictureIndex + 1) + ":v]" +
								 "blend=difference:shortest=1,blackframe=amount=" + to_string(amount) + ":threshold=" + to_string(threshold) +
								 "[differenceOut_" + to_string(pictureIndex + 1) + "]";
					}
					ffmpegArgumentList.push_back(filter);

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back("[differenceOut_" + to_string(pictureIndex + 1) + "]");

					ffmpegArgumentList.push_back("-f");
					ffmpegArgumentList.push_back("null");

					ffmpegArgumentList.push_back("-");
				}
			}
		}

		bool sigQuitOrTermReceived = true;
		while (sigQuitOrTermReceived)
		{
			// inizialmente la variabile è -1, per cui il primo incremento la inizializza a 0
			(*numberOfRestartBecauseOfFailure)++;

			sigQuitOrTermReceived = false;

			if (!ffmpegArgumentList.empty())
				copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

			SPDLOG_INFO(
				"liveRecorder: Executing ffmpeg command"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _outputFfmpegPathFileName: {}"
				", ffmpegArgumentList: {}",
				ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
			);

			bool redirectionStdOutput = true;
			bool redirectionStdError = true;

			startFfmpegCommand = chrono::system_clock::now();

			ProcessUtility::forkAndExec(
				_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
				&iReturnedStatus
			);
			*pChildPid = 0;

			endFfmpegCommand = chrono::system_clock::now();

			int64_t realDuration = chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count();

			if (iReturnedStatus != 0)
			{
				string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				// Exiting normally, received signal 3.
				// 2023-02-18: ho verificato che SIGQUIT non ha funzionato e il processo non si è stoppato,
				//	mentre ha funzionato SIGTERM, per cui ora sto usando SIGTERM
				if (lastPartOfFfmpegOutputFile.find("signal 3") != string::npos // SIGQUIT
					|| lastPartOfFfmpegOutputFile.find("signal: 3") != string::npos ||
					lastPartOfFfmpegOutputFile.find("signal 15") != string::npos // SIGTERM
					|| lastPartOfFfmpegOutputFile.find("signal: 15") != string::npos)
				{
					sigQuitOrTermReceived = true;

					string errorMessage = std::format(
						"liveRecorder: ffmpeg execution command failed because received SIGQUIT/SIGTERM and is called again"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", iReturnedStatus: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", lastPartOfFfmpegOutputFile: {}"
						", difference between real and expected duration: {}",
						ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						lastPartOfFfmpegOutputFile, realDuration - streamingDuration
					);
					SPDLOG_ERROR(errorMessage);

					// in case of IP_PUSH the monitor thread, in case the client does not
					// reconnect istantaneously, kills the process.
					// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
					// we will set again the pRecordingStart variable
					{
						if (chrono::system_clock::from_time_t(utcRecordingPeriodStart) < chrono::system_clock::now())
							*pRecordingStart = chrono::system_clock::now() + chrono::seconds(localPushListenTimeout);
						else
							*pRecordingStart = chrono::system_clock::from_time_t(utcRecordingPeriodStart) + chrono::seconds(localPushListenTimeout);
					}

					{
						chrono::system_clock::time_point now = chrono::system_clock::now();
						utcNow = chrono::system_clock::to_time_t(now);

						if (utcNow < utcRecordingPeriodEnd)
						{
							time_t localStreamingDuration = utcRecordingPeriodEnd - utcNow;
							ffmpegArgumentList[streamingDurationIndex] = to_string(localStreamingDuration);

							SPDLOG_INFO(
								"liveRecorder: ffmpeg execution command failed because received SIGQUIT/SIGTERM, recalculate streaming duration"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", iReturnedStatus: {}"
								", _outputFfmpegPathFileName: {}"
								", localStreamingDuration: {}",
								ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, localStreamingDuration
							);
						}
						else
						{
							// exit from loop even if SIGQUIT/SIGTERM because time period expired
							sigQuitOrTermReceived = false;

							SPDLOG_INFO(
								"liveRecorder: ffmpeg execution command should be called again because received SIGQUIT/SIGTERM but "
								"utcRecordingPeriod expired"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", iReturnedStatus: {}"
								", _outputFfmpegPathFileName: {}"
								", ffmpegArgumentList: {}",
								ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
							);
						}

						continue;
					}
				}

				string errorMessage = std::format(
					"liveRecorder: ffmpeg: ffmpeg execution command failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", iReturnedStatus: {}"
					", _outputFfmpegPathFileName: {}"
					", ffmpegArgumentList: {}"
					", difference between real and expected duration: {}",
					ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
					realDuration - streamingDuration
				);
				SPDLOG_ERROR(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "liveRecorder: command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							   ", encodingJobKey: " + to_string(encodingJobKey);
				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			"liveRecorder: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", ffmpegArgumentList: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, encodingJobKey, ffmpegArgumentListStream.str(),
			chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);

		try
		{
			outputsRootToFfmpeg_clean(ingestionJobKey, encodingJobKey, outputsRoot, externalEncoder);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"outputsRootToFfmpeg_clean failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, e.what()
			);

			// throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"outputsRootToFfmpeg_clean failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, e.what()
			);

			// throw e;
		}

		// if (segmenterType == "streamSegmenter" || segmenterType == "hlsSegmenter")
		{
			if (segmentListPath != "" && fs::exists(segmentListPath))
			{
				try
				{
					SPDLOG_INFO(
						"removeDirectory"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}",
						ingestionJobKey, encodingJobKey, segmentListPath
					);
					fs::remove_all(segmentListPath);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
			}
		}

		/*
		// liveRecording exit before unexpectly
		if (endFfmpegCommand - startFfmpegCommand < chrono::seconds(utcRecordingPeriodEnd - utcNow - 60))
		{
			char		sEndFfmpegCommand [64];

			time_t	utcEndFfmpegCommand = chrono::system_clock::to_time_t(endFfmpegCommand);
			tm		tmUtcEndFfmpegCommand;
			localtime_r (&utcEndFfmpegCommand, &tmUtcEndFfmpegCommand);
			sprintf (sEndFfmpegCommand, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcEndFfmpegCommand. tm_year + 1900,
				tmUtcEndFfmpegCommand. tm_mon + 1,
				tmUtcEndFfmpegCommand. tm_mday,
				tmUtcEndFfmpegCommand. tm_hour,
				tmUtcEndFfmpegCommand. tm_min,
				tmUtcEndFfmpegCommand. tm_sec);

			string debugOutputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey) + "_"
				+ to_string(encodingJobKey) + "_"
				+ sEndFfmpegCommand
				+ ".liveRecorder.log.debug"
				;

			info(__FILEREF__ + "Coping"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
				);
			fs::copy(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);

			throw runtime_error("liveRecording exit before unexpectly");
		}
		*/
	}
	catch (runtime_error &e)
	{
		*pChildPid = 0;

		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9) // 9 means: SIGKILL
			errorMessage = std::format(
				"ffmpeg: ffmpeg execution command failed because killed by the user"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _outputFfmpegPathFileName: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
			);
		else
			errorMessage = std::format(
				"ffmpeg: ffmpeg execution command failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _outputFfmpegPathFileName: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
			);
		SPDLOG_ERROR(errorMessage);

		// copy ffmpeg log file
		/*
		{
			char		sEndFfmpegCommand [64];

			time_t	utcEndFfmpegCommand = chrono::system_clock::to_time_t(
				chrono::system_clock::now());
			tm		tmUtcEndFfmpegCommand;
			localtime_r (&utcEndFfmpegCommand, &tmUtcEndFfmpegCommand);
			sprintf (sEndFfmpegCommand, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcEndFfmpegCommand. tm_year + 1900,
				tmUtcEndFfmpegCommand. tm_mon + 1,
				tmUtcEndFfmpegCommand. tm_mday,
				tmUtcEndFfmpegCommand. tm_hour,
				tmUtcEndFfmpegCommand. tm_min,
				tmUtcEndFfmpegCommand. tm_sec);

			string debugOutputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey) + "_"
				+ to_string(encodingJobKey) + "_"
				+ sEndFfmpegCommand
				+ ".liveRecorder.log.debug"
			;

			info(__FILEREF__ + "Coping"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
				);
			fs::copy(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);
		}
		*/

		/*
		info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
		*/
		renameOutputFfmpegPathFileName(ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName);

		SPDLOG_INFO(
			"remove"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", segmentListPathName: {}",
			ingestionJobKey, encodingJobKey, segmentListPathName
		);
		fs::remove_all(segmentListPathName);

		try
		{
			outputsRootToFfmpeg_clean(ingestionJobKey, encodingJobKey, outputsRoot, externalEncoder);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"outputsRootToFfmpeg_clean failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, e.what()
			);

			// throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"outputsRootToFfmpeg_clean failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, e.what()
			);

			// throw e;
		}

		// if (segmenterType == "streamSegmenter" || segmenterType == "hlsSegmenter")
		{
			if (segmentListPath != "" && fs::exists(segmentListPath))
			{
				try
				{
					SPDLOG_INFO(
						"removeDirectory"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}",
						ingestionJobKey, encodingJobKey, segmentListPath
					);
					fs::remove_all(segmentListPath);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"remove directory failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", segmentListPath: {}"
						", e.what(): {}",
						ingestionJobKey, encodingJobKey, segmentListPath, e.what()
					);

					// throw e;
				}
			}
		}

		if (iReturnedStatus == 9) // 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
			throw FFMpegURLForbidden();
		else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
			throw FFMpegURLNotFound();
		else
			throw e;
	}

	/*
	info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
	bool exceptionInCaseOfError = false;
	fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
	*/
	renameOutputFfmpegPathFileName(ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName);
}
