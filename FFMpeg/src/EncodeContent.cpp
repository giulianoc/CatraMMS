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
#include "JSONUtils.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "spdlog/spdlog.h"

void FFMpeg::encodeContent(
	string mmsSourceAssetPathName, int64_t durationInMilliSeconds, string encodedStagingAssetPathName, json encodingProfileDetailsRoot,
	bool isVideo, // if false it means is audio
	json videoTracksRoot, json audioTracksRoot, int videoTrackIndexToBeUsed, int audioTrackIndexToBeUsed, json filtersRoot, int64_t physicalPathKey,
	int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::EncodeContent;

	setStatus(ingestionJobKey, encodingJobKey, durationInMilliSeconds, mmsSourceAssetPathName, encodedStagingAssetPathName);

	try
	{
		SPDLOG_INFO(
			"Received {}"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", isVideo: {}"
			", mmsSourceAssetPathName: {}"
			", durationInMilliSeconds: {}"
			", videoTrackIndexToBeUsed: {}"
			", audioTrackIndexToBeUsed: {}",
			toString(_currentApiName), ingestionJobKey, encodingJobKey, isVideo, mmsSourceAssetPathName, durationInMilliSeconds,
			videoTrackIndexToBeUsed, audioTrackIndexToBeUsed
		);

		if (!fs::exists(mmsSourceAssetPathName))
		{
			string errorMessage = std::format(
				"Source asset path name not existing"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", mmsSourceAssetPathName: {}",
				ingestionJobKey, encodingJobKey, mmsSourceAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// if dest directory does not exist, just create it
		{
			size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
			if (endOfDirectoryIndex == string::npos)
			{
				string errorMessage = std::format(
					"encodedStagingAssetPathName is not well formed"
					", ingestionJobKey: {}"
					", encodedStagingAssetPathName: {}",
					ingestionJobKey, encodedStagingAssetPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string directory = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);
			if (!fs::exists(directory))
			{
				SPDLOG_INFO(
					"Creating directory"
					", ingestionJobKey: {}"
					", directory: {}",
					ingestionJobKey, directory
				);
				fs::create_directories(directory);
				fs::permissions(
					directory,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
						fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace
				);
			}
		}

		// _currentDurationInMilliSeconds      = durationInMilliSeconds;
		// _currentMMSSourceAssetPathName      = mmsSourceAssetPathName;
		// _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
		// _currentIngestionJobKey             = ingestionJobKey;
		// _currentEncodingJobKey              = encodingJobKey;

		_currentlyAtSecondPass = false;

		// we will set by default _twoPasses to false otherwise, since the ffmpeg class is reused
		// it could remain set to true from a previous call
		_twoPasses = false;

		FFMpegEncodingParameters ffmpegEncodingParameters(
			ingestionJobKey, encodingJobKey, encodingProfileDetailsRoot,
			isVideo, // if false it means is audio
			videoTrackIndexToBeUsed, audioTrackIndexToBeUsed, encodedStagingAssetPathName, videoTracksRoot, audioTracksRoot,

			_twoPasses, // out

			_ffmpegTempDir, _ffmpegTtfFontDir
		);

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
				std::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "encode", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
				*/
			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "encode", _currentIngestionJobKey,
				_currentEncodingJobKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);
		}

		// special case:
		//	- input is mp4 or ts
		//	- output is hls
		//	- more than 1 audio track
		//	- one video track
		// In this case we will create:
		//  - one m3u8 for each track (video and audio)
		//  - one main m3u8 having a group for AUDIO
		// string mp4Suffix = ".mp4";
		// string tsSuffix = ".ts";
		if (
			// input is mp4
			( mmsSourceAssetPathName.ends_with(".mp4")
			// (mmsSourceAssetPathName.size() >= mp4Suffix.size()
			// && 0 == mmsSourceAssetPathName.compare(mmsSourceAssetPathName.size()-mp4Suffix.size(), mp4Suffix.size(), mp4Suffix))
			||
			// input is ts
			mmsSourceAssetPathName.ends_with(".ts")
			// (mmsSourceAssetPathName.size() >= tsSuffix.size()
			// && 0 == mmsSourceAssetPathName.compare(mmsSourceAssetPathName.size()-tsSuffix.size(), tsSuffix.size(), tsSuffix))
			)

			// output is hls
			&& ffmpegEncodingParameters._httpStreamingFileFormat == "hls"

			// more than 1 audio track
			&& audioTracksRoot != nullptr && audioTracksRoot.size() > 1

			// one video track
			&& videoTracksRoot != nullptr && videoTracksRoot.size() == 1
		)
		{
			/*
			 * The command will be like this:

			ffmpeg -y -i /var/catramms/storage/MMSRepository/MMS_0000/ws2/000/228/001/1247989_source.mp4

				-map 0:1 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/ita/1247992_384637_%04d.ts -f
			hls /home/mms/tmp/ita/1247992_384637.m3u8

				-map 0:2 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/eng/1247992_384637_%04d.ts -f
			hls /home/mms/tmp/eng/1247992_384637.m3u8

				-map 0:0 -codec:v libx264 -profile:v high422 -b:v 800k -preset veryfast -level 4.0 -crf 22 -r 25 -vf scale=640:360 -threads 0
			-hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/low/1247992_384637_%04d.ts -f hls /home/mms/tmp/low/1247992_384637.m3u8

			Manifest will be like:
			#EXTM3U
			#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="ita",NAME="ita",AUTOSELECT=YES, DEFAULT=YES,URI="ita/8896718_1509416.m3u8"
			#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES, DEFAULT=YES,URI="eng/8896718_1509416.m3u8"
			#EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO="audio"
			0/8896718_1509416.m3u8


			https://developer.apple.com/documentation/http_live_streaming/example_playlists_for_http_live_streaming/adding_alternate_media_to_a_playlist#overview
			https://github.com/videojs/http-streaming/blob/master/docs/multiple-alternative-audio-tracks.md

			*/

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;

			{
				bool noErrorIfExists = true;
				bool recursive = true;
				SPDLOG_INFO(
					"Creating directory (if needed)"
					", ingestionJobKey: {}"
					", encodedStagingAssetPathName: {}",
					ingestionJobKey, encodedStagingAssetPathName
				);
				fs::create_directories(encodedStagingAssetPathName);
				fs::permissions(
					encodedStagingAssetPathName,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
						fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace
				);

				for (int index = 0; index < audioTracksRoot.size(); index++)
				{
					json audioTrack = audioTracksRoot[index];

					string audioTrackDirectoryName = JSONUtils::asString(audioTrack, "language", "");

					string audioPathName = std::format("{}/{}", encodedStagingAssetPathName, audioTrackDirectoryName);

					SPDLOG_INFO(
						"Creating directory (if needed)"
						", ingestionJobKey: {}"
						", audioPathName: {}",
						ingestionJobKey, audioPathName
					);
					fs::create_directories(audioPathName);
					fs::permissions(
						audioPathName,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
							fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace
					);
				}

				{
					string videoTrackDirectoryName;
					{
						json videoTrack = videoTracksRoot[0];

						videoTrackDirectoryName = to_string(JSONUtils::asInt(videoTrack, "trackIndex"));
					}

					string videoPathName = std::format("{}/{}", encodedStagingAssetPathName, videoTrackDirectoryName);

					SPDLOG_INFO(
						"Creating directory (if needed)"
						", ingestionJobKey: {}"
						", videoPathName: {}",
						ingestionJobKey, videoPathName
					);
					fs::create_directories(videoPathName);
					fs::permissions(
						videoPathName,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
							fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace
					);
				}
			}

			if (_twoPasses)
			{
				// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding_audioGroup(
					0, // YES two passes, first step
					ffmpegArgumentList
				);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command (first step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStream.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									   ", encodingJobKey: " + to_string(encodingJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command (first step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					SPDLOG_ERROR(errorMessage);

					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegArgumentList.clear();
				ffmpegArgumentListStream.clear();

				// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding_audioGroup(
					1, // YES two passes, second step
					ffmpegArgumentList
				);

				_currentlyAtSecondPass = true;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command (second step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStream.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command (second step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					SPDLOG_ERROR(errorMessage);

					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

				SPDLOG_INFO(
					"Remove"
					", _outputFfmpegPathFileName: {}",
					_outputFfmpegPathFileName
				);
				fs::remove_all(_outputFfmpegPathFileName);
			}
			else
			{
				// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding_audioGroup(
					-1, // NO two passes
					ffmpegArgumentList
				);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStream.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					SPDLOG_ERROR(errorMessage);

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				SPDLOG_INFO(
					"Remove"
					", _outputFfmpegPathFileName: {}",
					_outputFfmpegPathFileName
				);
				fs::remove_all(_outputFfmpegPathFileName);
			}

			long long llDirSize = -1;
			{
				llDirSize = 0;
				// recursive_directory_iterator, by default, does not follow sym links
				for (fs::directory_entry const &entry : fs::recursive_directory_iterator(encodedStagingAssetPathName))
				{
					if (entry.is_regular_file())
						llDirSize += entry.file_size();
				}
			}

			SPDLOG_INFO(
				"Encoded file generated"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodedStagingAssetPathName: {}"
				", llDirSize: {}"
				", _twoPasses: {}",
				ingestionJobKey, encodingJobKey, encodedStagingAssetPathName, llDirSize, _twoPasses
			);

			if (llDirSize == 0)
			{
				string errorMessage = std::format(
					"ffmpeg: ffmpeg command failed, encoded dir size is 0"
					", encodingJobKey: {}"
					", ingestionJobKey: {}"
					", ffmpegArgumentList: {}",
					encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str()
				);
				SPDLOG_ERROR(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "command failed, encoded dir size is 0" + ", encodingJobKey: " + to_string(encodingJobKey) +
							   ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}

			ffmpegEncodingParameters.createManifestFile_audioGroup();
		}
		else if (ffmpegEncodingParameters._httpStreamingFileFormat != "")
		{
			// hls or dash output

			vector<string> ffmpegArgumentList;

			{
				SPDLOG_INFO(
					"Creating directory (if needed)"
					", ingestionJobKey: {}"
					", encodedStagingAssetPathName: {}",
					ingestionJobKey, encodedStagingAssetPathName
				);
				fs::create_directories(encodedStagingAssetPathName);
				fs::permissions(
					encodedStagingAssetPathName,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
						fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace
				);
			}

			if (_twoPasses)
			{
				// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					0,	  // 0: YES two passes, first step
					true, // outputFileToBeAdded
					true, // videoResolutionToBeAdded
					filtersRoot,
					ffmpegArgumentList // out
				);

				ostringstream ffmpegArgumentListStreamFirstStep;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStreamFirstStep, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command (first step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStreamFirstStep.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStreamFirstStep.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command (first step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, ffmpegArgumentListStreamFirstStep.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStreamFirstStep.str(),
							lastPartOfFfmpegOutputFile, e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStreamFirstStep.str(),
							lastPartOfFfmpegOutputFile, e.what()
						);
					SPDLOG_ERROR(errorMessage);

					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegArgumentList.clear();

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					1,	  // 1: YES two passes, second step
					true, // outputFileToBeAdded
					true, // videoResolutionToBeAdded
					filtersRoot,
					ffmpegArgumentList // out
				);

				ostringstream ffmpegArgumentListStreamSecondStep;
				_currentlyAtSecondPass = true;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStreamSecondStep, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command (second step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStreamSecondStep.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStreamSecondStep.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command (second step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStreamSecondStep.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStreamSecondStep.str(),
							lastPartOfFfmpegOutputFile, e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStreamSecondStep.str(),
							lastPartOfFfmpegOutputFile, e.what()
						);
					SPDLOG_ERROR(errorMessage);

					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

				SPDLOG_INFO(
					"Remove"
					", _outputFfmpegPathFileName: {}",
					_outputFfmpegPathFileName
				);
				fs::remove_all(_outputFfmpegPathFileName);

				ffmpegEncodingParameters.createManifestFile();
			}
			else
			{
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					-1,	  // -1: NO two passes
					true, // outputFileToBeAdded
					true, // videoResolutionToBeAdded
					filtersRoot,
					ffmpegArgumentList // out
				);

				ostringstream ffmpegArgumentListStream;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStream.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					SPDLOG_ERROR(errorMessage);

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				SPDLOG_INFO(
					"Remove"
					", _outputFfmpegPathFileName: {}",
					_outputFfmpegPathFileName
				);
				fs::remove_all(_outputFfmpegPathFileName);

				ffmpegEncodingParameters.createManifestFile();
			}

			long long llDirSize = -1;
			{
				llDirSize = 0;
				// recursive_directory_iterator, by default, does not follow sym links
				for (fs::directory_entry const &entry : fs::recursive_directory_iterator(encodedStagingAssetPathName))
				{
					if (entry.is_regular_file())
						llDirSize += entry.file_size();
				}
			}

			SPDLOG_INFO(
				"Encoded file generated"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodedStagingAssetPathName: {}"
				", llDirSize: {}"
				", _twoPasses: {}",
				ingestionJobKey, encodingJobKey, encodedStagingAssetPathName, llDirSize, _twoPasses
			);

			if (llDirSize == 0)
			{
				string errorMessage = std::format(
					"ffmpeg: ffmpeg command failed, encoded dir size is 0"
					", encodingJobKey: {}"
					", ingestionJobKey: {}",
					encodingJobKey, ingestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "command failed, encoded dir size is 0" + ", encodingJobKey: " + to_string(encodingJobKey) +
							   ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}

			// changes to be done to the manifest, see EncoderThread.cpp
		}
		else
		{
			/* 2021-09-10: In case videoBitRatesInfo has more than one bitrates,
			 *	it has to be created one file for each bit rate and than
			 *	merge all in the last file with a copy command, i.e.:
			 *		- ffmpeg -i ./1.mp4 -i ./2.mp4 -c copy -map 0 -map 1 ./3.mp4
			 */

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;

			if (_twoPasses)
			{
				// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					0,	  // 1: YES two passes, first step
					true, // outputFileToBeAdded
					true, // videoResolutionToBeAdded
					filtersRoot,
					ffmpegArgumentList // out
				);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStream.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					SPDLOG_ERROR(errorMessage);

					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegArgumentList.clear();
				ffmpegArgumentListStream.clear();

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					1,	  // 1: YES two passes, second step
					true, // outputFileToBeAdded
					true, // videoResolutionToBeAdded
					filtersRoot,
					ffmpegArgumentList // out
				);

				_currentlyAtSecondPass = true;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command (second step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						string errorMessage = std::format(
							"encodeContent: ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _outputFfmpegPathFileName: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, iReturnedStatus, ffmpegArgumentListStream.str()
						);
						SPDLOG_ERROR(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed (second step)" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command (second step)"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						encodingJobKey, ingestionJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					SPDLOG_ERROR(errorMessage);

					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

				SPDLOG_INFO(
					"Remove"
					", _outputFfmpegPathFileName: {}",
					_outputFfmpegPathFileName
				);
				fs::remove_all(_outputFfmpegPathFileName);

				vector<string> sourcesPathName;
				if (ffmpegEncodingParameters.getMultiTrackPathNames(sourcesPathName))
				{
					// all the tracks generated in different files have to be copied
					// into the encodedStagingAssetPathName file
					// The command willl be:
					//		ffmpeg -i ... -i ... -c copy -map 0 -map 1 ... <dest file>

					try
					{
						muxAllFiles(ingestionJobKey, sourcesPathName, encodedStagingAssetPathName);

						ffmpegEncodingParameters.removeMultiTrackPathNames();
					}
					catch (runtime_error &e)
					{
						string errorMessage = std::format(
							"muxAllFiles failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
						SPDLOG_ERROR(errorMessage);

						ffmpegEncodingParameters.removeMultiTrackPathNames();

						throw e;
					}
				}
			}
			else
			{
				// used in case of multiple bitrate

				ffmpegArgumentList.clear();
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					-1,	  // -1: NO two passes
					true, // outputFileToBeAdded
					true, // videoResolutionToBeAdded
					filtersRoot,
					ffmpegArgumentList // out
				);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"encodeContent: Executing ffmpeg command"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}",
						ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec(
						_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
						&iReturnedStatus
					);
					*pChildPid = 0;
					if (iReturnedStatus != 0)
					{
						SPDLOG_ERROR(
							"encodeContent: executed ffmpeg command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", iReturnedStatus: {}"
							", _outputFfmpegPathFileName: {}"
							", ffmpegArgumentList: {}",
							ingestionJobKey, encodingJobKey, iReturnedStatus, _outputFfmpegPathFileName, ffmpegArgumentListStream.str()
						);

						// to hide the ffmpeg staff
						string errorMessage = std::format(
							"encodeContent command failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}",
							ingestionJobKey, encodingJobKey
						);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();
					SPDLOG_INFO(
						"encodeContent: Executed ffmpeg command"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", _outputFfmpegPathFileName: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName, ffmpegArgumentListStream.str(),
						chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed because killed by the user"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					else
						errorMessage = std::format(
							"ffmpeg: ffmpeg command failed"
							", _outputFfmpegPathFileName: {}"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", ffmpegArgumentList: {}"
							", lastPartOfFfmpegOutputFile: {}"
							", e.what(): {}",
							_outputFfmpegPathFileName, encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile,
							e.what()
						);
					SPDLOG_ERROR(errorMessage);

					SPDLOG_INFO(
						"Remove"
						", _outputFfmpegPathFileName: {}",
						_outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				SPDLOG_INFO(
					"Remove"
					", _outputFfmpegPathFileName: {}",
					_outputFfmpegPathFileName
				);
				bool exceptionInCaseOfError = false;
				fs::remove_all(_outputFfmpegPathFileName);

				vector<string> sourcesPathName;
				if (ffmpegEncodingParameters.getMultiTrackPathNames(sourcesPathName))
				{
					// all the tracks generated in different files have to be copied
					// into the encodedStagingAssetPathName file
					// The command willl be:
					//		ffmpeg -i ... -i ... -c copy -map 0 -map 1 ... <dest file>

					try
					{
						muxAllFiles(ingestionJobKey, sourcesPathName, encodedStagingAssetPathName);

						ffmpegEncodingParameters.removeMultiTrackPathNames();
					}
					catch (runtime_error &e)
					{
						string errorMessage = std::format(
							"muxAllFiles failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
						SPDLOG_ERROR(errorMessage);

						ffmpegEncodingParameters.removeMultiTrackPathNames();

						throw e;
					}
				}
			}

			long long llFileSize = -1;
			{
				llFileSize = fs::file_size(encodedStagingAssetPathName);
			}

			SPDLOG_INFO(
				"Encoded file generated"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodedStagingAssetPathName: {}"
				", llFileSize: {}"
				", _twoPasses: {}",
				ingestionJobKey, encodingJobKey, encodedStagingAssetPathName, llFileSize, _twoPasses
			);

			if (llFileSize == 0)
			{
				string errorMessage = std::format(
					"ffmpeg: ffmpeg command failed, encoded file size is 0"
					", encodingJobKey: {}"
					", ingestionJobKey: {}"
					", ffmpegArgumentList: {}",
					encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str()
				);
				SPDLOG_ERROR(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "command failed, encoded file size is 0" + ", encodingJobKey: " + to_string(encodingJobKey) +
							   ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg encode failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", physicalPathKey: {}"
			", mmsSourceAssetPathName: {}"
			", encodedStagingAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, physicalPathKey, mmsSourceAssetPathName, encodedStagingAssetPathName, e.what()
		);

		if (fs::exists(encodedStagingAssetPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", encodedStagingAssetPathName: {}",
				encodingJobKey, ingestionJobKey, encodedStagingAssetPathName
			);

			SPDLOG_INFO(
				"Remove"
				", encodedStagingAssetPathName: {}",
				encodedStagingAssetPathName
			);
			fs::remove_all(encodedStagingAssetPathName);
		}

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg encode failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", physicalPathKey: {}"
			", mmsSourceAssetPathName: {}"
			", encodedStagingAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, physicalPathKey, mmsSourceAssetPathName, encodedStagingAssetPathName, e.what()
		);

		if (fs::exists(encodedStagingAssetPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", encodedStagingAssetPathName: {}",
				encodingJobKey, ingestionJobKey, encodedStagingAssetPathName
			);

			SPDLOG_INFO(
				"Remove"
				", encodedStagingAssetPathName: {}",
				encodedStagingAssetPathName
			);
			fs::remove_all(encodedStagingAssetPathName);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg encode failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", physicalPathKey: {}"
			", mmsSourceAssetPathName: {}"
			", encodedStagingAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, physicalPathKey, mmsSourceAssetPathName, encodedStagingAssetPathName, e.what()
		);

		if (fs::exists(encodedStagingAssetPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", encodedStagingAssetPathName: {}",
				encodingJobKey, ingestionJobKey, encodedStagingAssetPathName
			);

			SPDLOG_INFO(
				"Remove"
				", encodedStagingAssetPathName: {}",
				encodedStagingAssetPathName
			);
			fs::remove_all(encodedStagingAssetPathName);
		}

		throw e;
	}
}
