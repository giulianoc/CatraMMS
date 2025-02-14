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
#include "catralibraries/ProcessUtility.h"

void FFMpeg::generateFrameToIngest(
	int64_t ingestionJobKey, string mmsAssetPathName, int64_t videoDurationInMilliSeconds, double startTimeInSeconds, string frameAssetPathName,
	int imageWidth, int imageHeight, pid_t *pChildPid
)
{
	_currentApiName = APIName::GenerateFrameToIngest;

	setStatus(
		ingestionJobKey,
		-1, // encodingJobKey,
		videoDurationInMilliSeconds, mmsAssetPathName
	);

	SPDLOG_INFO(
		"generateFrameToIngest"
		", ingestionJobKey: {}"
		", mmsAssetPathName: {}"
		", videoDurationInMilliSeconds: {}"
		", startTimeInSeconds: {}"
		", frameAssetPathName: {}"
		", imageWidth: {}"
		", imageHeight: {}",
		ingestionJobKey, mmsAssetPathName, videoDurationInMilliSeconds, startTimeInSeconds, frameAssetPathName, imageWidth, imageHeight
	);

	if (!fs::exists(mmsAssetPathName))
	{
		string errorMessage = std::format(
			"Asset path name not existing"
			", ingestionJobKey: {}"
			", mmsAssetPathName: {}",
			ingestionJobKey, mmsAssetPathName
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	int iReturnedStatus = 0;

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

		_outputFfmpegPathFileName = std::format("{}/{}_{}_{}.log", _ffmpegTempDir, "generateFrameToIngest", _currentIngestionJobKey, sUtcTimestamp);
		*/
		_outputFfmpegPathFileName = std::format(
			"{}/{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "generateFrameToIngest", _currentIngestionJobKey,
			tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec
		);
	}

	// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;

	ffmpegArgumentList.push_back("ffmpeg");
	// global options
	ffmpegArgumentList.push_back("-y");
	// input options
	ffmpegArgumentList.push_back("-i");
	ffmpegArgumentList.push_back(mmsAssetPathName);
	// output options
	ffmpegArgumentList.push_back("-ss");
	ffmpegArgumentList.push_back(to_string(startTimeInSeconds));
	{
		ffmpegArgumentList.push_back("-vframes");
		ffmpegArgumentList.push_back(to_string(1));
	}
	ffmpegArgumentList.push_back("-an");
	ffmpegArgumentList.push_back("-s");
	ffmpegArgumentList.push_back(to_string(imageWidth) + "x" + to_string(imageHeight));
	ffmpegArgumentList.push_back(frameAssetPathName);

	try
	{
		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

		SPDLOG_INFO(
			"generateFramesToIngest: Executing ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegArgumentList: {}",
			ingestionJobKey, ffmpegArgumentListStream.str()
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
				"generateFrameToIngest: ffmpeg command failed"
				", ingestionJobKey: {}"
				", iReturnedStatus: {}"
				", ffmpegArgumentList: {}",
				ingestionJobKey, iReturnedStatus, ffmpegArgumentListStream.str()
			);
			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "generateFrameToIngest: command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey);
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"generateFrameToIngest: Executed ffmpeg command"
			", ingestionJobKey: {}"
			", ffmpegArgumentList: {}"
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
			ingestionJobKey, ffmpegArgumentListStream.str(), chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()
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
				", ingestionJobKey: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				_outputFfmpegPathFileName, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
			);
		else
			errorMessage = std::format(
				"ffmpeg: ffmpeg command failed"
				", _outputFfmpegPathFileName: {}"
				", ingestionJobKey: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				_outputFfmpegPathFileName, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
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

void FFMpeg::generateFramesToIngest(
	int64_t ingestionJobKey, int64_t encodingJobKey, string imagesDirectory, string imageBaseFileName, double startTimeInSeconds, int framesNumber,
	string videoFilter, int periodInSeconds, bool mjpeg, int imageWidth, int imageHeight, string mmsAssetPathName,
	int64_t videoDurationInMilliSeconds, pid_t *pChildPid
)
{
	_currentApiName = APIName::GenerateFramesToIngest;

	setStatus(
		ingestionJobKey, encodingJobKey, videoDurationInMilliSeconds, mmsAssetPathName
		// stagingEncodedAssetPathName
	);

	SPDLOG_INFO(
		"generateFramesToIngest"
		", ingestionJobKey: {}"
		", encodingJobKey: {}"
		", imagesDirectory: {}"
		", imageBaseFileName: {}"
		", startTimeInSeconds: {}"
		", framesNumber: {}"
		", videoFilter: {}"
		", periodInSeconds: {}"
		", mjpeg: {}"
		", imageWidth: {}"
		", imageHeight: {}"
		", mmsAssetPathName: {}"
		", videoDurationInMilliSeconds: {}",
		ingestionJobKey, encodingJobKey, imagesDirectory, imageBaseFileName, startTimeInSeconds, framesNumber, videoFilter, periodInSeconds, mjpeg,
		imageWidth, imageHeight, mmsAssetPathName, videoDurationInMilliSeconds
	);

	if (!fs::exists(mmsAssetPathName))
	{
		string errorMessage = std::format(
			"Asset path name not existing"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", mmsAssetPathName: {}",
			ingestionJobKey, encodingJobKey, mmsAssetPathName
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	if (fs::exists(imagesDirectory))
	{
		SPDLOG_INFO(
			"Remove"
			", imagesDirectory: {}",
			imagesDirectory
		);
		fs::remove_all(imagesDirectory);
	}
	{
		SPDLOG_INFO(
			"Create directory"
			", imagesDirectory: {}",
			imagesDirectory
		);
		fs::create_directories(imagesDirectory);
		fs::permissions(
			imagesDirectory,
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
				fs::perms::others_read | fs::perms::others_exec,
			fs::perm_options::replace
		);
	}

	int iReturnedStatus = 0;

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

		_outputFfmpegPathFileName = std::format(
			"{}/{}_{}_{}_{}.log", _ffmpegTempDir, "generateFramesToIngest", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp
		);
		*/
		_outputFfmpegPathFileName = std::format(
			"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "generateFramesToIngest", _currentIngestionJobKey,
			_currentEncodingJobKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
			tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
		);
	}

	string localImageFileName;
	if (mjpeg)
	{
		localImageFileName = imageBaseFileName + ".mjpeg";
	}
	else
	{
		if (framesNumber == -1 || framesNumber > 1)
			localImageFileName = imageBaseFileName + "_%04d.jpg";
		else
			localImageFileName = imageBaseFileName + ".jpg";
	}

	FFMpegFilters ffmpegFilters(_ffmpegTtfFontDir);

	string videoFilterParameters;
	if (videoFilter == "PeriodicFrame")
	{
		string filter;
		{
			json filterRoot;
			filterRoot["type"] = "fps";
			filterRoot["framesNumber"] = 1;
			filterRoot["periodInSeconds"] = periodInSeconds;

			filter = ffmpegFilters.getFilter(filterRoot, -1);
		}

		// videoFilterParameters = "-vf fps=1/" + to_string(periodInSeconds) + " ";
		videoFilterParameters = "-vf " + videoFilter + " ";
	}
	else if (videoFilter == "All-I-Frames")
	{
		if (mjpeg)
		{
			string filter;
			{
				json filterRoot;
				filterRoot["type"] = "select";
				filterRoot["frameType"] = "i-frame";
				filter = ffmpegFilters.getFilter(filterRoot, -1);
			}

			// videoFilterParameters = "-vf select='eq(pict_type,PICT_TYPE_I)' ";
			videoFilterParameters = "-vf " + filter + " ";
		}
		else
		{
			string filter;
			{
				json filterRoot;
				filterRoot["type"] = "select";
				filterRoot["frameType"] = "i-frame";
				filterRoot["fpsMode"] = "vfr";
				filter = ffmpegFilters.getFilter(filterRoot, -1);
			}

			// videoFilterParameters = "-vf select='eq(pict_type,PICT_TYPE_I)' -fps_mode vfr ";
			videoFilterParameters = "-vf " + filter + " ";
		}
	}

	/*
		ffmpeg -y -i [source.wmv] -f mjpeg -ss [10] -vframes 1 -an -s [176x144] [thumbnail_image.jpg]
		-y: overwrite output files
		-i: input file name
		-f: force format
		-ss: When used as an output option (before an output url), decodes but discards input
			until the timestamps reach position.
			Format: HH:MM:SS.xxx (xxx are decimals of seconds) or in seconds (sec.decimals)
		-vframes: set the number of video frames to record
		-an: disable audio
		-s set frame size (WxH or abbreviation)
	 */
	// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;

	ffmpegArgumentList.push_back("ffmpeg");
	// global options
	ffmpegArgumentList.push_back("-y");
	// input options
	ffmpegArgumentList.push_back("-i");
	ffmpegArgumentList.push_back(mmsAssetPathName);
	// output options
	ffmpegArgumentList.push_back("-ss");
	ffmpegArgumentList.push_back(to_string(startTimeInSeconds));
	if (framesNumber != -1)
	{
		ffmpegArgumentList.push_back("-vframes");
		ffmpegArgumentList.push_back(to_string(framesNumber));
	}
	FFMpegEncodingParameters::addToArguments(videoFilterParameters, ffmpegArgumentList);
	if (mjpeg)
	{
		ffmpegArgumentList.push_back("-f");
		ffmpegArgumentList.push_back("mjpeg");
	}
	ffmpegArgumentList.push_back("-an");
	ffmpegArgumentList.push_back("-s");
	ffmpegArgumentList.push_back(to_string(imageWidth) + "x" + to_string(imageHeight));
	ffmpegArgumentList.push_back(imagesDirectory + "/" + localImageFileName);

	try
	{
		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

		SPDLOG_INFO(
			"generateFramesToIngest: Executing ffmpeg command"
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
				"generateFramesToIngest: ffmpeg command failed"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", iReturnedStatus: {}"
				", ffmpegArgumentList: {}",
				encodingJobKey, ingestionJobKey, iReturnedStatus, ffmpegArgumentListStream.str()
			);
			SPDLOG_ERROR(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "generateFramesToIngest: command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
						   ", ingestionJobKey: " + to_string(ingestionJobKey);
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		SPDLOG_INFO(
			"generateFramesToIngest: Executed ffmpeg command"
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
				", ingestionJobKey: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				_outputFfmpegPathFileName, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
			);
		else
			errorMessage = std::format(
				"ffmpeg: ffmpeg command failed"
				", _outputFfmpegPathFileName: {}"
				", ingestionJobKey: {}"
				", ffmpegArgumentList: {}"
				", lastPartOfFfmpegOutputFile: {}"
				", e.what(): {}",
				_outputFfmpegPathFileName, ingestionJobKey, ffmpegArgumentListStream.str(), lastPartOfFfmpegOutputFile, e.what()
			);
		SPDLOG_ERROR(errorMessage);

		if (fs::exists(imagesDirectory))
		{
			SPDLOG_INFO(
				"Remove"
				", imagesDirectory: {}",
				imagesDirectory
			);
			fs::remove_all(imagesDirectory);
		}

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
