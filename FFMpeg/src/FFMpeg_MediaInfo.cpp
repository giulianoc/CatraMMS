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
#include "JSONUtils.h"
#include "catralibraries/ProcessUtility.h"
#include <fstream>
/*
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "MMSCURL.h"
#include "catralibraries/StringUtils.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
*/

tuple<int64_t, long, json> FFMpeg::getMediaInfo(
	int64_t ingestionJobKey,
	bool isMMSAssetPathName, // false means it is a URL
	int timeoutInSeconds,	 // used only in case of URL
	string mediaSource, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
	vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks
)
{
	_currentApiName = APIName::GetMediaInfo;

	SPDLOG_INFO(
		"getMediaInfo"
		", ingestionJobKey: {}"
		", isMMSAssetPathName: {}"
		", mediaSource: {}",
		ingestionJobKey, isMMSAssetPathName, mediaSource
	);

	if (mediaSource == "")
	{
		string errorMessage = fmt::format(
			"Media Source is wrong"
			", ingestionJobKey: {}"
			", mediaSource: {}",
			ingestionJobKey, mediaSource
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	// milli secs to wait in case of nfs delay
	if (isMMSAssetPathName)
	{
		bool exists = false;
		{
			chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
			do
			{
				if (fs::exists(mediaSource))
				{
					exists = true;
					break;
				}

				this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
			} while (chrono::system_clock::now() < end);
		}
		if (!exists)
		{
			string errorMessage =
				string("Source asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		/*
		size_t fileNameIndex = mediaSource.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "ffmpeg: No fileName find in the asset path name"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaSource: " + mediaSource;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sourceFileName = mediaSource.substr(fileNameIndex + 1);
		*/
	}

	string detailsPathFileName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + ".json";

	string timeoutParameter;
	if (!isMMSAssetPathName)
	{
		// 2023-01-23: il parametro timeout di ffprobe dipende dal protocollo della URL
		//	E' possibile avere le opzioni con il comando "ffprobe -help" cercando per "timeout"

		string mediaSourceLowerCase;
		mediaSourceLowerCase.resize(mediaSource.size());
		transform(mediaSource.begin(), mediaSource.end(), mediaSourceLowerCase.begin(), [](unsigned char c) { return tolower(c); });

		// 2023-01-23: udp è stato verificato con udp://@239.255.1.1:8013 (tv dig terr)
		if (mediaSourceLowerCase.find("udp://") != string::npos)
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds * 1000000) + " "; // micro secs
		else if (mediaSourceLowerCase.find("rtmp://") != string::npos)
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds) + " "; // secs
		else if (mediaSourceLowerCase.find("rtmps://") != string::npos)
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds) + " "; // secs
		else if (mediaSourceLowerCase.find("rtp://") != string::npos)
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds * 1000000) + " "; // micro secs
		else if (mediaSourceLowerCase.find("srt://") != string::npos)
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds * 1000000) + " "; // micro secs
		else if (mediaSourceLowerCase.find("http://") != string::npos)
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds * 1000000) + " "; // micro secs
		// 2023-01-23: https è stato verificato con https://radioitaliatv.akamaized.net/hls/live/2093117/RadioitaliaTV/master.m3u8
		else if (mediaSourceLowerCase.find("https://") != string::npos)
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds * 1000000) + " "; // micro secs
		else
			timeoutParameter = "-timeout " + to_string(timeoutInSeconds * 1000000) + " "; // micro secs
	}

	/*
	 * ffprobe:
		"-v quiet": Don't output anything else but the desired raw data value
		"-print_format": Use a certain format to print out the data
		"compact=": Use a compact output format
		"print_section=0": Do not print the section name
		":nokey=1": do not print the key of the key:value pair
		":escape=csv": escape the value
		"-show_entries format=duration": Get entries of a field named duration inside a section named format
	*/
	string ffprobeExecuteCommand = _ffmpegPath + "/ffprobe " +
								   timeoutParameter
								   // + "-v quiet -print_format compact=print_section=0:nokey=1:escape=csv -show_entries format=duration "
								   + "-v quiet -print_format json -show_streams -show_format \"" + mediaSource + "\" " + "> " + detailsPathFileName +
								   " 2>&1";

