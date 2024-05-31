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
#include "catralibraries/ProcessUtility.h"

void FFMpeg::streamingToFile(int64_t ingestionJobKey, bool regenerateTimestamps, string sourceReferenceURL, string destinationPathName)
{
	string ffmpegExecuteCommand;

	_currentApiName = APIName::StreamingToFile;

	setStatus(ingestionJobKey
			  /*
			  encodingJobKey
			  videoDurationInMilliSeconds,
			  mmsAssetPathName
			  stagingEncodedAssetPathName
			  */
	);

	try
	{
		{
			char sUtcTimestamp[64];
			tm tmUtcTimestamp;
			time_t utcTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now());

			localtime_r(&utcTimestamp, &tmUtcTimestamp);
			sprintf(
				sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d", tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);

			_outputFfmpegPathFileName = fmt::format("{}/{}_{}_{}.log", _ffmpegTempDir, "streamingToFile", _currentIngestionJobKey, sUtcTimestamp);
		}

		// 2022-06-06: cosa succede se sourceReferenceURL rappresenta un live?
		//		- destinationPathName diventerà enorme!!!
		//	Per questo motivo ho inserito un timeout di X hours
		int maxStreamingHours = 5;
		ffmpegExecuteCommand = fmt::format(
			"{}/ffmpeg {} -y -i \"{}\" -t {} "
			// -map 0:v and -map 0:a is to get all video-audio tracks
			"-map 0:v -c:v copy -map 0:a -c:a copy "
			//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
			"-q 0 {} > {} 2>&1",
			_ffmpegPath, (regenerateTimestamps ? "-fflags +genpts" : ""), sourceReferenceURL, maxStreamingHours * 3600, destinationPathName,
			_outputFfmpegPathFileName
		);

#ifdef __APPLE__
		ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

		SPDLOG_INFO(
			"streamingToFile: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}",
			ingestionJobKey, ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			SPDLOG_ERROR(
				"streamingToFile: ffmpeg command failed"
				", executeCommandStatus: {}"
				", ffmpegExecuteCommand: {}",
				executeCommandStatus, ffmpegExecuteCommand
			);

			// 2021-12-13: sometimes we have one track creating problems and the command fails
			// in this case it is enought to avoid to copy all the tracks, leave ffmpeg
			// to choice the track and it works
			{
				ffmpegExecuteCommand = fmt::format(
					"{}/ffmpeg {} -y -i \"{}\" -t {} "
					"-c:v copy -c:a copy "
					//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
					"-q 0 {} > {} 2>&1",
					_ffmpegPath, (regenerateTimestamps ? "-fflags +genpts" : ""), sourceReferenceURL, maxStreamingHours * 3600, destinationPathName,
					_outputFfmpegPathFileName
				);

				SPDLOG_INFO(
					"streamingToFile: Executing ffmpeg command"
					", ingestionJobKey: {}"
					", ffmpegExecuteCommand: {}",
					ingestionJobKey, ffmpegExecuteCommand
				);

				chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
				if (executeCommandStatus != 0)
				{
					SPDLOG_ERROR(
						"streamingToFile: ffmpeg command failed"
						", executeCommandStatus: {}"
						", ffmpegExecuteCommand: {}",
						executeCommandStatus, ffmpegExecuteCommand
					);

					// to hide the ffmpeg staff
					string errorMessage = "streamingToFile: command failed";
					throw runtime_error(errorMessage);
				}
			}
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"streamingToFile: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, ffmpegExecuteCommand, chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg command failed"
			", ffmpegExecuteCommand: {}"
			", lastPartOfFfmpegOutputFile: {}"
			", e.what(): {}",
			ffmpegExecuteCommand, lastPartOfFfmpegOutputFile, e.what()
		);

		SPDLOG_INFO(
			"Remove"
			", _outputFfmpegPathFileName: {}",
			_outputFfmpegPathFileName
		);
		fs::remove_all(_outputFfmpegPathFileName);

		SPDLOG_INFO(
			"Remove"
			", destinationPathName: {}",
			destinationPathName
		);
		fs::remove_all(destinationPathName);

		/*
ffmpeg version 7.0 Copyright (c) 2000-2024 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
  configuration: --prefix=/opt/catrasoftware/deploy/ffmpeg --enable-indev=alsa --enable-outdev=alsa --enable-libfreetype --enable-libharfbuzz
--enable-libfontconfig --enable-libfribidi --enable-libmp3lame --enable-libopus --enable-libx265 --enable-libaom --enable-gpl --enable-libxml2
--enable-libfdk-aac --enable-libvpx --enable-libx264 --enable-nonfree --enable-shared --enable-libopencore-amrnb --enable-libopencore-amrwb
--enable-libvorbis --enable-libxvid --enable-version3 --enable-openssl --enable-libsrt --enable-ffplay libavutil      59.  8.100 / 59.  8.100
  libavcodec     61.  3.100 / 61.  3.100
  libavformat    61.  1.100 / 61.  1.100
  libavdevice    61.  1.100 / 61.  1.100
  libavfilter    10.  1.100 / 10.  1.100
  libswscale      8.  1.100 /  8.  1.100
  libswresample   5.  1.100 /  5.  1.100
  libpostproc    58.  1.100 / 58.  1.100
[https @ 0x55cb355cc4c0] HTTP error 410 Gone
[in#0 @ 0x55cb355cb780] Error opening input: Server returned 4XX Client Error, but not one of 40{0,1,3,4}
Error opening input file
https://player.vimeo.com/progressive_redirect/download/830622686/container/24656461-f99e-4335-9c99-2a1806c2d7bc/2879f8e3-588c0b73/episodio_%232_scotto%20%281080p%29.mp4?expires=1716885089&loc=external&signature=3d8bb844b1b2f124925067a3f717edf4be86b7a060511a8042763409b8e8f05b.
Error opening input files: Server returned 4XX Client Error, but not one of 40{0,1,3,4}
		 */
		size_t pos = lastPartOfFfmpegOutputFile.rfind("Error opening input files: Server returned 4XX");
		if (pos != string::npos)
			throw runtime_error(fmt::format("streamingToFile: {}", lastPartOfFfmpegOutputFile.substr(pos)));
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
