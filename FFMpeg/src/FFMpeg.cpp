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
#include "catralibraries/StringUtils.h"

FFMpeg::FFMpeg(json configuration, shared_ptr<spdlog::logger> logger)
{
	_logger = logger;

	_ffmpegPath = JSONUtils::asString(configuration["ffmpeg"], "path", "");
	_ffmpegTempDir = JSONUtils::asString(configuration["ffmpeg"], "tempDir", "");
	_ffmpegEndlessRecursivePlaylistDir = JSONUtils::asString(configuration["ffmpeg"], "endlessRecursivePlaylistDir", "");
	_ffmpegTtfFontDir = JSONUtils::asString(configuration["ffmpeg"], "ttfFontDir", "");

	_youTubeDlPath = JSONUtils::asString(configuration["youTubeDl"], "path", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", youTubeDl->path: " + _youTubeDlPath);
	_pythonPathName = JSONUtils::asString(configuration["youTubeDl"], "pythonPathName", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", youTubeDl->pythonPathName: " + _pythonPathName);

	_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(configuration["storage"], "waitingNFSSync_maxMillisecondsToWait", 60000);
	/*
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", storage->waitingNFSSync_attemptNumber: "
		+ to_string(_waitingNFSSync_attemptNumber)
	);
	*/
	_waitingNFSSync_milliSecondsWaitingBetweenChecks =
		JSONUtils::asInt(configuration["storage"], "waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	/*
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", storage->waitingNFSSync_sleepTimeInSeconds: "
		+ to_string(_waitingNFSSync_sleepTimeInSeconds)
	);
	*/

	// _startCheckingFrameInfoInMinutes = JSONUtils::asInt(configuration["ffmpeg"], "startCheckingFrameInfoInMinutes", 5);

	_charsToBeReadFromFfmpegErrorOutput = 1024 * 3;

	_twoPasses = false;
	_currentlyAtSecondPass = false;

	_currentIngestionJobKey = -1;			  // just for log
	_currentEncodingJobKey = -1;			  // just for log
	_currentDurationInMilliSeconds = -1;	  // in case of some functionalities, it is important for getEncodingProgress
	_currentMMSSourceAssetPathName = "";	  // just for log
	_currentStagingEncodedAssetPathName = ""; // just for log

	_startFFMpegMethod = chrono::system_clock::now();

	_incrontabConfigurationDirectory = "/home/mms/mms/conf";
	_incrontabConfigurationFileName = "incrontab.txt";
	_incrontabBinary = "/usr/bin/incrontab";
}

FFMpeg::~FFMpeg() {}

