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
#include "spdlog/spdlog.h"

// destinationPathName will end with the new file format
void FFMpeg::changeFileFormat(
	int64_t ingestionJobKey, int64_t physicalPathKey, string sourcePhysicalPath,
	vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> &sourceVideoTracks,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> &sourceAudioTracks,

	string destinationPathName, string outputFileFormat
)
{
	string ffmpegExecuteCommand;

	_currentApiName = APIName::ChangeFileFormat;

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
		if (!fs::exists(sourcePhysicalPath))
		{
			string errorMessage = std::format(
				"Source asset path name not existing"
				", ingestionJobKey: {}"
				", sourcePhysicalPath: {}",
				ingestionJobKey, sourcePhysicalPath
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
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
				std::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "changeFileFormat", _currentIngestionJobKey, physicalPathKey, sUtcTimestamp);
				*/
			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "changeFileFormat", _currentIngestionJobKey,
				physicalPathKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);
		}

		/*
		if (outputFileFormat == "m3u8-tar.gz" || outputFileFormat == "m3u8-streaming")
		{
			ffmpegExecuteCommand =
				_ffmpegPath + "/ffmpeg "
				+ "-i " + sourcePhysicalPath + " "
			;
			for(tuple<int64_t, int, int64_t, int, int, string, string, long,
				string> videoTrack: videoTracks)
			{
				int64_t videoTrackKey;
				int trackIndex;
				int64_t durationInMilliSeconds;
				int width;
				int height;
				string avgFrameRate;
				string codecName;
				long bitRate;
				string profile;

				tie(videoTrackKey, trackIndex, durationInMilliSeconds, width, height,
					avgFrameRate, codecName, bitRate, profile) = videoTrack;

				ffmpegExecuteCommand +=
					"0:v:" + to_string(trackIndex) -c:v copy
					  -hls_time 10 -hls_playlist_type vod  -hls_segment_filename beach/360p_%03d.ts beach/360p.m3u8 \

			}
				+ "-map 0:v -c:v copy -map 0:a -c:a copy "
				//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
				+ "-q 0 "
				+ destinationPathName + " "
				+ "> " + _outputFfmpegPathFileName + " "
				+ "2>&1"
			;
		}
		else
		*/
		{
			ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg " + "-i " + sourcePhysicalPath +
								   " "
								   // -map 0:v and -map 0:a is to get all video-audio tracks
								   // 2023-09-07: ottengo un errore eseguendo questo comando su un .ts
								   //	Ho risolto il problema eliminando i due -map
								   // + "-map 0:v -c:v copy -map 0:a -c:a copy "
								   + "-c:v copy -c:a copy "
								   //  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
								   + "-q 0 " + destinationPathName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1";
		}

#ifdef __APPLE__
		ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

		SPDLOG_INFO(
			"changeFileFormat: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", physicalPathKey: {}"
			", ffmpegExecuteCommand: {}",
			ingestionJobKey, physicalPathKey, ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = std::format(
				"changeFileFormat: ffmpeg command failed"
				", executeCommandStatus: {}"
				", ffmpegExecuteCommand: {}",
				executeCommandStatus, ffmpegExecuteCommand
			);
			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "changeFileFormat: command failed";
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"changeContainer: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", physicalPathKey: {}"
			", ffmpegExecuteCommand: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, physicalPathKey, ffmpegExecuteCommand,
			chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = std::format(
			"ffmpeg: ffmpeg command failed"
			", ffmpegExecuteCommand: {}"
			", lastPartOfFfmpegOutputFile: {}"
			", e.what(): {}",
			ffmpegExecuteCommand, lastPartOfFfmpegOutputFile, e.what()
		);
		SPDLOG_ERROR(errorMessage);

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

		throw e;
	}

	SPDLOG_INFO(
		"Remove"
		", _outputFfmpegPathFileName: {}",
		_outputFfmpegPathFileName
	);
	fs::remove_all(_outputFfmpegPathFileName);
}
