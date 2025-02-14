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
#include "catralibraries/ProcessUtility.h"
#include "spdlog/spdlog.h"
#include <fstream>

void FFMpeg::muxAllFiles(int64_t ingestionJobKey, vector<string> sourcesPathName, string destinationPathName)
{
	_currentApiName = APIName::MuxAllFiles;

	SPDLOG_INFO(
		"{}"
		", ingestionJobKey: {}"
		", destinationPathName: {}",
		toString(_currentApiName), ingestionJobKey, destinationPathName
	);

	for (string sourcePathName : sourcesPathName)
	{
		// milli secs to wait in case of nfs delay
		bool exists = false;
		{
			chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
			do
			{
				if (fs::exists(sourcePathName))
				{
					exists = true;
					break;
				}

				this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
			} while (chrono::system_clock::now() < end);
		}
		if (!exists)
		{
			string errorMessage = string("Source asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourcePathName: " + sourcePathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	string ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg ";
	for (string sourcePathName : sourcesPathName)
		ffmpegExecuteCommand += "-i " + sourcePathName + " ";
	ffmpegExecuteCommand += "-c copy ";
	for (int sourceIndex = 0; sourceIndex < sourcesPathName.size(); sourceIndex++)
		ffmpegExecuteCommand += "-map " + to_string(sourceIndex) + " ";
	ffmpegExecuteCommand += destinationPathName;

	try
	{
		SPDLOG_INFO(
			"{}: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}",
			toString(_currentApiName), ingestionJobKey, ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__ + toString(_currentApiName) + ": ffmpeg command failed" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", executeCommandStatus: " + to_string(executeCommandStatus) +
								  ", ffmpegExecuteCommand: " + ffmpegExecuteCommand;
			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + toString(_currentApiName) + ": command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey);
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"{}: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}"
			", @FFMPEG statistics@ - duration (secs): @{}@",
			toString(_currentApiName), ingestionJobKey, ffmpegExecuteCommand,
			chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);
	}
	catch (runtime_error &e)
	{
		string errorMessage = __FILEREF__ + "ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", ffmpegExecuteCommand: " + ffmpegExecuteCommand + ", e.what(): " + e.what();
		SPDLOG_ERROR(errorMessage);

		// to hide the ffmpeg staff
		errorMessage = __FILEREF__ + "command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
		throw e;
	}
}

void FFMpeg::concat(int64_t ingestionJobKey, bool isVideo, vector<string> &sourcePhysicalPaths, string concatenatedMediaPathName)
{
	_currentApiName = APIName::Concat;

	setStatus(ingestionJobKey
			  /*
			  encodingJobKey
			  videoDurationInMilliSeconds,
			  mmsAssetPathName
			  stagingEncodedAssetPathName
			  */
	);

	string concatenationListPathName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + ".concatList.txt";

	ofstream concatListFile(concatenationListPathName.c_str(), ofstream::trunc);
	for (string sourcePhysicalPath : sourcePhysicalPaths)
	{
		SPDLOG_INFO(
			"ffmpeg: adding physical path"
			", ingestionJobKey: {}"
			", sourcePhysicalPath: {}",
			ingestionJobKey, sourcePhysicalPath
		);

		if (!fs::exists(sourcePhysicalPath))
		{
			string errorMessage = string("Source asset path name not existing") + ", ingestionJobKey: " +
								  to_string(ingestionJobKey)
								  // + ", encodingJobKey: " + to_string(encodingJobKey)
								  + ", sourcePhysicalPath: " + sourcePhysicalPath;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		concatListFile << "file '" << sourcePhysicalPath << "'" << endl;
	}
	concatListFile.close();

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

		_outputFfmpegPathFileName = std::format("{}/{}_{}_{}.log", _ffmpegTempDir, "concat", _currentIngestionJobKey, sUtcTimestamp);
		*/
		_outputFfmpegPathFileName = std::format(
			"{}/{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "concat", _currentIngestionJobKey,
			tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec
		);
	}

	// Then you can stream copy or re-encode your files
	// The -safe 0 above is not required if the paths are relative
	// ffmpeg -f concat -safe 0 -i mylist.txt -c copy output
	// 2019-10-10: added -fflags +genpts -async 1 for lipsync issue!!!
	// 2019-10-11: removed -fflags +genpts -async 1 because does not have inpact on lipsync issue!!!
	string ffmpegExecuteCommand;
	if (isVideo)
	{
		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg " + "-f concat -safe 0 -i " + concatenationListPathName + " ";
		bool allVideoAudioTracks = true;
		if (allVideoAudioTracks)
			ffmpegExecuteCommand += "-map 0:v -c:v copy -map 0:a -c:a copy ";
		else
			ffmpegExecuteCommand += "-c copy ";
		ffmpegExecuteCommand += (concatenatedMediaPathName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1");
	}
	else
	{
		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg " + "-f concat -safe 0 -i " + concatenationListPathName + " ";
		bool allVideoAudioTracks = true;
		if (allVideoAudioTracks)
			ffmpegExecuteCommand += "-map 0:a -c:a copy ";
		else
			ffmpegExecuteCommand += "-c copy ";
		ffmpegExecuteCommand += (concatenatedMediaPathName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1");
	}

#ifdef __APPLE__
	ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

	try
	{
		SPDLOG_INFO(
			"concat: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}",
			ingestionJobKey, ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			// 2020-07-20: added the log of the input file
			string inputBuffer;
			{
				ifstream inputFile(concatenationListPathName);
				stringstream input;
				input << inputFile.rdbuf();

				inputBuffer = input.str();
			}
			string errorMessage = __FILEREF__ + "concat: ffmpeg command failed" + ", executeCommandStatus: " + to_string(executeCommandStatus) +
								  ", ffmpegExecuteCommand: " + ffmpegExecuteCommand + ", inputBuffer: " + inputBuffer;

			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "concat: command failed" + ", inputBuffer: " + inputBuffer;
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"concat: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, ffmpegExecuteCommand, chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		// 2020-07-20: log of ffmpegExecuteCommand commented because already added into the catched exception
		string errorMessage = __FILEREF__ +
							  "ffmpeg: ffmpeg command failed"
							  // + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
							  + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
		SPDLOG_ERROR(errorMessage);

		SPDLOG_INFO(
			"Remove"
			", concatenationListPathName: {}",
			concatenationListPathName
		);
		fs::remove_all(concatenationListPathName);

		SPDLOG_INFO(
			"Remove"
			", _outputFfmpegPathFileName: {}",
			_outputFfmpegPathFileName
		);
		fs::remove_all(_outputFfmpegPathFileName);

		// to hide the ffmpeg staff
		errorMessage = __FILEREF__ + "command failed" + ", e.what(): " + e.what();
		throw runtime_error(errorMessage);
	}

	bool exceptionInCaseOfError = false;
	SPDLOG_INFO(
		"Remove"
		", concatenationListPathName: {}",
		concatenationListPathName
	);
	fs::remove_all(concatenationListPathName);
	SPDLOG_INFO(
		"Remove"
		", _outputFfmpegPathFileName: {}",
		_outputFfmpegPathFileName
	);
	fs::remove_all(_outputFfmpegPathFileName);
}

void FFMpeg::splitVideoInChunks(
	int64_t ingestionJobKey, string sourcePhysicalPath, long chunksDurationInSeconds, string chunksDirectory, string chunkBaseFileName
)
{
	_currentApiName = APIName::SplitVideoInChunks;

	setStatus(ingestionJobKey
			  /*
			  encodingJobKey
			  videoDurationInMilliSeconds,
			  mmsAssetPathName
			  stagingEncodedAssetPathName
			  */
	);

	SPDLOG_INFO(
		"Received {}"
		", ingestionJobKey: {}"
		", sourcePhysicalPath: {}"
		", chunksDurationInSeconds: {}"
		", chunksDirectory: {}"
		", chunkBaseFileName: {}",
		toString(_currentApiName), ingestionJobKey, sourcePhysicalPath, chunksDurationInSeconds, chunksDirectory, chunkBaseFileName
	);

	if (!fs::exists(sourcePhysicalPath))
	{
		string errorMessage = string("Source asset path name not existing") + ", ingestionJobKey: " +
							  to_string(ingestionJobKey)
							  // + ", encodingJobKey: " + to_string(encodingJobKey)
							  + ", sourcePhysicalPath: " + sourcePhysicalPath;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	// if dest directory does not exist, just create it
	{
		if (!fs::exists(chunksDirectory))
		{
			SPDLOG_INFO(
				"Creating directory"
				", ingestionJobKey: {}"
				", chunksDirectory: {}",
				ingestionJobKey, chunksDirectory
			);
			fs::create_directories(chunksDirectory);
			fs::permissions(
				chunksDirectory,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
					fs::perms::others_read | fs::perms::others_exec,
				fs::perm_options::replace
			);
		}
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

		_outputFfmpegPathFileName = std::format("{}/{}_{}_{}.log", _ffmpegTempDir, "splitVideoInChunks", _currentIngestionJobKey, sUtcTimestamp);
		*/
		_outputFfmpegPathFileName = std::format(
			"{}/{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "splitVideoInChunks", _currentIngestionJobKey,
			tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec
		);
	}

	string outputPathFileName;
	{
		size_t beginOfFileFormatIndex = sourcePhysicalPath.find_last_of(".");
		if (beginOfFileFormatIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "sourcePhysicalPath is not well formed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourcePhysicalPath: " + sourcePhysicalPath;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		outputPathFileName = chunksDirectory;
		if (chunksDirectory.back() != '/')
			outputPathFileName += "/";
		outputPathFileName += (chunkBaseFileName + "_%04d" + sourcePhysicalPath.substr(beginOfFileFormatIndex));
	}

	// This operation is very quick
	// -reset_timestamps: Reset timestamps at the beginning of each segment, so that each segment
	//		will start with near-zero timestamps.
	//		Rather than splitting based on a particular time, it splits on the nearest keyframe
	//		following the requested time, so each new segment always starts with a keyframe.
	string ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg " + "-i " + sourcePhysicalPath + " " + "-c copy -map 0 -segment_time " +
								  secondsToTime(ingestionJobKey, chunksDurationInSeconds) + " " + "-f segment -reset_timestamps 1 " +
								  outputPathFileName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1";

	try
	{
		SPDLOG_INFO(
			"splitVideoInChunks: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}",
			ingestionJobKey, ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__ + "splitVideoInChunks: ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", executeCommandStatus: " + to_string(executeCommandStatus) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand;
			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "splitVideoInChunks: command failed";
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"splitVideoInChunks: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, ffmpegExecuteCommand, chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", ffmpegExecuteCommand: " + ffmpegExecuteCommand + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile +
							  ", e.what(): " + e.what();
		SPDLOG_ERROR(errorMessage);

		SPDLOG_INFO(
			"Remove"
			", _outputFfmpegPathFileName: {}",
			_outputFfmpegPathFileName
		);
		fs::remove_all(_outputFfmpegPathFileName);

		throw e;
	}

	SPDLOG_INFO(
		"Remove"
		", _outputFfmpegPathFileName: {}",
		_outputFfmpegPathFileName
	);
	fs::remove_all(_outputFfmpegPathFileName);
}

// audio and video
void FFMpeg::cutWithoutEncoding(
	int64_t ingestionJobKey, string sourcePhysicalPath, bool isVideo,
	string cutType, // KeyFrameSeeking, FrameAccurateWithoutEncoding, KeyFrameSeekingInterval
	string startKeyFrameSeekingInterval, string endKeyFrameSeekingInterval,
	string startTime, // [-][HH:]MM:SS[.m...] or [-]S+[.m...] or HH:MM:SS:FF
	string endTime,	  // [-][HH:]MM:SS[.m...] or [-]S+[.m...] or HH:MM:SS:FF
	int framesNumber, string cutMediaPathName
)
{

	_currentApiName = APIName::CutWithoutEncoding;

	setStatus(ingestionJobKey
			  /*
			  encodingJobKey
			  videoDurationInMilliSeconds,
			  mmsAssetPathName
			  stagingEncodedAssetPathName
			  */
	);

	SPDLOG_INFO(
		"Received cutWithoutEncoding"
		", ingestionJobKey: {}"
		", sourcePhysicalPath: {}"
		", isVideo: {}"
		", cutType: {}"
		", startKeyFrameSeekingInterval: {}"
		", endKeyFrameSeekingInterval: {}"
		", startTime: {}"
		", endTime: {}"
		", framesNumber: {}"
		", cutMediaPathName: {}",
		ingestionJobKey, sourcePhysicalPath, isVideo, cutType, startKeyFrameSeekingInterval, endKeyFrameSeekingInterval, startTime, endTime,
		framesNumber, cutMediaPathName
	);

	if (!fs::exists(sourcePhysicalPath))
	{
		string errorMessage = string("Source asset path name not existing") + ", ingestionJobKey: " +
							  to_string(ingestionJobKey)
							  // + ", encodingJobKey: " + to_string(encodingJobKey)
							  + ", sourcePhysicalPath: " + sourcePhysicalPath;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	// if dest directory does not exist, just create it
	{
		size_t endOfDirectoryIndex = cutMediaPathName.find_last_of("/");
		if (endOfDirectoryIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "cutMediaPathName is not well formed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", cutMediaPathName: " + cutMediaPathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string cutMediaDirectory = cutMediaPathName.substr(0, endOfDirectoryIndex);
		if (!fs::exists(cutMediaDirectory))
		{
			SPDLOG_INFO(
				"Creating directory"
				", ingestionJobKey: {}"
				", cutMediaDirectory: {}",
				ingestionJobKey, cutMediaDirectory
			);
			fs::create_directories(cutMediaDirectory);
			fs::permissions(
				cutMediaDirectory,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
					fs::perms::others_read | fs::perms::others_exec,
				fs::perm_options::replace
			);
		}
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

		_outputFfmpegPathFileName = std::format("{}/{}_{}_{}.log", _ffmpegTempDir, "cutWithoutEncoding", _currentIngestionJobKey, sUtcTimestamp);
		*/
		_outputFfmpegPathFileName = std::format(
			"{}/{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "cutWithoutEncoding", _currentIngestionJobKey,
			tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec
		);
	}

	/*
		-ss: When used as an output option (before an output url), decodes but discards input
			until the timestamps reach position.
			Format: HH:MM:SS.xxx (xxx are decimals of seconds) or in seconds (sec.decimals)
		2019-09-24: Added -async 1 option because the Escenic transcoder (ffmpeg) was failing
			The generated error was: Too many packets buffered for output stream
			(look https://trac.ffmpeg.org/ticket/6375)
			-async samples_per_second
				Audio sync method. "Stretches/squeezes" the audio stream to match the timestamps, the parameter is
				the maximum samples per second by which the audio is changed.  -async 1 is a special case where only
				the start of the audio stream is corrected without any later correction.
			-af "aresample=async=1:min_hard_comp=0.100000:first_pts=0" helps to keep your audio lined up
				with the beginning of your video. It is common for a container to have the beginning
				of the video and the beginning of the audio start at different points. By using this your container
				should have little to no audio drift or offset as it will pad the audio with silence or trim audio
				with negative PTS timestamps if the audio does not actually start at the beginning of the video.
		2019-09-26: introduced the concept of 'Key-Frame Seeking' vs 'All-Frame Seeking' vs 'Full Re-Encoding'
			(see http://www.markbuckler.com/post/cutting-ffmpeg/)
	*/
	string ffmpegExecuteCommand;
	if (isVideo)
	{
		// Putting the -ss parameter before the -i parameter is called input seeking and
		// is very fast because FFmpeg jumps from I-frame to I-frame to reach the seek-point.
		// Since the seeking operation jumps between I-frames, it is not going to accurately stop
		// on the frame (or time) that you requested. It will search for the nearest I-frame
		// and start the copy operation from that point.
		//
		// If we insert the -ss parameter after the -i parameter, it is called output seeking.
		// Here is a problem. In video compression, you have I-frames that are indepently encoded
		// and you have predicted frames (P, B) that depend on other frames for decoding.
		// If the start time that you specified falls on a Predicted Frame, the copy operation
		// will start with that frame (call it X). It is possible that the frames that “X”
		// requires in order to be decoded are missing in the output! Consequently,
		// it is possible that the output video will not start smoothly and
		// might have some stutter, or black video until the first I-frame is reached.
		// We are not using this option.

		/* Example:
			Original video
				Length of 1:00:00
				Has a key frame every 10s
			Desired cut:
				From 0:01:35 through till the end
			Attempt #1:
				Using -ss 0:01:35 -i blah.mp4 -vcodec copy, what results is a file where:
				audio starts at 0:01:30
				video also starts at 0:01:30
				this starts both the audio and the video too early
			using -i blah.mp4 -ss 0:01:35 -vcodec copy, what results is a file where:
				audio starts at 0:01:35,
				but the video is blank/ black for the first 5 seconds,
				until 0:01:40, when the video starts
				this starts the audio on time, but the video starts too late

			List timestamps of key frames:
				ffprobe -v error -select_streams v:0 -skip_frame nokey -show_entries frame=pkt_pts_time -of csv=p=0 input.mp4
			(https://stackoverflow.com/questions/63548027/cut-a-video-in-between-key-frames-without-re-encoding-the-full-video-using-ffpme)
		*/

		string startTimeToBeUsed;
		{
			if (cutType == "KeyFrameSeekingInterval")
			{
				startTimeToBeUsed = getNearestKeyFrameTime(
					ingestionJobKey, sourcePhysicalPath, startKeyFrameSeekingInterval, timeToSeconds(ingestionJobKey, startTime).first
				);
				if (startTimeToBeUsed == "")
					startTimeToBeUsed = startTime;
			}
			else
				startTimeToBeUsed = startTime;
		}

		string endTimeToBeUsed;
		{
			if (cutType == "KeyFrameSeekingInterval")
			{
				endTimeToBeUsed = getNearestKeyFrameTime(
					ingestionJobKey, sourcePhysicalPath, endKeyFrameSeekingInterval, timeToSeconds(ingestionJobKey, endTime).first
				);
				if (endTimeToBeUsed == "")
					endTimeToBeUsed = endTime;
			}
			else if (cutType == "KeyFrameSeeking") // input seeking
			{
				// if you specify -ss before -i, -to will have the same effect as -t, i.e. it will act as a duration.
				endTimeToBeUsed = to_string(timeToSeconds(ingestionJobKey, endTime).first - timeToSeconds(ingestionJobKey, startTime).first);
			}
			else
			{
				endTimeToBeUsed = endTime;
			}
		}

		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg ";

		{
			if (cutType == "KeyFrameSeeking") // input seeking
			{
				// input seeking: beginning of the generated video will be to the nearest keyframe
				// found before your specified timestamp
				// KeyFrameSeeking è impreciso (perchè utilizza il keyframe) ma veloce

				ffmpegExecuteCommand += (string("-ss ") + startTimeToBeUsed + " " + "-i " + sourcePhysicalPath + " ");
			}
			else // if (cutType == "FrameAccurateWithoutEncoding") output seeking or "KeyFrameSeekingInterval"
			{
				// FrameAccurateWithoutEncoding: it means it is used any frame even if it is not a key frame
				// FrameAccurateWithoutEncoding è lento (perchè NON utilizza il keyframe) ma preciso (pechè utilizza il frame indicato)
				ffmpegExecuteCommand += (string("-i ") + sourcePhysicalPath + " " + "-ss " + startTimeToBeUsed + " ");
			}
		}

		if (cutType != "KeyFrameSeekingInterval" && framesNumber != -1)
			ffmpegExecuteCommand += (string("-vframes ") + to_string(framesNumber) + " ");
		else
			ffmpegExecuteCommand += (string("-to ") + endTimeToBeUsed + " ");

		ffmpegExecuteCommand += (
			// string("-async 1 ")
			// commented because aresample filtering requires encoding and here we are just streamcopy
			// + "-af \"aresample=async=1:min_hard_comp=0.100000:first_pts=0\" "
			// -map 0:v and -map 0:a is to get all video-audio tracks
			"-map 0:v -c:v copy -map 0:a -c:a copy " + cutMediaPathName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1"
		);
	}
	else
	{
		// audio

		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg " + "-ss " + startTime + " " + "-i " + sourcePhysicalPath + " " + "-to " + endTime +
							   " "
							   // + "-async 1 "
							   // commented because aresample filtering requires encoding and here we are just streamcopy
							   // + "-af \"aresample=async=1:min_hard_comp=0.100000:first_pts=0\" "
							   // -map 0:v and -map 0:a is to get all video-audio tracks
							   + "-map 0:a -c:a copy " + cutMediaPathName + " " + "> " + _outputFfmpegPathFileName + " " + "2>&1";
	}

#ifdef __APPLE__
	ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

	try
	{
		SPDLOG_INFO(
			"cut: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}",
			ingestionJobKey, ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__ + "cut: ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", executeCommandStatus: " + to_string(executeCommandStatus) + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand;
			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "cut: command failed";
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"cut: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, ffmpegExecuteCommand, chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
		);
	}
	catch (runtime_error &e)
	{
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", ffmpegExecuteCommand: " + ffmpegExecuteCommand + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile +
							  ", e.what(): " + e.what();
		SPDLOG_ERROR(errorMessage);

		SPDLOG_INFO(
			"Remove"
			", _outputFfmpegPathFileName: {}",
			_outputFfmpegPathFileName
		);
		fs::remove_all(_outputFfmpegPathFileName);

		throw e;
	}

	SPDLOG_INFO(
		"Remove"
		", _outputFfmpegPathFileName: {}",
		_outputFfmpegPathFileName
	);
	fs::remove_all(_outputFfmpegPathFileName);
}

// only video
void FFMpeg::cutFrameAccurateWithEncoding(
	int64_t ingestionJobKey, string sourceVideoAssetPathName,
	// no keyFrameSeeking needs reencoding otherwise the key frame is always used
	// If you re-encode your video when you cut/trim, then you get a frame-accurate cut
	// because FFmpeg will re-encode the video and start with an I-frame.
	// There is an option to encode only a little part of the video,
	// see https://stackoverflow.com/questions/63548027/cut-a-video-in-between-key-frames-without-re-encoding-the-full-video-using-ffpme
	int64_t encodingJobKey, json encodingProfileDetailsRoot, string startTime, string endTime, int framesNumber, string stagingEncodedAssetPathName,
	pid_t *pChildPid
)
{

	_currentApiName = APIName::CutFrameAccurateWithEncoding;

	setStatus(
		ingestionJobKey, encodingJobKey,
		framesNumber == -1 ? ((timeToSeconds(ingestionJobKey, endTime).first - timeToSeconds(ingestionJobKey, startTime).first) * 1000) : -1,
		sourceVideoAssetPathName, stagingEncodedAssetPathName
	);

	SPDLOG_INFO(
		"Received cutFrameAccurateWithEncoding"
		", ingestionJobKey: {}"
		", sourceVideoAssetPathName: {}"
		", encodingJobKey: {}"
		", startTime: {}"
		", endTime: {}"
		", framesNumber: {}"
		", stagingEncodedAssetPathName: {}",
		ingestionJobKey, sourceVideoAssetPathName, encodingJobKey, startTime, endTime, framesNumber, stagingEncodedAssetPathName
	);

	try
	{
		if (!fs::exists(sourceVideoAssetPathName))
		{
			string errorMessage = string("Source asset path name not existing") + ", ingestionJobKey: " +
								  to_string(ingestionJobKey)
								  // + ", encodingJobKey: " + to_string(encodingJobKey)
								  + ", sourceVideoAssetPathName: " + sourceVideoAssetPathName;
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
				std::format("{}/{}_{}_{}.log", _ffmpegTempDir, "cutFrameAccurateWithEncoding", _currentIngestionJobKey, sUtcTimestamp);
				*/
			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "cutFrameAccurateWithEncoding", _currentIngestionJobKey,
				tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec
			);
		}

		/*
			-ss: When used as an output option (before an output url), decodes but discards input
				until the timestamps reach position.
				Format: HH:MM:SS.xxx (xxx are decimals of seconds) or in seconds (sec.decimals)
			2019-09-24: Added -async 1 option because the Escenic transcoder (ffmpeg) was failing
				The generated error was: Too many packets buffered for output stream
				(look https://trac.ffmpeg.org/ticket/6375)
				-async samples_per_second
					Audio sync method. "Stretches/squeezes" the audio stream to match the timestamps, the parameter is
					the maximum samples per second by which the audio is changed.  -async 1 is a special case where only
					the start of the audio stream is corrected without any later correction.
				-af "aresample=async=1:min_hard_comp=0.100000:first_pts=0" helps to keep your audio lined up
					with the beginning of your video. It is common for a container to have the beginning
					of the video and the beginning of the audio start at different points. By using this your container
					should have little to no audio drift or offset as it will pad the audio with silence or trim audio
					with negative PTS timestamps if the audio does not actually start at the beginning of the video.
			2019-09-26: introduced the concept of 'Key-Frame Seeking' vs 'All-Frame Seeking' vs 'Full Re-Encoding'
				(see http://www.markbuckler.com/post/cutting-ffmpeg/)
		*/

		if (encodingProfileDetailsRoot == nullptr)
		{
			string errorMessage = __FILEREF__ + "encodingProfileDetailsRoot is mandatory" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegEncodingProfileArgumentList;
		{
			try
			{
				string httpStreamingFileFormat;
				string ffmpegHttpStreamingParameter = "";
				bool encodingProfileIsVideo = true;

				string ffmpegFileFormatParameter = "";

				string ffmpegVideoCodecParameter = "";
				string ffmpegVideoProfileParameter = "";
				string ffmpegVideoResolutionParameter = "";
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
					_logger, encodingProfileDetailsRoot, encodingProfileIsVideo,

					httpStreamingFileFormat, ffmpegHttpStreamingParameter,

					ffmpegFileFormatParameter,

					ffmpegVideoCodecParameter, ffmpegVideoProfileParameter, ffmpegVideoOtherParameters, twoPasses, ffmpegVideoFrameRateParameter,
					ffmpegVideoKeyFramesRateParameter, videoBitRatesInfo,

					ffmpegAudioCodecParameter, ffmpegAudioOtherParameters, ffmpegAudioChannelsParameter, ffmpegAudioSampleRateParameter,
					audioBitRatesInfo
				);

				tuple<string, int, int, int, string, string, string> videoBitRateInfo = videoBitRatesInfo[0];
				tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore, ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
					ffmpegVideoBufSizeParameter) = videoBitRateInfo;

				ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

				/*
				if (httpStreamingFileFormat != "")
				{
					// let's make it simple, in case of cut we do not use http streaming output
					string errorMessage = __FILEREF__ + "in case of cut we are not using an httpStreaming encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				else */
				if (twoPasses)
				{
					// let's make it simple, in case of cut we do not use twoPasses
					/*
					string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", twoPasses: " + to_string(twoPasses)
					;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					string errorMessage = std::format(
						"in case of cut we are not using two passes encoding. Changed it to false"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", twoPasses: {}",
						ingestionJobKey, encodingJobKey, twoPasses
					);
					SPDLOG_WARN(errorMessage);
				}

				FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegEncodingProfileArgumentList);
				// we cannot have two video filters parameters (-vf), one is for the overlay.
				// If it is needed we have to combine both using the same -vf parameter and using the
				// comma (,) as separator. For now we will just comment it and the resolution will be the one
				// coming from the video (no changes)
				// FFMpegEncodingParameters::addToArguments(ffmpegVideoResolutionParameter, ffmpegEncodingProfileArgumentList);
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

		vector<string> ffmpegArgumentList;
		ostringstream ffmpegArgumentListStream;
		{
			ffmpegArgumentList.push_back("ffmpeg");
			// global options
			ffmpegArgumentList.push_back("-y");
			// input options
			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(sourceVideoAssetPathName);
			ffmpegArgumentList.push_back("-ss");
			ffmpegArgumentList.push_back(startTime);
			if (framesNumber != -1)
			{
				ffmpegArgumentList.push_back("-vframes");
				ffmpegArgumentList.push_back(to_string(framesNumber));
			}
			else
			{
				ffmpegArgumentList.push_back("-to");
				ffmpegArgumentList.push_back(endTime);
			}
			// ffmpegArgumentList.push_back("-async");
			// ffmpegArgumentList.push_back(to_string(1));

			for (string parameter : ffmpegEncodingProfileArgumentList)
				FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

			ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

			int iReturnedStatus = 0;

			try
			{
				chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				if (!ffmpegArgumentList.empty())
					copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

				SPDLOG_INFO(
					"cut with reencoding: Executing ffmpeg command"
					", encodingJobKey: {}"
					", ingestionJobKey: {}"
					", ffmpegArgumentList: {}",
					encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str()
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
						"cut with reencoding: ffmpeg command failed"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", iReturnedStatus: {}"
						", ffmpegArgumentList: {}",
						encodingJobKey, ingestionJobKey, iReturnedStatus, ffmpegArgumentListStream.str()
					);
					SPDLOG_ERROR(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "cut with reencoding: command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
								   ", ingestionJobKey: " + to_string(ingestionJobKey);
					throw runtime_error(errorMessage);
				}

				chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

				SPDLOG_INFO(
					"cut with reencoding: Executed ffmpeg command"
					", encodingJobKey: {}"
					", ingestionJobKey: {}"
					", ffmpegArgumentList: {}"
					", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
					encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(),
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
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg cut with reencoding failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", sourceVideoAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, sourceVideoAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			{
				SPDLOG_INFO(
					"Remove"
					", stagingEncodedAssetPathName: {}",
					stagingEncodedAssetPathName
				);
				fs::remove_all(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg cut with reencoding failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", sourceVideoAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, sourceVideoAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			{
				SPDLOG_INFO(
					"Remove"
					", stagingEncodedAssetPathName: {}",
					stagingEncodedAssetPathName
				);
				fs::remove_all(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg cut with reencoding failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", sourceVideoAssetPathName: {}"
			", stagingEncodedAssetPathName: {}",
			encodingJobKey, ingestionJobKey, sourceVideoAssetPathName, stagingEncodedAssetPathName
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			SPDLOG_INFO(
				"Remove"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			{
				SPDLOG_INFO(
					"Remove"
					", stagingEncodedAssetPathName: {}",
					stagingEncodedAssetPathName
				);
				fs::remove_all(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
}

void FFMpeg::extractTrackMediaToIngest(
	int64_t ingestionJobKey, string sourcePhysicalPath, vector<pair<string, int>> &tracksToBeExtracted, string extractTrackMediaPathName
)
{
	_currentApiName = APIName::ExtractTrackMediaToIngest;

	setStatus(ingestionJobKey
			  /*
			  encodingJobKey
			  videoDurationInMilliSeconds,
			  mmsAssetPathName
			  stagingEncodedAssetPathName
			  */
	);

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
			std::format("{}/{}_{}_{}.log", _ffmpegTempDir, "extractTrackMediaToIngest", _currentIngestionJobKey, sUtcTimestamp);
			*/
		_outputFfmpegPathFileName = std::format(
			"{}/{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "extractTrackMediaToIngest", _currentIngestionJobKey,
			tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec
		);
	}

	string mapParameters;
	bool videoTrackIsPresent = false;
	bool audioTrackIsPresent = false;
	for (pair<string, int> &trackToBeExtracted : tracksToBeExtracted)
	{
		string trackType;
		int trackNumber;

		tie(trackType, trackNumber) = trackToBeExtracted;

		mapParameters += (string("-map 0:") + (trackType == "video" ? "v" : "a") + ":" + to_string(trackNumber) + " ");

		if (trackType == "video")
			videoTrackIsPresent = true;
		else
			audioTrackIsPresent = true;
	}
	/*
		-map option: http://ffmpeg.org/ffmpeg.html#Advanced-options
		-c:a copy:      codec option for audio streams has been set to copy, so no decoding-filtering-encoding operations will occur
		-an:            disables audio stream selection for the output
	*/
	// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
	string globalOptions = "-y ";
	string inputOptions = "";
	string outputOptions = mapParameters + (videoTrackIsPresent ? (string("-c:v") + " copy ") : "") +
						   (audioTrackIsPresent ? (string("-c:a") + " copy ") : "") + (videoTrackIsPresent && !audioTrackIsPresent ? "-an " : "") +
						   (!videoTrackIsPresent && audioTrackIsPresent ? "-vn " : "");

	string ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg " + globalOptions + inputOptions + "-i " + sourcePhysicalPath + " " + outputOptions +
								  extractTrackMediaPathName + " " + "> " + _outputFfmpegPathFileName + " 2>&1";

#ifdef __APPLE__
	ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
#endif

	try
	{
		SPDLOG_INFO(
			"extractTrackMediaToIngest: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}",
			ingestionJobKey, ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = std::format(
				"extractTrackMediaToIngest: ffmpeg command failed"
				", executeCommandStatus: {}"
				", ffmpegExecuteCommand: {}",
				executeCommandStatus, ffmpegExecuteCommand
			);
			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "extractTrackMediaToIngest: command failed";
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"extractTrackMediaToIngest: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegExecuteCommand: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, ffmpegExecuteCommand, chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
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

		throw e;
	}

	SPDLOG_INFO(
		"Remove"
		", _outputFfmpegPathFileName: {}",
		_outputFfmpegPathFileName
	);
	fs::remove_all(_outputFfmpegPathFileName);
}
