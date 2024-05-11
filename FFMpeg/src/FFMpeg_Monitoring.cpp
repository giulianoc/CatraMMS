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
#include "catralibraries/StringUtils.h"
#include <regex>
#include <fstream>
/*
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "catralibraries/ProcessUtility.h"
#include <filesystem>
#include <sstream>
#include <string>
*/

double FFMpeg::getEncodingProgress()
{
	double encodingPercentage = 0.0;

	try
	{
		if (
			// really in this case it is calculated in EncoderVideoAudioProxy.cpp based ion TimePeriod
			_currentApiName == APIName::LiveProxy
			|| _currentApiName == APIName::LiveRecorder

			|| _currentApiName == APIName::LiveGrid
		)
		{
			// it's a live

			return -1.0;
		}

		if (!fs::exists(_outputFfmpegPathFileName.c_str()))
		{
			_logger->info(
				__FILEREF__ + "ffmpeg: Encoding progress not available" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
				", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
				", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		string ffmpegEncodingStatus;
		try
		{
			int lastCharsToBeReadToGetInfo = 10000;

			ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "ffmpeg: Failure reading the encoding progress file" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
				", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
				", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		{
			// frame= 2315 fps= 98 q=27.0 q=28.0 size=    6144kB time=00:01:32.35 bitrate= 545.0kbits/s speed=3.93x

			smatch m; // typedef std:match_result<string>

			regex e("time=([^ ]+)");

			bool match = regex_search(ffmpegEncodingStatus, m, e);

			// m is where the result is saved
			// we will have three results: the entire match, the first submatch, the second submatch
			// giving the following input: <email>user@gmail.com<end>
			// m.prefix(): everything is in front of the matched string (<email> in the previous example)
			// m.suffix(): everything is after the matched string (<end> in the previous example)

			/*
			_logger->info(string("m.size(): ") + to_string(m.size()) + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus);
			for (int n = 0; n < m.size(); n++)
			{
				_logger->info(string("m[") + to_string(n) + "]: str()=" + m[n].str());
			}
			cout << "m.prefix().str(): " << m.prefix().str() << endl;
			cout << "m.suffix().str(): " << m.suffix().str() << endl;
			 */

			if (m.size() >= 2)
			{
				string duration = m[1].str(); // 00:01:47.87

				stringstream ss(duration);
				string hours;
				string minutes;
				string seconds;
				string centsOfSeconds;
				char delim = ':';

				getline(ss, hours, delim);
				getline(ss, minutes, delim);

				delim = '.';
				getline(ss, seconds, delim);
				getline(ss, centsOfSeconds, delim);

				int iHours = atoi(hours.c_str());
				int iMinutes = atoi(minutes.c_str());
				int iSeconds = atoi(seconds.c_str());
				double dCentsOfSeconds = atoi(centsOfSeconds.c_str());

				// dCentsOfSeconds deve essere double altrimenti (dCentsOfSeconds / 100) sarà arrotontado ai secondi
				double encodingSeconds = (iHours * 3600) + (iMinutes * 60) + (iSeconds) + (dCentsOfSeconds / 100);
				double currentTimeInMilliSeconds = (encodingSeconds * 1000) + (_currentlyAtSecondPass ? _currentDurationInMilliSeconds : 0);
				//  encodingSeconds : _encodingItem->videoOrAudioDurationInMilliSeconds = x : 100

				encodingPercentage = 100 * currentTimeInMilliSeconds / (_currentDurationInMilliSeconds * (_twoPasses ? 2 : 1));

				if (encodingPercentage > 100.0 || encodingPercentage < 0.0)
				{
					_logger->error(
						__FILEREF__ + "Encoding progress too big" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
						", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", duration: " + duration +
						", encodingSeconds: " + to_string(encodingSeconds) + ", _twoPasses: " + to_string(_twoPasses) + ", _currentlyAtSecondPass: " +
						to_string(_currentlyAtSecondPass) + ", currentTimeInMilliSeconds: " + to_string(currentTimeInMilliSeconds) +
						", _currentDurationInMilliSeconds: " + to_string(_currentDurationInMilliSeconds) + ", encodingPercentage: " +
						to_string(encodingPercentage) + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
						", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
					);

					if (encodingPercentage > 100.0)
						encodingPercentage = 100.0;
					else // if (encodingPercentage < 0.0)
						encodingPercentage = 0.0;
				}
				else
				{
					_logger->info(
						__FILEREF__ + "Encoding progress" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) + ", encodingJobKey: " +
						to_string(_currentEncodingJobKey) + ", duration: " + duration + ", encodingSeconds: " + to_string(encodingSeconds) +
						", _twoPasses: " + to_string(_twoPasses) + ", _currentlyAtSecondPass: " + to_string(_currentlyAtSecondPass) +
						", currentTimeInMilliSeconds: " + to_string(currentTimeInMilliSeconds) + ", _currentDurationInMilliSeconds: " +
						to_string(_currentDurationInMilliSeconds) + ", encodingPercentage: " + to_string(encodingPercentage) +
						", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
						", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
					);
				}
			}
		}
	}
	catch (FFMpegEncodingStatusNotAvailable &e)
	{
		_logger->warn(
			__FILEREF__ + "ffmpeg: getEncodingProgress failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName + ", e.what(): " + e.what()
		);

		throw FFMpegEncodingStatusNotAvailable();
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: getEncodingProgress failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
		);

		throw e;
	}

	return encodingPercentage;
}