void FFMpeg::encodingVideoCodecValidation(string codec, shared_ptr<spdlog::logger> logger)
{
	if (codec != "libx264" && codec != "libvpx" && codec != "rawvideo" && codec != "mpeg4" && codec != "xvid")
	{
		string errorMessage = __FILEREF__ + "ffmpeg: Video codec is wrong" + ", codec: " + codec;

		logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
}

void FFMpeg::setStatus(
	int64_t ingestionJobKey, int64_t encodingJobKey, int64_t durationInMilliSeconds, string mmsSourceAssetPathName, string stagingEncodedAssetPathName
)
{

	_currentIngestionJobKey = ingestionJobKey;						   // just for log
	_currentEncodingJobKey = encodingJobKey;						   // just for log
	_currentDurationInMilliSeconds = durationInMilliSeconds;		   // in case of some functionalities, it is important for getEncodingProgress
	_currentMMSSourceAssetPathName = mmsSourceAssetPathName;		   // just for log
	_currentStagingEncodedAssetPathName = stagingEncodedAssetPathName; // just for log

	_startFFMpegMethod = chrono::system_clock::now();
}

/*
int FFMpeg::progressDownloadCallback(
	int64_t ingestionJobKey, chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated, double dltotal, double dlnow,
	double ultotal, double ulnow
)
{

	int progressUpdatePeriodInSeconds = 15;

	chrono::system_clock::time_point now = chrono::system_clock::now();

	if (dltotal != 0 && (dltotal == dlnow || now - lastTimeProgressUpdate >= chrono::seconds(progressUpdatePeriodInSeconds)))
	{
		double progress = (dlnow / dltotal) * 100;
		// int downloadingPercentage = floorf(progress * 100) / 100;
		// this is to have one decimal in the percentage
		double downloadingPercentage = ((double)((int)(progress * 10))) / 10;

		_logger->info(
			__FILEREF__ + "Download still running" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", downloadingPercentage: " + to_string(downloadingPercentage) + ", dltotal: " + to_string(dltotal) + ", dlnow: " + to_string(dlnow) +
			", ultotal: " + to_string(ultotal) + ", ulnow: " + to_string(ulnow)
		);

		lastTimeProgressUpdate = now;

		if (lastPercentageUpdated != downloadingPercentage)
		{
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", downloadingPercentage: " + to_string(downloadingPercentage)
			);
			// downloadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceDownloadingInProgress (
			//     ingestionJobKey, downloadingPercentage);

			lastPercentageUpdated = downloadingPercentage;
		}

		// if (downloadingStoppedByUser)
		//     return 1;   // stop downloading
	}

	return 0;
}
*/

/*
int FFMpeg::progressDownloadCallback(double dltotal, double dlnow, double ultotal, double ulnow)
{

	int progressUpdatePeriodInSeconds = 15;

	chrono::system_clock::time_point now = chrono::system_clock::now();

	if (dltotal != 0 && (dltotal == dlnow || now - _lastTimeProgressUpdate >= chrono::seconds(progressUpdatePeriodInSeconds)))
	{
		double progress = (dlnow / dltotal) * 100;
		// int downloadingPercentage = floorf(progress * 100) / 100;
		// this is to have one decimal in the percentage
		double downloadingPercentage = ((double)((int)(progress * 10))) / 10;

		SPDLOG_INFO(
			"Download still running"
			", ingestionJobKey: {}"
			", downloadingPercentage: {}"
			", dltotal: {}"
			", dlnow: {}"
			", ultotal: {}"
			", ulnow: {}",
			_ingestionJobKey, downloadingPercentage, dltotal, dlnow, ultotal, ulnow
		);

		_lastTimeProgressUpdate = now;

		if (_lastPercentageUpdated != downloadingPercentage)
		{
			SPDLOG_INFO(
				"Update IngestionJob"
				", ingestionJobKey: {}"
				", downloadingPercentage: {}",
				_ingestionJobKey, downloadingPercentage
			);
			// downloadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceDownloadingInProgress (
			//     ingestionJobKey, downloadingPercentage);

			_lastPercentageUpdated = downloadingPercentage;
		}

		// if (downloadingStoppedByUser)
		//     return 1;   // stop downloading
	}

	return 0;
}
*/

void FFMpeg::renameOutputFfmpegPathFileName(int64_t ingestionJobKey, int64_t encodingJobKey, string outputFfmpegPathFileName)
{
	string debugOutputFfmpegPathFileName;
	try
	{
		char sNow[64];

		time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
		tm tmUtcNow;
		localtime_r(&utcNow, &tmUtcNow);
		sprintf(
			sNow, "%04d-%02d-%02d-%02d-%02d-%02d", tmUtcNow.tm_year + 1900, tmUtcNow.tm_mon + 1, tmUtcNow.tm_mday, tmUtcNow.tm_hour, tmUtcNow.tm_min,
			tmUtcNow.tm_sec
		);

		debugOutputFfmpegPathFileName = outputFfmpegPathFileName + "." + sNow;

		_logger->info(
			__FILEREF__ + "move" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
			", outputFfmpegPathFileName: " + outputFfmpegPathFileName + ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
		);
		// fs::rename works only if source and destination are on the same file systems
		if (fs::exists(outputFfmpegPathFileName))
			fs::rename(outputFfmpegPathFileName, debugOutputFfmpegPathFileName);
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "move failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
			", outputFfmpegPathFileName: " + outputFfmpegPathFileName + ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
		);
	}
}

