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
/*
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

void FFMpeg::videoSpeed(
	string mmsSourceVideoAssetPathName, int64_t videoDurationInMilliSeconds,

	string videoSpeedType, int videoSpeedSize,

	json encodingProfileDetailsRoot,

	string stagingEncodedAssetPathName, int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::VideoSpeed;

	setStatus(ingestionJobKey, encodingJobKey, videoDurationInMilliSeconds, mmsSourceVideoAssetPathName, stagingEncodedAssetPathName);

	try
	{
		if (!fs::exists(mmsSourceVideoAssetPathName))
		{
			string errorMessage = string("Source video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegEncodingProfileArgumentList;
		if (encodingProfileDetailsRoot != nullptr)
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
					string errorMessage = __FILEREF__ + "in case of recorder it is not possible to have an httpStreaming encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else */
				if (twoPasses)
				{
					// siamo sicuri che non sia possibile?
					/*
					string errorMessage = __FILEREF__ + "in case of videoSpeed it is not possible to have a two passes encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", twoPasses: " + to_string(twoPasses)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					string errorMessage = __FILEREF__ + "in case of videoSpeed it is not possible to have a two passes encoding. Change it to false" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
										  ", twoPasses: " + to_string(twoPasses);
					_logger->warn(errorMessage);
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
				string errorMessage = __FILEREF__ + "ffmpeg: encodingProfileParameter retrieving failed" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", e.what(): " + e.what();
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							   ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
				throw e;
			}
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
				fmt::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "videoSpeed", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
		}

		{
			string videoPTS;
			string audioTempo;

			if (videoSpeedType == "SlowDown")
			{
				switch (videoSpeedSize)
				{
				case 1:
					videoPTS = "1.1";
					audioTempo = "(1/1.1)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds / 100);

					break;
				case 2:
					videoPTS = "1.2";
					audioTempo = "(1/1.2)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 20 / 100);

					break;
				case 3:
					videoPTS = "1.3";
					audioTempo = "(1/1.3)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 30 / 100);

					break;
				case 4:
					videoPTS = "1.4";
					audioTempo = "(1/1.4)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 40 / 100);

					break;
				case 5:
					videoPTS = "1.5";
					audioTempo = "(1/1.5)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 50 / 100);

					break;
				case 6:
					videoPTS = "1.6";
					audioTempo = "(1/1.6)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 60 / 100);

					break;
				case 7:
					videoPTS = "1.7";
					audioTempo = "(1/1.7)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 70 / 100);

					break;
				case 8:
					videoPTS = "1.8";
					audioTempo = "(1/1.8)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 80 / 100);

					break;
				case 9:
					videoPTS = "1.9";
					audioTempo = "(1/1.9)";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 90 / 100);

					break;
				case 10:
					videoPTS = "2";
					audioTempo = "0.5";
					_currentDurationInMilliSeconds += (videoDurationInMilliSeconds * 100 / 100);

					break;
				default:
					videoPTS = "1.3";
					audioTempo = "(1/1.3)";

					break;
				}
			}
			else // if (videoSpeedType == "SpeedUp")
			{
				switch (videoSpeedSize)
				{
				case 1:
					videoPTS = "(1/1.1)";
					audioTempo = "1.1";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 10 / 100);

					break;
				case 2:
					videoPTS = "(1/1.2)";
					audioTempo = "1.2";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 20 / 100);

					break;
				case 3:
					videoPTS = "(1/1.3)";
					audioTempo = "1.3";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 30 / 100);

					break;
				case 4:
					videoPTS = "(1/1.4)";
					audioTempo = "1.4";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 40 / 100);

					break;
				case 5:
					videoPTS = "(1/1.5)";
					audioTempo = "1.5";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 50 / 100);

					break;
				case 6:
					videoPTS = "(1/1.6)";
					audioTempo = "1.6";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 60 / 100);

					break;
				case 7:
					videoPTS = "(1/1.7)";
					audioTempo = "1.7";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 70 / 100);

					break;
				case 8:
					videoPTS = "(1/1.8)";
					audioTempo = "1.8";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 80 / 100);

					break;
				case 9:
					videoPTS = "(1/1.9)";
					audioTempo = "1.9";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 90 / 100);

					break;
				case 10:
					videoPTS = "0.5";
					audioTempo = "2";
					_currentDurationInMilliSeconds -= (videoDurationInMilliSeconds * 100 / 100);

					break;
				default:
					videoPTS = "(1/1.3)";
					audioTempo = "1.3";

					break;
				}
			}

			string complexFilter = "-filter_complex [0:v]setpts=" + videoPTS + "*PTS[v];[0:a]atempo=" + audioTempo + "[a]";
			string videoMap = "-map [v]";
			string audioMap = "-map [a]";

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
			{
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceVideoAssetPathName);
				// output options
				FFMpegEncodingParameters::addToArguments(complexFilter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(videoMap, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(audioMap, ffmpegArgumentList);

				// encoding parameters
				if (encodingProfileDetailsRoot != nullptr)
				{
					for (string parameter : ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);
				}

				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(
						__FILEREF__ + "videoSpeed: Executing ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
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
						string errorMessage = __FILEREF__ + "videoSpeed: ffmpeg command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", iReturnedStatus: " + to_string(iReturnedStatus) +
											  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "videoSpeed command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(
						__FILEREF__ + "videoSpeed: Executed ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" +
						to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch (runtime_error &e)
				{
					*pChildPid = 0;

					string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9) // 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user" +
									   ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
									   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									   ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
									   ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
									   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									   ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
									   ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
					_logger->error(errorMessage);

					_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				fs::remove_all(_outputFfmpegPathFileName);
			}

			_logger->info(
				__FILEREF__ + "VideoSpeed file generated" + ", encodingJobKey: " + to_string(encodingJobKey) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			unsigned long ulFileSize = fs::file_size(stagingEncodedAssetPathName);

			if (ulFileSize == 0)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0" +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();

				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "command failed, encoded file size is 0" + ", encodingJobKey: " + to_string(encodingJobKey) +
							   ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg VideoSpeed failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName +
			", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			_logger->info(
				__FILEREF__ + "Remove" + ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			{
				_logger->info(__FILEREF__ + "Remove" + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				fs::remove_all(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg VideoSpeed failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName +
			", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			_logger->info(
				__FILEREF__ + "Remove" + ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			{
				_logger->info(__FILEREF__ + "Remove" + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				fs::remove_all(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg VideoSpeed failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName +
			", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			_logger->info(
				__FILEREF__ + "Remove" + ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			{
				_logger->info(__FILEREF__ + "Remove" + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				fs::remove_all(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
}