bool FFMpeg::nonMonotonousDTSInOutputLog()
{
	try
	{
		/*
		if (_currentApiName != "liveProxyByCDN")
		{
			// actually we need this check just for liveProxyByCDN

			return false;
		}
		*/

		if (!fs::exists(_outputFfmpegPathFileName.c_str()))
		{
			_logger->warn(
				__FILEREF__ + "ffmpeg: Encoding status not available" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
				", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
				", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		string ffmpegEncodingStatus;
		try
		{
			int lastCharsToBeReadToGetInfo = 10000;

			ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "ffmpeg: Failure reading the encoding status file" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
				", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
				", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		string lowerCaseFfmpegEncodingStatus;
		lowerCaseFfmpegEncodingStatus.resize(ffmpegEncodingStatus.size());
		transform(
			ffmpegEncodingStatus.begin(), ffmpegEncodingStatus.end(), lowerCaseFfmpegEncodingStatus.begin(),
			[](unsigned char c) { return tolower(c); }
		);

		// [flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result
		// in incorrect timestamps in the output file.
		if (lowerCaseFfmpegEncodingStatus.find("non-monotonous dts in output stream") != string::npos &&
			lowerCaseFfmpegEncodingStatus.find("incorrect timestamps") != string::npos)
			return true;
		else
			return false;
	}
	catch (FFMpegEncodingStatusNotAvailable &e)
	{
		_logger->warn(
			__FILEREF__ + "ffmpeg: nonMonotonousDTSInOutputLog failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName + ", e.what(): " + e.what()
		);

		throw FFMpegEncodingStatusNotAvailable();
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: nonMonotonousDTSInOutputLog failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
		);

		throw e;
	}
}

bool FFMpeg::forbiddenErrorInOutputLog()
{
	try
	{
		/*
		if (_currentApiName != "liveProxyByCDN")
		{
			// actually we need this check just for liveProxyByCDN

			return false;
		}
		*/

		if (!fs::exists(_outputFfmpegPathFileName.c_str()))
		{
			_logger->warn(
				__FILEREF__ + "ffmpeg: Encoding status not available" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
				", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
				", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		string ffmpegEncodingStatus;
		try
		{
			int lastCharsToBeReadToGetInfo = 10000;

			ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "ffmpeg: Failure reading the encoding status file" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
				", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
				", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		string lowerCaseFfmpegEncodingStatus;
		lowerCaseFfmpegEncodingStatus.resize(ffmpegEncodingStatus.size());
		transform(
			ffmpegEncodingStatus.begin(), ffmpegEncodingStatus.end(), lowerCaseFfmpegEncodingStatus.begin(),
			[](unsigned char c) { return tolower(c); }
		);

		// [https @ 0x555a8e428a00] HTTP error 403 Forbidden
		if (lowerCaseFfmpegEncodingStatus.find("http error 403 forbidden") != string::npos)
			return true;
		else
			return false;
	}
	catch (FFMpegEncodingStatusNotAvailable &e)
	{
		_logger->warn(
			__FILEREF__ + "ffmpeg: forbiddenErrorInOutputLog failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName + ", e.what(): " + e.what()
		);

		throw FFMpegEncodingStatusNotAvailable();
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: forbiddenErrorInOutputLog failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
		);

		throw e;
	}
}

tuple<long, long, double, double> FFMpeg::getRealTimeInfoByOutputLog()
{
	// frame= 2315 fps= 98 q=27.0 q=28.0 size=    6144kB time=00:01:32.35 bitrate= 545.0kbits/s speed=3.93x

	long frame = -1;
	long size = -1;
	double timeInMilliSeconds = -1.0;
	double bitRate = -1.0;

	try
	{
		if (!fs::exists(_outputFfmpegPathFileName.c_str()))
		{
			_logger->info(
				__FILEREF__ + "getRealTimeInfoByOutputLog: Encoding status not available" +
				", ingestionJobKey: " + to_string(_currentIngestionJobKey) + ", encodingJobKey: " + to_string(_currentEncodingJobKey) +
				", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		string ffmpegEncodingStatus;
		try
		{
			int lastCharsToBeReadToGetInfo = 50000;
			ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "getRealTimeInfoByOutputLog: Failure reading the encoding status file" +
				", ingestionJobKey: " + to_string(_currentIngestionJobKey) + ", encodingJobKey: " + to_string(_currentEncodingJobKey) +
				", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
				", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		{
			string toSearch = "frame=";
			size_t startIndex = ffmpegEncodingStatus.rfind(toSearch);
			if (startIndex == string::npos)
			{
				_logger->warn(
					__FILEREF__ + "ffmpeg: frame info was not found" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
					", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
					", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
					_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
				);
			}
			else
			{
				string value = ffmpegEncodingStatus.substr(startIndex + toSearch.size());
				value = StringUtils::ltrim(value);
				size_t endIndex = value.find(" ");
				if (endIndex == string::npos)
				{
					_logger->error(
						__FILEREF__ + "ffmpeg: encodingStatus bad format" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
						", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
						", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
						_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
					);
				}
				else
				{
					value = value.substr(0, endIndex);
					frame = stol(value);
				}
			}
		}
		{
			string toSearch = "size=";
			size_t startIndex = ffmpegEncodingStatus.rfind(toSearch);
			if (startIndex == string::npos)
			{
				_logger->warn(
					__FILEREF__ + "ffmpeg: size info was not found" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
					", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
					", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
					_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
				);
			}
			else
			{
				string value = ffmpegEncodingStatus.substr(startIndex + toSearch.size());
				value = StringUtils::ltrim(value);
				size_t endIndex = value.find(" ");
				if (endIndex == string::npos)
				{
					_logger->error(
						__FILEREF__ + "ffmpeg: encodingStatus bad format" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
						", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
						", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
						_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
					);
				}
				else
				{
					value = value.substr(0, endIndex);
					size = stol(value);
				}
			}
		}
		{
			string toSearch = "time=";
			size_t startIndex = ffmpegEncodingStatus.rfind(toSearch);
			if (startIndex == string::npos)
			{
				_logger->warn(
					__FILEREF__ + "ffmpeg: time info was not found" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
					", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
					", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
					_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
				);
			}
			else
			{
				string value = ffmpegEncodingStatus.substr(startIndex + toSearch.size());
				value = StringUtils::ltrim(value);
				size_t endIndex = value.find(" ");
				if (endIndex == string::npos)
				{
					_logger->error(
						__FILEREF__ + "ffmpeg: encodingStatus bad format" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
						", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
						", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
						_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
					);
				}
				else
				{
					value = value.substr(0, endIndex);

					// frame= 2315 fps= 98 q=27.0 q=28.0 size=    6144kB time=00:01:32.35 bitrate= 545.0kbits/s speed=3.93x
					stringstream ss(value);
					string hours;
					string minutes;
					string seconds;
					string centsOfSeconds;
					char delim = ':';
					getline(ss, hours, delim);
					getline(ss, minutes, delim);
					delim = '.';
					getline(ss, seconds, delim);
					getline(ss, centsOfSeconds, delim);

					int iHours = atoi(hours.c_str());
					int iMinutes = atoi(minutes.c_str());
					int iSeconds = atoi(seconds.c_str());
					double dCentsOfSeconds = atoi(centsOfSeconds.c_str());

					// dCentsOfSeconds deve essere double altrimenti (dCentsOfSeconds / 100) sarà arrotontado ai secondi
					timeInMilliSeconds = (iHours * 3600) + (iMinutes * 60) + (iSeconds) + (dCentsOfSeconds / 100);
					timeInMilliSeconds *= 1000;
				}
			}
		}
		{
			string toSearch = "bitrate=";
			size_t startIndex = ffmpegEncodingStatus.rfind(toSearch);
			if (startIndex == string::npos)
			{
				_logger->warn(
					__FILEREF__ + "ffmpeg: bitrate info was not found" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
					", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
					", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
					_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
				);
			}
			else
			{
				string value = ffmpegEncodingStatus.substr(startIndex + toSearch.size());
				value = StringUtils::ltrim(value);
				if (!StringUtils::startWith(value, "N/A"))
				{
				size_t endIndex = value.find("kbits/s");
				if (endIndex == string::npos)
				{
					_logger->error(
						__FILEREF__ + "ffmpeg: encodingStatus bad format" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
						", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
						", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName + ", _currentStagingEncodedAssetPathName: " +
						_currentStagingEncodedAssetPathName + ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
					);
				}
				else
				{
					value = value.substr(0, endIndex);
					bitRate = stof(value);
				}
				}
			}
		}
	}
	catch (FFMpegEncodingStatusNotAvailable &e)
	{
		_logger->info(
			__FILEREF__ + "getRealTimeInfoByOutputLog failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
			", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName + ", e.what(): " + e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "getRealTimeInfoByOutputLog failed" + ", ingestionJobKey: " + to_string(_currentIngestionJobKey) +
			", encodingJobKey: " + to_string(_currentEncodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
			", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName +
			", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
		);

		throw e;
	}

	return make_tuple(frame, size, timeInMilliSeconds, bitRate);
}

string FFMpeg::getLastPartOfFile(string pathFileName, int lastCharsToBeRead)
{
	string lastPartOfFile = "";
	char *buffer = nullptr;

	auto logger = spdlog::get("mmsEngineService");

	try
	{
		ifstream ifPathFileName(pathFileName);
		if (ifPathFileName)
		{
			int charsToBeRead;

			// get length of file:
			ifPathFileName.seekg(0, ifPathFileName.end);
			int fileSize = ifPathFileName.tellg();
			if (fileSize >= lastCharsToBeRead)
			{
				ifPathFileName.seekg(fileSize - lastCharsToBeRead, ifPathFileName.beg);
				charsToBeRead = lastCharsToBeRead;
			}
			else
			{
				ifPathFileName.seekg(0, ifPathFileName.beg);
				charsToBeRead = fileSize;
			}

			buffer = new char[charsToBeRead];
			ifPathFileName.read(buffer, charsToBeRead);
			if (ifPathFileName)
			{
				// all characters read successfully
				lastPartOfFile.assign(buffer, charsToBeRead);
			}
			else
			{
				// error: only is.gcount() could be read";
				lastPartOfFile.assign(buffer, ifPathFileName.gcount());
			}
			ifPathFileName.close();

			delete[] buffer;
		}
	}
	catch (exception &e)
	{
		if (buffer != nullptr)
			delete[] buffer;

		logger->error("getLastPartOfFile failed");
	}

	return lastPartOfFile;
}