bool FFMpeg::isNumber(int64_t ingestionJobKey, string number)
{
	try
	{
		for (int i = 0; i < number.length(); i++)
		{
			if (!isdigit(number[i]) && number[i] != '.' && number[i] != '-')
				return false;
		}

		return true;
	}
	catch (exception &e)
	{
		string errorMessage = fmt::format(
			"isNumber failed"
			", ingestionJobKey: {}"
			", number: {}"
			", exception: {}",
			ingestionJobKey, number, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

// ritorna: secondi (double), centesimi (long). Es: 5.27 e 527. I centesimi sono piu precisi perchè evitano
//	i troncamenti di un double
pair<double, long> FFMpeg::timeToSeconds(int64_t ingestionJobKey, string time)
{
	try
	{
		string localTime = StringUtils::trimTabToo(time);
		if (localTime == "")
			return make_pair(0.0, 0);

		if (isNumber(ingestionJobKey, localTime))
			return make_pair(stod(localTime), stod(localTime) * 100);

		// format: [-][HH:]MM:SS[.m...]

		bool isNegative = false;
		if (localTime[0] == '-')
			isNegative = true;

		// int semiColonNumber = count_if(localTime.begin(), localTime.end(), []( char c ){return c == ':';});
		bool hourPresent = false;
		int hours = 0;
		int minutes = 0;
		int seconds = 0;
		int decimals = 0; // centesimi di secondo

		bool hoursPresent = std::count_if(localTime.begin(), localTime.end(), [](char c) { return c == ':'; }) == 2;
		bool decimalPresent = localTime.find(".") != string::npos;

		stringstream ss(isNegative ? localTime.substr(1) : localTime);

		char delim = ':';

		if (hoursPresent)
		{
			string sHours;
			getline(ss, sHours, delim);
			hours = stoi(sHours);
		}

		string sMinutes;
		getline(ss, sMinutes, delim);
		minutes = stoi(sMinutes);

		delim = '.';
		string sSeconds;
		getline(ss, sSeconds, delim);
		seconds = stoi(sSeconds);

		if (decimalPresent)
		{
			string sDecimals;
			getline(ss, sDecimals, delim);
			decimals = stoi(sDecimals);
		}

		double dSeconds;
		long centsOfSeconds;
		if (decimals != 0)
		{
			sSeconds = to_string((hours * 3600) + (minutes * 60) + seconds) + "." + to_string(decimals);
			dSeconds = stod(sSeconds);

			centsOfSeconds = ((hours * 3600) + (minutes * 60) + seconds) * 100 + decimals;
		}
		else
		{
			dSeconds = (hours * 3600) + (minutes * 60) + seconds;
			centsOfSeconds = ((hours * 3600) + (minutes * 60) + seconds) * 100;
		}

		if (isNegative)
		{
			dSeconds *= -1;
			centsOfSeconds *= -1;
		}

		return make_pair(dSeconds, centsOfSeconds);
	}
	catch (exception &e)
	{
		string errorMessage = fmt::format(
			"timeToSeconds failed"
			", ingestionJobKey: {}"
			", time: {}"
			", exception: {}",
			ingestionJobKey, time, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

string FFMpeg::secondsToTime(int64_t ingestionJobKey, double dSeconds)
{
	try
	{
		double dLocalSeconds = dSeconds;

		int hours = dLocalSeconds / 3600;
		dLocalSeconds -= (hours * 3600);

		int minutes = dLocalSeconds / 60;
		dLocalSeconds -= (minutes * 60);

		int seconds = dLocalSeconds;
		dLocalSeconds -= seconds;

		string time;
		{
			char buffer[64];
			sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
			time = buffer;
			// dLocalSeconds dovrebbe essere 0.12345
			if (dLocalSeconds > 0.0)
			{
				// poichè siamo interessati ai decimi di secondo
				int decimals = dLocalSeconds * 100;
				time += ("." + to_string(decimals));
				/*
				string decimals = to_string(dLocalSeconds);
				size_t decimalPoint = decimals.find(".");
				if (decimalPoint != string::npos)
					time += decimals.substr(decimalPoint);
				*/
			}
		}

		return time;
	}
	catch (exception &e)
	{
		string errorMessage = fmt::format(
			"secondsToTime failed"
			", ingestionJobKey: {}"
			", dSeconds: {}"
			", exception: {}",
			ingestionJobKey, dSeconds, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

string FFMpeg::centsOfSecondsToTime(int64_t ingestionJobKey, long centsOfSeconds)
{
	try
	{
		long localCentsOfSeconds = centsOfSeconds;

		int hours = localCentsOfSeconds / 360000;
		localCentsOfSeconds -= (hours * 360000);

		int minutes = localCentsOfSeconds / 6000;
		localCentsOfSeconds -= (minutes * 6000);

		int seconds = localCentsOfSeconds / 100;
		localCentsOfSeconds -= (seconds * 100);

		string time;
		{
			char buffer[64];
			sprintf(buffer, "%02d:%02d:%02d.%02ld", hours, minutes, seconds, localCentsOfSeconds);
			time = buffer;
		}

		return time;
	}
	catch (exception &e)
	{
		string errorMessage = fmt::format(
			"centsOfSecondsToTime failed"
			", ingestionJobKey: {}"
			", centsOfSeconds: {}"
			", exception: {}",
			ingestionJobKey, centsOfSeconds, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

string FFMpeg::getDrawTextTemporaryPathName(int64_t ingestionJobKey, int64_t encodingJobKey, int outputIndex)
{
	if (outputIndex != -1)
		return fmt::format("{}/{}_{}_{}.overlayText", _ffmpegTempDir, ingestionJobKey, encodingJobKey, outputIndex);
	else
		return fmt::format("{}/{}_{}.overlayText", _ffmpegTempDir, ingestionJobKey, encodingJobKey);
}