#ifdef __APPLE__
	ffprobeExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

	chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();
	try
	{
		_logger->info(
			__FILEREF__ + "getMediaInfo: Executing ffprobe command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffprobeExecuteCommand: " + ffprobeExecuteCommand
		);

		// The check/retries below was done to manage the scenario where the file was created
		// by another MMSEngine and it is not found just because of nfs delay.
		// Really, looking the log, we saw the file is just missing and it is not an nfs delay
		int attemptIndex = 0;
		bool executeDone = false;
		while (!executeDone)
		{
			int executeCommandStatus = ProcessUtility::execute(ffprobeExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage =
					__FILEREF__ + "getMediaInfo: ffmpeg: ffprobe command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", executeCommandStatus: " + to_string(executeCommandStatus) + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "getMediaInfo command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
			else
			{
				executeDone = true;
			}
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "getMediaInfo: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffprobeExecuteCommand: @" + ffprobeExecuteCommand + "@" + ", @FFMPEG statistics@ - duration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);
	}
	catch (runtime_error &e)
	{
		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		string lastPartOfFfmpegOutputFile = getLastPartOfFile(detailsPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "getMediaInfo: Executed ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", ffprobeExecuteCommand: " + ffprobeExecuteCommand + ", @FFMPEG statistics@ - duration (secs): @" +
							  to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@" +
							  ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);

		throw e;
	}

	int64_t durationInMilliSeconds = -1;
	long bitRate = -1;
	json metadataRoot = nullptr;

	try
	{
		// json output will be like:
		/*
			{
				"streams": [
					{
						"index": 0,
						"codec_name": "mpeg4",
						"codec_long_name": "MPEG-4 part 2",
						"profile": "Advanced Simple Profile",
						"codec_type": "video",
						"codec_time_base": "1/25",
						"codec_tag_string": "XVID",
						"codec_tag": "0x44495658",
						"width": 712,
						"height": 288,
						"coded_width": 712,
						"coded_height": 288,
						"has_b_frames": 1,
						"sample_aspect_ratio": "1:1",
						"display_aspect_ratio": "89:36",
						"pix_fmt": "yuv420p",
						"level": 5,
						"chroma_location": "left",
						"refs": 1,
						"quarter_sample": "false",
						"divx_packed": "false",
						"r_frame_rate": "25/1",
						"avg_frame_rate": "25/1",
						"time_base": "1/25",
						"start_pts": 0,
						"start_time": "0.000000",
						"duration_ts": 142100,
						"duration": "5684.000000",
						"bit_rate": "873606",
						"nb_frames": "142100",
						"disposition": {
							"default": 0,
							"dub": 0,
							"original": 0,
							"comment": 0,
							"lyrics": 0,
							"karaoke": 0,
							"forced": 0,
							"hearing_impaired": 0,
							"visual_impaired": 0,
							"clean_effects": 0,
							"attached_pic": 0,
							"timed_thumbnails": 0
						}
					},
					{
						"index": 1,
						"codec_name": "mp3",
						"codec_long_name": "MP3 (MPEG audio layer 3)",
						"codec_type": "audio",
						"codec_time_base": "1/48000",
						"codec_tag_string": "U[0][0][0]",
						"codec_tag": "0x0055",
						"sample_fmt": "s16p",
						"sample_rate": "48000",
						"channels": 2,
						"channel_layout": "stereo",
						"bits_per_sample": 0,
						"r_frame_rate": "0/0",
						"avg_frame_rate": "0/0",
						"time_base": "3/125",
						"start_pts": 0,
						"start_time": "0.000000",
						"duration_ts": 236822,
						"duration": "5683.728000",
						"bit_rate": "163312",
						"nb_frames": "236822",
						"disposition": {
							"default": 0,
							"dub": 0,
							"original": 0,
							"comment": 0,
							"lyrics": 0,
							"karaoke": 0,
							"forced": 0,
							"hearing_impaired": 0,
							"visual_impaired": 0,
							"clean_effects": 0,
							"attached_pic": 0,
							"timed_thumbnails": 0
						}
					}
				],
				"format": {
					"filename": "/Users/multi/VitadaCamper.avi",
					"nb_streams": 2,
					"nb_programs": 0,
					"format_name": "avi",
					"format_long_name": "AVI (Audio Video Interleaved)",
					"start_time": "0.000000",
					"duration": "5684.000000",
					"size": "745871360",
					"bit_rate": "1049783",
					"probe_score": 100,
					"tags": {
						"encoder": "VirtualDubMod 1.5.10.2 (build 2540/release)"
					}
				}
			}
		 */

		ifstream detailsFile(detailsPathFileName);
		stringstream buffer;
		buffer << detailsFile.rdbuf();

		_logger->info(
			__FILEREF__ + "Details found" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource +
			", details: " + buffer.str()
		);

		string mediaDetails = buffer.str();
		// LF and CR create problems to the json parser...
		while (mediaDetails.size() > 0 && (mediaDetails.back() == 10 || mediaDetails.back() == 13))
			mediaDetails.pop_back();

		json detailsRoot = JSONUtils::toJson(mediaDetails);

		string field = "streams";
		if (!JSONUtils::isMetadataPresent(detailsRoot, field))
		{
			string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", mediaSource: " + mediaSource + ", Field: " + field + ", mediaInfo: " + JSONUtils::toString(detailsRoot);
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						   ", mediaSource: " + mediaSource + ", Field: " + field + ", mediaInfo: " + JSONUtils::toString(detailsRoot);
			throw runtime_error(errorMessage);
		}
		json streamsRoot = detailsRoot[field];
		bool videoFound = false;
		bool audioFound = false;
		string firstVideoCodecName;
		for (int streamIndex = 0; streamIndex < streamsRoot.size(); streamIndex++)
		{
			json streamRoot = streamsRoot[streamIndex];

			field = "codec_type";
			if (!JSONUtils::isMetadataPresent(streamRoot, field))
			{
				string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							   ", mediaSource: " + mediaSource + ", Field: " + field;
				throw runtime_error(errorMessage);
			}
			string codecType = JSONUtils::asString(streamRoot, field, "");

			if (codecType == "video")
			{
				videoFound = true;

				int trackIndex;
				int64_t videoDurationInMilliSeconds = -1;
				string videoCodecName;
				string videoProfile;
				int videoWidth = -1;
				int videoHeight = -1;
				string videoAvgFrameRate;
				long videoBitRate = -1;

				field = "index";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				trackIndex = JSONUtils::asInt(streamRoot, field, 0);

				field = "codec_name";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					field = "codec_tag_string";
					if (!JSONUtils::isMetadataPresent(streamRoot, field))
					{
						string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource +
											  ", Field: " + field;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									   ", mediaSource: " + mediaSource + ", Field: " + field;
						throw runtime_error(errorMessage);
					}
				}
				videoCodecName = JSONUtils::asString(streamRoot, field, "");

				if (firstVideoCodecName == "")
					firstVideoCodecName = videoCodecName;

				field = "profile";
				if (JSONUtils::isMetadataPresent(streamRoot, field))
					videoProfile = JSONUtils::asString(streamRoot, field, "");
				else
				{
					/*
					if (videoCodecName != "mjpeg")
					{
						string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
								+ ", mediaSource: " + mediaSource
								+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					 */
				}

				field = "width";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				videoWidth = JSONUtils::asInt(streamRoot, field, 0);

				field = "height";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				videoHeight = JSONUtils::asInt(streamRoot, field, 0);

				field = "avg_frame_rate";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				videoAvgFrameRate = JSONUtils::asString(streamRoot, field, "");

				field = "bit_rate";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					if (videoCodecName != "mjpeg")
					{
						// I didn't find bit_rate also in a ts file, let's set it as a warning

						string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource +
											  ", Field: " + field;
						_logger->warn(errorMessage);

						// throw runtime_error(errorMessage);
					}
				}
				else
					videoBitRate = stol(JSONUtils::asString(streamRoot, field, ""));

				field = "duration";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					// I didn't find it in a .avi file generated using OpenCV::VideoWriter
					// let's log it as a warning
					if (videoCodecName != "" && videoCodecName != "mjpeg")
					{
						string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource +
											  ", Field: " + field;
						_logger->warn(errorMessage);

						// throw runtime_error(errorMessage);
					}
				}
				else
				{
					string duration = JSONUtils::asString(streamRoot, field, "0");

					// 2020-01-13: atoll remove the milliseconds and this is wrong
					// durationInMilliSeconds = atoll(duration.c_str()) * 1000;

					double dDurationInMilliSeconds = stod(duration);
					videoDurationInMilliSeconds = dDurationInMilliSeconds * 1000;
				}

				videoTracks.push_back(make_tuple(
					trackIndex, videoDurationInMilliSeconds, videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate
				));
			}
			else if (codecType == "audio")
			{
				audioFound = true;

				int trackIndex;
				int64_t audioDurationInMilliSeconds = -1;
				string audioCodecName;
				long audioSampleRate = -1;
				int audioChannels = -1;
				long audioBitRate = -1;

				field = "index";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				trackIndex = JSONUtils::asInt(streamRoot, field, 0);

				field = "codec_name";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				audioCodecName = JSONUtils::asString(streamRoot, field, "");

				field = "sample_rate";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				audioSampleRate = stol(JSONUtils::asString(streamRoot, field, ""));

				field = "channels";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", mediaSource: " + mediaSource + ", Field: " + field;
					throw runtime_error(errorMessage);
				}
				audioChannels = JSONUtils::asInt(streamRoot, field, 0);

				field = "bit_rate";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					// I didn't find bit_rate in a webm file, let's set it as a warning

					string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
					_logger->warn(errorMessage);

					// throw runtime_error(errorMessage);
				}
				else
					audioBitRate = stol(JSONUtils::asString(streamRoot, field, ""));

				field = "duration";
				if (JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string duration = JSONUtils::asString(streamRoot, field, "0");

					// 2020-01-13: atoll remove the milliseconds and this is wrong
					// durationInMilliSeconds = atoll(duration.c_str()) * 1000;

					double dDurationInMilliSeconds = stod(duration);
					audioDurationInMilliSeconds = dDurationInMilliSeconds * 1000;
				}

				string language;
				string tagsField = "tags";
				if (JSONUtils::isMetadataPresent(streamRoot, tagsField))
				{
					field = "language";
					if (JSONUtils::isMetadataPresent(streamRoot[tagsField], field))
					{
						language = JSONUtils::asString(streamRoot[tagsField], field, "");
					}
				}

				audioTracks.push_back(
					make_tuple(trackIndex, audioDurationInMilliSeconds, audioCodecName, audioSampleRate, audioChannels, audioBitRate, language)
				);
			}
		}

		field = "format";
		if (!JSONUtils::isMetadataPresent(detailsRoot, field))
		{
			string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", mediaSource: " + mediaSource + ", Field: " + field;
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						   ", mediaSource: " + mediaSource + ", Field: " + field;
			throw runtime_error(errorMessage);
		}
		json formatRoot = detailsRoot[field];

		field = "duration";
		if (!JSONUtils::isMetadataPresent(formatRoot, field))
		{
			// I didn't find it in a .avi file generated using OpenCV::VideoWriter
			// let's log it as a warning
			if (firstVideoCodecName != "" && firstVideoCodecName != "mjpeg")
			{
				string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}
		else
		{
			string duration = JSONUtils::asString(formatRoot, field, "");

			// 2020-01-13: atoll remove the milliseconds and this is wrong
			// durationInMilliSeconds = atoll(duration.c_str()) * 1000;

			double dDurationInMilliSeconds = stod(duration);
			durationInMilliSeconds = dDurationInMilliSeconds * 1000;
		}

		if (!JSONUtils::isMetadataPresent(formatRoot, "bit_rate"))
		{
			if (firstVideoCodecName != "" && firstVideoCodecName != "mjpeg")
			{
				SPDLOG_WARN(
					"ffmpeg: Field is not present or it is null"
					", ingestionJobKey: {}"
					", mediaSource: {}"
					", firstVideoCodecName: {}"
					", Field: bit_rate",
					ingestionJobKey, mediaSource, firstVideoCodecName
				);

				// throw runtime_error(errorMessage);
			}
		}
		else
		{
			string bit_rate = JSONUtils::asString(formatRoot, "bit_rate", "");
			bitRate = atoll(bit_rate.c_str());
		}

		// 2023-12-21: tags contiene possibili metadati all'interno del file
		//	Ad esempio, nel caso di file mxf ricevuti da RCS, recuperiamo il timecode (es: 09:59:00:00)
		//	Questo campo è importante perchè, separatamente, RCS ci da i CuePoints
		//		"CuePoints1":"10:00:00:00 / 10:19:00:10 / 10:20:00:00 / 10:39:32:13 / 10:41:00:00 / 10:53:10:06”
		//	Sottraendo dai CuePoints il timecode otteniamo i punti dove tagliare il file mxf per togliere
		//	lo "sporco". Per cui i tagli saranno:
		//	- da (10:00:00:00 - 09:59:00:00) a (10:19:00:10 - 09:59:00:00)
		//	- da (10:20:00:00 - 09:59:00:00) a (10:39:32:13 - 09:59:00:00)
		//	- da (10:41:00:00 - 09:59:00:00) a (10:53:10:06 - 09:59:00:00)
		//	e poi bisogna concatenare i tre tagli ottenuti
		field = "tags";
		if (JSONUtils::isMetadataPresent(formatRoot, field))
			metadataRoot = formatRoot[field];

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);
	}
	catch (runtime_error &e)
	{
		string errorMessage =
			__FILEREF__ + "ffmpeg: error processing ffprobe output" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage =
			__FILEREF__ + "ffmpeg: error processing ffprobe output" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);

		throw e;
	}

	/*
	if (durationInMilliSeconds == -1)
	{
		string errorMessage = __FILEREF__ + "ffmpeg: durationInMilliSeconds was not able to be retrieved from media"
				+ ", mediaSource: " + mediaSource
				+ ", durationInMilliSeconds: " + to_string(durationInMilliSeconds);
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	else if (width == -1 || height == -1)
	{
		string errorMessage = __FILEREF__ + "ffmpeg: width/height were not able to be retrieved from media"
				+ ", mediaSource: " + mediaSource
				+ ", width: " + to_string(width)
				+ ", height: " + to_string(height)
				;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	 */

	/*
	_logger->info(__FILEREF__ + "FFMpeg::getMediaInfo"
		+ ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
		+ ", bitRate: " + to_string(bitRate)
		+ ", videoCodecName: " + videoCodecName
		+ ", videoProfile: " + videoProfile
		+ ", videoWidth: " + to_string(videoWidth)
		+ ", videoHeight: " + to_string(videoHeight)
		+ ", videoAvgFrameRate: " + videoAvgFrameRate
		+ ", videoBitRate: " + to_string(videoBitRate)
		+ ", audioCodecName: " + audioCodecName
		+ ", audioSampleRate: " + to_string(audioSampleRate)
		+ ", audioChannels: " + to_string(audioChannels)
		+ ", audioBitRate: " + to_string(audioBitRate)
	);
	*/

	return make_tuple(durationInMilliSeconds, bitRate, metadataRoot);
}

