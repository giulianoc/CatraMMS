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
/*
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "catralibraries/StringUtils.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
*/

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

		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg ";
		if (regenerateTimestamps)
			ffmpegExecuteCommand += "-fflags +genpts ";
		// 2022-06-06: cosa succede se sourceReferenceURL rappresenta un live?
		//		- destinationPathName diventerà enorme!!!
		//	Per questo motivo ho inserito un timeout di X hours
		int maxStreamingHours = 5;
		ffmpegExecuteCommand += string("-y -i \"" + sourceReferenceURL + "\" ") + "-t " + to_string(maxStreamingHours * 3600) +
								" "
								// -map 0:v and -map 0:a is to get all video-audio tracks
								+ "-map 0:v -c:v copy -map 0:a -c:a copy "
								//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
								+ "-q 0 " + destinationPathName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1";

#ifdef __APPLE__
		ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

		_logger->info(
			__FILEREF__ + "streamingToFile: Executing ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffmpegExecuteCommand: " + ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__ + "streamingToFile: ffmpeg command failed" +
								  ", executeCommandStatus: " + to_string(executeCommandStatus) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand;
			_logger->error(errorMessage);

			// 2021-12-13: sometimes we have one track creating problems and the command fails
			// in this case it is enought to avoid to copy all the tracks, leave ffmpeg
			// to choice the track and it works
			{
				ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg ";
				if (regenerateTimestamps)
					ffmpegExecuteCommand += "-fflags +genpts ";
				ffmpegExecuteCommand += string("-y -i \"" + sourceReferenceURL + "\" ") +
										"-c:v copy -c:a copy "
										//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
										+ "-q 0 " + destinationPathName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1";

				_logger->info(
					__FILEREF__ + "streamingToFile: Executing ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", ffmpegExecuteCommand: " + ffmpegExecuteCommand
				);

				chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
				if (executeCommandStatus != 0)
				{
					string errorMessage = __FILEREF__ + "streamingToFile: ffmpeg command failed" +
										  ", executeCommandStatus: " + to_string(executeCommandStatus) +
										  ", ffmpegExecuteCommand: " + ffmpegExecuteCommand;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "streamingToFile: command failed";
					throw runtime_error(errorMessage);
				}
			}
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "streamingToFile: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffmpegExecuteCommand: " + ffmpegExecuteCommand + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" +
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
