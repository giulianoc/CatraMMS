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
			string errorMessage = string("Source asset path name not existing") + ", ingestionJobKey: " +
								  to_string(ingestionJobKey)
								  // + ", encodingJobKey: " + to_string(encodingJobKey)
								  + ", sourcePhysicalPath: " + sourcePhysicalPath;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			char sUtcTimestamp[64];
			tm tmUtcTimestamp;
			time_t utcTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now());

			localtime_r(&utcTimestamp, &tmUtcTimestamp);
			sprintf(
				sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d", tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);

			_outputFfmpegPathFileName =
				fmt::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "changeFileFormat", _currentIngestionJobKey, physicalPathKey, sUtcTimestamp);
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

		_logger->info(
			__FILEREF__ + "changeFileFormat: Executing ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", physicalPathKey: " + to_string(physicalPathKey) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__ + "changeFileFormat: ffmpeg command failed" +
								  ", executeCommandStatus: " + to_string(executeCommandStatus) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand;
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "changeFileFormat: command failed";
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "changeContainer: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", physicalPathKey: " + to_string(physicalPathKey) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand +
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed" + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand +
							  ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		fs::remove_all(_outputFfmpegPathFileName);

		_logger->info(__FILEREF__ + "Remove" + ", destinationPathName: " + destinationPathName);
		fs::remove_all(destinationPathName);

		throw e;
	}

	_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
	fs::remove_all(_outputFfmpegPathFileName);
}