string FFMpeg::getNearestKeyFrameTime(
	int64_t ingestionJobKey, string mediaSource,
	string readIntervals, // intervallo dove cercare il key frame piu vicino
						  // INTERVAL  ::= [START|+START_OFFSET][%[END|+END_OFFSET]]
						  // INTERVALS ::= INTERVAL[,INTERVALS]
						  // esempi:
						  //	Seek to time 10, read packets until 20 seconds after the found seek point, then seek to position 01:30 (1 minute and
						  // thirty seconds) and read packets until position 01:45. 			10%+20,01:30%01:45
						  // Read only 42 packets after seeking to position 01:23:
						  //			01:23%+#42
						  // Read only the first 20 seconds from the start:
						  //			%+20
						  // Read from the start until position 02:30:
						  //			%02:30
	double keyFrameTime
)
{
	_currentApiName = APIName::NearestKeyFrameTime;

	_logger->info(
		__FILEREF__ + "getNearestKeyFrameTime" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource +
		", readIntervals: " + readIntervals + ", keyFrameTime: " + to_string(keyFrameTime)
	);

	if (mediaSource == "")
	{
		string errorMessage = string("Media Source is wrong") + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	// milli secs to wait in case of nfs delay
	{
		bool exists = false;
		{
			chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
			do
			{
				if (fs::exists(mediaSource))
				{
					exists = true;
					break;
				}

				this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
			} while (chrono::system_clock::now() < end);
		}
		if (!exists)
		{
			string errorMessage =
				string("Source asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	string detailsPathFileName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + ".json";

	/*
	 * ffprobe:
		"-v quiet": Don't output anything else but the desired raw data value
		"-print_format": Use a certain format to print out the data
		"compact=": Use a compact output format
		"print_section=0": Do not print the section name
		":nokey=1": do not print the key of the key:value pair
		":escape=csv": escape the value
		"-show_entries format=duration": Get entries of a field named duration inside a section named format
		-sexagesimal:
	*/
	string ffprobeExecuteCommand = _ffmpegPath + "/ffprobe " + "-v quiet -select_streams v:0 -show_entries packet=pts_time,flags " +
								   "-print_format json -read_intervals \"" + readIntervals + "\" \"" + mediaSource + "\" " + "> " +
								   detailsPathFileName + " 2>&1";

#ifdef __APPLE__
	ffprobeExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

	chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();
	try
	{
		_logger->info(
			__FILEREF__ + "getNearestKeyFrameTime: Executing ffprobe command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffprobeExecuteCommand: " + ffprobeExecuteCommand
		);

		// The check/retries below was done to manage the scenario where the file was created
		// by another MMSEngine and it is not found just because of nfs delay.
		// Really, looking the log, we saw the file is just missing and it is not an nfs delay
		int attemptIndex = 0;
		bool executeDone = false;
		while (!executeDone)
		{
			int executeCommandStatus = ProcessUtility::execute(ffprobeExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage =
					__FILEREF__ + "getNearestKeyFrameTime: ffmpeg: ffprobe command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", executeCommandStatus: " + to_string(executeCommandStatus) + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "getNearestKeyFrameTime command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
			else
			{
				executeDone = true;
			}
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "getNearestKeyFrameTime: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffprobeExecuteCommand: @" + ffprobeExecuteCommand + "@" + ", @FFMPEG statistics@ - duration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);
	}
	catch (runtime_error &e)
	{
		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		string lastPartOfFfmpegOutputFile = getLastPartOfFile(detailsPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "getNearestKeyFrameTime: Executed ffmpeg command failed" +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand +
							  ", @FFMPEG statistics@ - duration (secs): @" +
							  to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@" +
							  ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);

		throw e;
	}

	string sNearestKeyFrameTime;

	try
	{
		// json output will be like:
		/*
{
	"packets": [
		{
			"pts_time": "536.981333",
			"flags": "__",
			"side_data_list": [
				{

				}
			]
		},
		{
			"pts_time": "537.061333",
			"flags": "K_",
			"side_data_list": [
				{

				}
			]
		},
		{
			"pts_time": "537.261333",
			"flags": "__",
			"side_data_list": [
				{

				}
			]
		},
		............
		{
			"pts_time": "538.901333",
			"flags": "__",
			"side_data_list": [
				{

				}
			]
		}
	]
}
		 */

		double nearestDistance = -1.0;

		ifstream detailsFile(detailsPathFileName);
		stringstream buffer;
		buffer << detailsFile.rdbuf();

		_logger->info(
			__FILEREF__ + "Details found" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource +
			", details: " + buffer.str()
		);

		string mediaDetails = buffer.str();
		// LF and CR create problems to the json parser...
		while (mediaDetails.size() > 0 && (mediaDetails.back() == 10 || mediaDetails.back() == 13))
			mediaDetails.pop_back();

		json detailsRoot = JSONUtils::toJson(mediaDetails);

		string field = "packets";
		if (!JSONUtils::isMetadataPresent(detailsRoot, field))
		{
			string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", mediaSource: " + mediaSource + ", Field: " + field + ", mediaInfo: " + JSONUtils::toString(detailsRoot);
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						   ", mediaSource: " + mediaSource + ", Field: " + field + ", mediaInfo: " + JSONUtils::toString(detailsRoot);
			throw runtime_error(errorMessage);
		}
		json packetsRoot = detailsRoot[field];
		for (int packetIndex = 0; packetIndex < packetsRoot.size(); packetIndex++)
		{
			json packetRoot = packetsRoot[packetIndex];

			field = "pts_time";
			if (!JSONUtils::isMetadataPresent(packetRoot, field))
			{
				string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							   ", mediaSource: " + mediaSource + ", Field: " + field;
				throw runtime_error(errorMessage);
			}
			string pts_time = JSONUtils::asString(packetRoot, field, "");

			field = "flags";
			if (!JSONUtils::isMetadataPresent(packetRoot, field))
			{
				string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource + ", Field: " + field;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							   ", mediaSource: " + mediaSource + ", Field: " + field;
				throw runtime_error(errorMessage);
			}
			string flags = JSONUtils::asString(packetRoot, field, "");

			if (flags.find("K") != string::npos)
			{
				double currentKeyFrameTime = stod(pts_time);
				double currentDistance = abs(currentKeyFrameTime - keyFrameTime);

				if (nearestDistance == -1.0)
				{
					nearestDistance = currentDistance;
					sNearestKeyFrameTime = pts_time;
				}
				else if (currentDistance < nearestDistance)
				{
					nearestDistance = currentDistance;
					sNearestKeyFrameTime = pts_time;
				}
				else if (currentDistance >= nearestDistance)
				{
					// i packet sono in ordine crescente, se ci stiamo allontanando dal keyFrameTime è inutile continuare
					break;
				}
			}
		}

		if (nearestDistance == -1.0)
		{
			string errorMessage = __FILEREF__ + "getNearestKeyFrameTime failed to look for a key frame" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSource: " + mediaSource +
								  ", readIntervals: " + readIntervals + ", keyFrameTime: " + to_string(keyFrameTime);
			_logger->error(errorMessage);
		}

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);
	}
	catch (runtime_error &e)
	{
		string errorMessage =
			__FILEREF__ + "ffmpeg: error processing ffprobe output" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage =
			__FILEREF__ + "ffmpeg: error processing ffprobe output" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", detailsPathFileName: " + detailsPathFileName);
		fs::remove_all(detailsPathFileName);

		throw e;
	}

	return sNearestKeyFrameTime;
}

int FFMpeg::probeChannel(int64_t ingestionJobKey, string url)
{
	_currentApiName = APIName::ProbeChannel;

	string outputProbePathFileName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + ".probeChannel.log";

	_logger->info(__FILEREF__ + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url);

	// 2021-09-18: l'idea sarebbe che se il comando ritorna 124 (timeout), vuol dire
	//	che il play ha strimmato per i secondi indicati ed è stato fermato dal comando
	//	di timeout. Quindi in questo caso l'url funziona bene.
	//	Se invece ritorna 0 vuol dire che ffplay non è riuscito a strimmare, c'è
	//	stato un errore. Quindi in questo caso l'url NON funziona bene.
	string probeExecuteCommand = string("timeout 10 ") + _ffmpegPath + "/ffplay \"" + url + "\" " + "> " + outputProbePathFileName + " 2>&1";

	int executeCommandStatus = 0;
	try
	{
		_logger->info(
			__FILEREF__ + toString(_currentApiName) + ": Executing ffplay command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", probeExecuteCommand: " + probeExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		executeCommandStatus = ProcessUtility::execute(probeExecuteCommand);
		if (executeCommandStatus != 124)
		{
			string errorMessage = __FILEREF__ + toString(_currentApiName) + ": ffmpeg: probe command failed" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", executeCommandStatus: " + to_string(executeCommandStatus) +
								  ", probeExecuteCommand: " + probeExecuteCommand;
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + toString(_currentApiName) + ": probe command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey);
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + toString(_currentApiName) + ": Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", probeExecuteCommand: " + probeExecuteCommand + ", @FFMPEG statistics@ - duration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(outputProbePathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "ffmpeg: probe command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", probeExecuteCommand: " + probeExecuteCommand + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile +
							  ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(
			__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", outputProbePathFileName: " + outputProbePathFileName
		);
		fs::remove_all(outputProbePathFileName);

		// throw e;
		return 1;
	}

	return 0;
}

void FFMpeg::getLiveStreamingInfo(
	string liveURL, string userAgent, int64_t ingestionJobKey, int64_t encodingJobKey,
	vector<tuple<int, string, string, string, string, int, int>> &videoTracks, vector<tuple<int, string, string, string, int, bool>> &audioTracks
)
{

	_logger->info(
		__FILEREF__ + "getLiveStreamingInfo" + ", liveURL: " + liveURL + ", userAgent: " + userAgent +
		", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey)
	);

	string outputFfmpegPathFileName;
	string ffmpegExecuteCommand;

	try
	{
		outputFfmpegPathFileName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + "_" + to_string(encodingJobKey) + ".liveStreamingInfo.log";

		int liveDurationInSeconds = 5;

		// user agent is an HTTP header and can be used only in case of http request
		bool userAgentToBeUsed = false;
		if (userAgent != "")
		{
			string httpPrefix = "http"; // it includes also https
			if (liveURL.size() >= httpPrefix.size() && liveURL.compare(0, httpPrefix.size(), httpPrefix) == 0)
			{
				userAgentToBeUsed = true;
			}
			else
			{
				_logger->warn(
					__FILEREF__ + "getLiveStreamingInfo: user agent cannot be used if not http" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", liveURL: " + liveURL
				);
			}
		}

		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg " + "-nostdin " + (userAgentToBeUsed ? ("-user_agent \"" + userAgent + "\" ") : "") +
							   "-re -i \"" + liveURL + "\" " + "-t " + to_string(liveDurationInSeconds) + " " + "-c:v copy " + "-c:a copy " +
							   "-f null " + "/dev/null " + "> " + outputFfmpegPathFileName + " 2>&1";

#ifdef __APPLE__
		ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "getLiveStreamingInfo: Executing ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
		);
		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__ + "getLiveStreamingInfo failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", executeCommandStatus: " + to_string(executeCommandStatus) +
								  ", ffmpegExecuteCommand: " + ffmpegExecuteCommand;
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "getLiveStreamingInfo failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						   ", encodingJobKey: " + to_string(encodingJobKey);
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "getLiveStreamingInfo: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand +
			", @FFMPEG statistics@ - duration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "getLiveStreamingInfo failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", encodingJobKey: " + to_string(encodingJobKey) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand +
							  ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		if (fs::exists(outputFfmpegPathFileName.c_str()))
		{
			_logger->info(__FILEREF__ + "Remove" + ", outputFfmpegPathFileName: " + outputFfmpegPathFileName);
			fs::remove_all(outputFfmpegPathFileName);
		}

		throw e;
	}

	try
	{
		if (!fs::exists(outputFfmpegPathFileName.c_str()))
		{
			_logger->info(
				__FILEREF__ + "ffmpeg: ffmpeg status not available" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", outputFfmpegPathFileName: " + outputFfmpegPathFileName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		ifstream ifPathFileName(outputFfmpegPathFileName);

		int charsToBeRead = 200000;

		// get size/length of file:
		ifPathFileName.seekg(0, ifPathFileName.end);
		int fileSize = ifPathFileName.tellg();
		if (fileSize <= charsToBeRead)
			charsToBeRead = fileSize;

		ifPathFileName.seekg(0, ifPathFileName.beg);

		string firstPartOfFile;
		{
			char *buffer = new char[charsToBeRead];
			ifPathFileName.read(buffer, charsToBeRead);
			if (ifPathFileName)
			{
				// all characters read successfully
				firstPartOfFile.assign(buffer, charsToBeRead);
			}
			else
			{
				// error: only is.gcount() could be read";
				firstPartOfFile.assign(buffer, ifPathFileName.gcount());
			}
			ifPathFileName.close();

			delete[] buffer;
		}

		/*
  Program 0
	Metadata:
	  variant_bitrate : 200000
	Stream #0:0: Video: h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 320x180 [SAR 1:1 DAR 16:9], 15 fps, 15 tbr, 90k tbn, 30 tbc
	Metadata:
	  variant_bitrate : 200000
	Stream #0:1: Audio: aac (LC) ([15][0][0][0] / 0x000F), 32000 Hz, stereo, fltp
	Metadata:
	  variant_bitrate : 200000
	Stream #0:2: Data: timed_id3 (ID3  / 0x20334449)
	Metadata:
	  variant_bitrate : 200000
  Program 1
	Metadata:
	  variant_bitrate : 309000
	Stream #0:3: Video: h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 480x270 [SAR 1:1 DAR 16:9], 25 fps, 25 tbr, 90k tbn, 50 tbc
	Metadata:
	  variant_bitrate : 309000
	Stream #0:4: Audio: aac (LC) ([15][0][0][0] / 0x000F), 32000 Hz, stereo, fltp
	Metadata:
	  variant_bitrate : 309000
	Stream #0:5: Data: timed_id3 (ID3  / 0x20334449)
	Metadata:
	  variant_bitrate : 309000
...
Output #0, flv, to 'rtmp://prg-1.s.cdn77.com:1936/static/1620280677?password=DMGBKQCH':
		 */
		string programLabel = "  Program ";
		string outputLabel = "Output #0,";
		string streamLabel = "    Stream #";
		string videoLabel = "Video: ";
		string audioLabel = "Audio: ";
		string stereoLabel = ", stereo,";

		int startToLookForProgram = 0;
		bool programFound = true;
		int programId = 0;
		while (programFound)
		{
			string program;
			{
				/*
				_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Looking Program"
					+ ", programId: " + to_string(programId)
					+ ", startToLookForProgram: " + to_string(startToLookForProgram)
				);
				*/
				size_t programBeginIndex = firstPartOfFile.find(programLabel, startToLookForProgram);
				if (programBeginIndex == string::npos)
				{
					programFound = false;

					continue;
				}

				startToLookForProgram = (programBeginIndex + programLabel.size());
				size_t programEndIndex = firstPartOfFile.find(programLabel, startToLookForProgram);
				if (programEndIndex == string::npos)
				{
					programEndIndex = firstPartOfFile.find(outputLabel, startToLookForProgram);
					if (programEndIndex == string::npos)
					{
						/*
						_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. This is the last Program"
							+ ", programId: " + to_string(programId)
							+ ", startToLookForProgram: " + to_string(startToLookForProgram)
						);
						*/

						programFound = false;

						continue;
					}
				}

				program = firstPartOfFile.substr(programBeginIndex, programEndIndex - programBeginIndex);

				/*
				_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Program"
					+ ", programId: " + to_string(programId)
					+ ", programBeginIndex: " + to_string(programBeginIndex)
					+ ", programEndIndex: " + to_string(programEndIndex)
					+ ", program: " + program
				);
				*/
			}

			stringstream programStream(program);
			string line;

			while (getline(programStream, line))
			{
				// start with
				if (line.size() >= streamLabel.size() && 0 == line.compare(0, streamLabel.size(), streamLabel))
				{
					size_t codecStartIndex;
					if ((codecStartIndex = line.find(videoLabel)) != string::npos)
					{
						//     Stream #0:0: Video: h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 320x180 [SAR 1:1 DAR 16:9], 15 fps, 15
						//     tbr, 90k tbn, 30 tbc

						// video
						string videoStreamId;
						string videoStreamDescription;
						string videoCodec;
						string videoYUV;
						int videoWidth = -1;
						int videoHeight = -1;

						// stream id
						{
							string streamIdEndLabel = ": ";
							size_t streamIdEndIndex;
							if ((streamIdEndIndex = line.find(streamIdEndLabel, streamLabel.size())) != string::npos)
							{
								size_t streamIdStartIndex = streamLabel.size();

								// Stream #0:2(des): Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								//	or
								// Stream #0:2: Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								string localStreamId = line.substr(streamIdStartIndex, streamIdEndIndex - streamIdStartIndex);

								string streamDescriptionStartLabel = "(";
								size_t streamDescriptionStartIndex;
								if ((streamDescriptionStartIndex = localStreamId.find(streamDescriptionStartLabel, 0)) != string::npos)
								{
									// 0:2(des)
									videoStreamId = localStreamId.substr(0, streamDescriptionStartIndex);
									videoStreamDescription = localStreamId.substr(
										streamDescriptionStartIndex + 1, (localStreamId.size() - 2) - streamDescriptionStartIndex
									);
								}
								else
								{
									// 0:2
									videoStreamId = localStreamId;
								}
							}
						}

						// codec
						size_t codecEndIndex;
						{
							string codecEndLabel = ", yuv";
							if ((codecEndIndex = line.find(codecEndLabel, codecStartIndex)) != string::npos)
							{
								codecStartIndex += videoLabel.size();

								videoCodec = line.substr(codecStartIndex, codecEndIndex - codecStartIndex);
							}
						}

						// yuv
						size_t yuvEndIndex;
						{
							// h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv), 624x352
							//	or
							// h264 (Main) ([27][0][0][0] / 0x001B), yuv420p, 928x522
							//  or
							// h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 320x180
							string yuvEndLabel_1 = "), ";
							string yuvEndLabel_2 = ", ";
							if ((yuvEndIndex = line.find(yuvEndLabel_1, codecEndIndex + 1)) != string::npos)
							{
								videoYUV = line.substr(codecEndIndex + 2, (yuvEndIndex + 1) - (codecEndIndex + 2));
							}
							else if ((yuvEndIndex = line.find(yuvEndLabel_2, codecEndIndex + 1)) != string::npos)
							{
								videoYUV = line.substr(codecEndIndex + 2, yuvEndIndex - (codecEndIndex + 2));
							}
						}

						// width & height
						if (yuvEndIndex != string::npos)
						{
							string widthEndLabel = "x";
							size_t widthEndIndex;
							if ((widthEndIndex = line.find(widthEndLabel, yuvEndIndex + 2)) != string::npos)
							{
								string sWidth = line.substr(yuvEndIndex + 2, widthEndIndex - (yuvEndIndex + 2));
								try
								{
									videoWidth = stoi(sWidth);
								}
								catch (exception &e)
								{
									string errorMessage =
										__FILEREF__ + "getLiveStreamingInfo error" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										", encodingJobKey: " + to_string(encodingJobKey) + ", line: " + line +
										", yuvEndIndex: " + to_string(yuvEndIndex) + ", sWidth: " + sWidth + ", e.what(): " + e.what();
									_logger->error(errorMessage);
								}
							}

							string heightEndLabel = " ";
							size_t heightEndIndex;
							if ((heightEndIndex = line.find(heightEndLabel, widthEndIndex)) != string::npos)
							{
								string sHeight = line.substr(widthEndIndex + 1, heightEndIndex - (widthEndIndex + 1));
								try
								{
									videoHeight = stoi(sHeight);
								}
								catch (exception &e)
								{
									string errorMessage = __FILEREF__ + "getLiveStreamingInfo error" +
														  ", ingestionJobKey: " + to_string(ingestionJobKey) +
														  ", encodingJobKey: " + to_string(encodingJobKey) + ", line: " + line +
														  ", sHeight: " + sHeight + ", e.what(): " + e.what();
									_logger->error(errorMessage);
								}
							}
						}

						{
							videoTracks.push_back(
								make_tuple(programId, videoStreamId, videoStreamDescription, videoCodec, videoYUV, videoWidth, videoHeight)
							);

							_logger->info(
								__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Video track" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								", encodingJobKey: " + to_string(encodingJobKey) + ", programId: " + to_string(programId) + ", videoStreamId: " +
								videoStreamId + ", videoStreamDescription: " + videoStreamDescription + ", videoCodec: " + videoCodec +
								", videoYUV: " + videoYUV + ", videoWidth: " + to_string(videoWidth) + ", videoHeight: " + to_string(videoHeight)
							);
						}
					}
					else if ((codecStartIndex = line.find(audioLabel)) != string::npos)
					{
						// Stream #0:1: Audio: aac (LC) ([15][0][0][0] / 0x000F), 32000 Hz, stereo, fltp

						// audio
						string audioStreamId;
						string audioStreamDescription;
						string audioCodec;
						int audioSamplingRate = -1;
						bool audioStereo = false;

						// stream id
						{
							string streamIdEndLabel = ": ";
							size_t streamIdEndIndex;
							if ((streamIdEndIndex = line.find(streamIdEndLabel, streamLabel.size())) != string::npos)
							{
								size_t streamIdStartIndex = streamLabel.size();

								// Stream #0:2(des): Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								//	or
								// Stream #0:2: Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								string localStreamId = line.substr(streamIdStartIndex, streamIdEndIndex - streamIdStartIndex);

								string streamDescriptionStartLabel = "(";
								size_t streamDescriptionStartIndex;
								if ((streamDescriptionStartIndex = localStreamId.find(streamDescriptionStartLabel, 0)) != string::npos)
								{
									// 0:2(des)
									audioStreamId = localStreamId.substr(0, streamDescriptionStartIndex);
									audioStreamDescription = localStreamId.substr(
										streamDescriptionStartIndex + 1, (localStreamId.size() - 2) - streamDescriptionStartIndex
									);
								}
								else
								{
									// 0:2
									audioStreamId = localStreamId;
								}
							}
						}

						// codec
						size_t codecEndIndex;
						{
							string codecEndLabel = ", ";
							if ((codecEndIndex = line.find(codecEndLabel, codecStartIndex)) != string::npos)
							{
								codecStartIndex += audioLabel.size();

								audioCodec = line.substr(codecStartIndex, codecEndIndex - codecStartIndex);
							}
						}

						// samplingRate
						size_t samplingEndIndex = 0;
						{
							string samplingEndLabel = " Hz";
							if ((samplingEndIndex = line.find(samplingEndLabel, codecEndIndex)) != string::npos)
							{
								string sSamplingRate = line.substr(codecEndIndex + 2, samplingEndIndex - (codecEndIndex + 2));
								try
								{
									audioSamplingRate = stoi(sSamplingRate);
								}
								catch (exception &e)
								{
									string errorMessage = __FILEREF__ + "getLiveStreamingInfo error" +
														  ", ingestionJobKey: " + to_string(ingestionJobKey) +
														  ", encodingJobKey: " + to_string(encodingJobKey) + ", line: " + line +
														  ", sSamplingRate: " + sSamplingRate + ", e.what(): " + e.what();
									_logger->error(errorMessage);
								}
							}
						}

						// stereo
						{
							if (line.find(stereoLabel, samplingEndIndex) != string::npos)
								audioStereo = true;
							else
								audioStereo = false;
						}

						{
							audioTracks.push_back(
								make_tuple(programId, audioStreamId, audioStreamDescription, audioCodec, audioSamplingRate, audioStereo)
							);

							_logger->info(
								__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Audio track" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								", encodingJobKey: " + to_string(encodingJobKey) + ", programId: " + to_string(programId) + ", audioStreamId: " +
								audioStreamId + ", audioStreamDescription: " + audioStreamDescription + ", audioCodec: " + audioCodec +
								", audioSamplingRate: " + to_string(audioSamplingRate) + ", audioStereo: " + to_string(audioStereo)
							);
						}
					}
				}
			}

			programId++;
		}

		_logger->info(
			__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
			", outputFfmpegPathFileName: " + outputFfmpegPathFileName
		);
		fs::remove_all(outputFfmpegPathFileName);
	}
	catch (runtime_error &e)
	{
		string errorMessage = __FILEREF__ + "getLiveStreamingInfo error" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", outputFfmpegPathFileName: " + outputFfmpegPathFileName);
		fs::remove_all(outputFfmpegPathFileName);

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage = __FILEREF__ + "getLiveStreamingInfo error" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", outputFfmpegPathFileName: " + outputFfmpegPathFileName);
		fs::remove_all(outputFfmpegPathFileName);

		throw e;
	}
}
