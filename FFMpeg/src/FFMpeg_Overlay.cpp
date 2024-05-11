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
#include "FFMpegFilters.h"
#include "FFMpegEncodingParameters.h"
#include "catralibraries/ProcessUtility.h"
#include <regex>
#include <fstream>
/*
#include "MMSCURL.h"
#include "catralibraries/StringUtils.h"
#include <filesystem>
#include <sstream>
#include <string>
*/

void FFMpeg::overlayImageOnVideo(
	bool externalEncoder, string mmsSourceVideoAssetPathName, int64_t videoDurationInMilliSeconds, string mmsSourceImageAssetPathName,
	string imagePosition_X_InPixel, string imagePosition_Y_InPixel, string stagingEncodedAssetPathName, json encodingProfileDetailsRoot,
	int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::OverlayImageOnVideo;

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

		if (!externalEncoder)
		{
			if (!fs::exists(mmsSourceImageAssetPathName))
			{
				string errorMessage = string("Source image asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
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
					string errorMessage = __FILEREF__ + "in case of overlayImageOnVideo it is not possible to have a two passes encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", twoPasses: " + to_string(twoPasses)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					string errorMessage = __FILEREF__ +
										  "in case of overlayImageOnVideo it is not possible to have a two passes encoding. Change it to false" +
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

			_outputFfmpegPathFileName = fmt::format(
				"{}/{}_{}_{}_{}.log", _ffmpegTempDir, "overlayImageOnVideo", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp
			);
		}

		{
			string ffmpegImagePosition_X_InPixel = regex_replace(imagePosition_X_InPixel, regex("video_width"), "main_w");
			ffmpegImagePosition_X_InPixel = regex_replace(ffmpegImagePosition_X_InPixel, regex("image_width"), "overlay_w");

			string ffmpegImagePosition_Y_InPixel = regex_replace(imagePosition_Y_InPixel, regex("video_height"), "main_h");
			ffmpegImagePosition_Y_InPixel = regex_replace(ffmpegImagePosition_Y_InPixel, regex("image_height"), "overlay_h");

			/*
			string ffmpegFilterComplex = string("-filter_complex 'overlay=")
					+ ffmpegImagePosition_X_InPixel + ":"
					+ ffmpegImagePosition_Y_InPixel + "'"
					;
			*/
			string ffmpegFilterComplex = string("-filter_complex overlay=") + ffmpegImagePosition_X_InPixel + ":" + ffmpegImagePosition_Y_InPixel;
			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
			{
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceVideoAssetPathName);
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceImageAssetPathName);
				// output options
				FFMpegEncodingParameters::addToArguments(ffmpegFilterComplex, ffmpegArgumentList);

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
						__FILEREF__ + "overlayImageOnVideo: Executing ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
						string errorMessage = __FILEREF__ + "overlayImageOnVideo: ffmpeg command failed" +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", iReturnedStatus: " + to_string(iReturnedStatus) +
											  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "overlayImageOnVideo command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(
						__FILEREF__ + "overlayImageOnVideo: Executed ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
				__FILEREF__ + "Overlayed file generated" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
			__FILEREF__ + "ffmpeg: ffmpeg overlay failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName +
			", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
			", e.what(): " + e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			_logger->info(
				__FILEREF__ + "Remove" + ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

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
			__FILEREF__ + "ffmpeg: ffmpeg overlay failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName +
			", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
			", e.what(): " + e.what()
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
			__FILEREF__ + "ffmpeg: ffmpeg overlay failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName +
			", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			_logger->info(
				__FILEREF__ + "Remove" + ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			{
				_logger->info(__FILEREF__ + "Remove" + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				fs::remove_all(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
}

void FFMpeg::overlayTextOnVideo(
	string mmsSourceVideoAssetPathName, int64_t videoDurationInMilliSeconds,

	json drawTextDetailsRoot,

	json encodingProfileDetailsRoot, string stagingEncodedAssetPathName, int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::OverlayTextOnVideo;

	setStatus(ingestionJobKey, encodingJobKey, videoDurationInMilliSeconds, mmsSourceVideoAssetPathName, stagingEncodedAssetPathName);

	string textTemporaryFileName;
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
					string errorMessage = __FILEREF__ + "in case of overlayTextOnVideo it is not possible to have a two passes encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", twoPasses: " + to_string(twoPasses)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					string errorMessage = __FILEREF__ +
										  "in case of overlayTextOnVideo it is not possible to have a two passes encoding. Change it to false" +
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
				// addToArguments(ffmpegVideoResolutionParameter, ffmpegEncodingProfileArgumentList);
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

			_outputFfmpegPathFileName = fmt::format(
				"{}/{}_{}_{}_{}.log", _ffmpegTempDir, "overlayTextOnVideo", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp
			);
		}

		{
			string text = JSONUtils::asString(drawTextDetailsRoot, "text", "");

			{
				textTemporaryFileName =
					_ffmpegTempDir + "/" + to_string(_currentIngestionJobKey) + "_" + to_string(_currentEncodingJobKey) + ".overlayText";
				ofstream of(textTemporaryFileName, ofstream::trunc);
				of << text;
				of.flush();
			}

			_logger->info(
				__FILEREF__ + "overlayTextOnVideo: added text into a temporary file" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", textTemporaryFileName: " + textTemporaryFileName
			);

			string ffmpegDrawTextFilter;
			{
				json filterRoot = drawTextDetailsRoot;
				filterRoot["type"] = "drawtext";
				filterRoot["textFilePathName"] = textTemporaryFileName;

				FFMpegFilters ffmpegFilters(_ffmpegTtfFontDir);
				ffmpegDrawTextFilter = ffmpegFilters.getFilter(filterRoot, -1);
			}
			/*
			string ffmpegDrawTextFilter = getDrawTextVideoFilterDescription(ingestionJobKey,
				"", textTemporaryFileName, reloadAtFrameInterval,
				textPosition_X_InPixel, textPosition_Y_InPixel, fontType, fontSize,
				fontColor, textPercentageOpacity, shadowX, shadowY,
				boxEnable, boxColor, boxPercentageOpacity, boxBorderW, -1);
			*/

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
				// FFMpegEncodingParameters::addToArguments(ffmpegDrawTextFilter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-vf");
				ffmpegArgumentList.push_back(ffmpegDrawTextFilter);

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
						__FILEREF__ + "overlayTextOnVideo: Executing ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
						string errorMessage = __FILEREF__ + "overlayTextOnVideo: ffmpeg command failed" +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", iReturnedStatus: " + to_string(iReturnedStatus) +
											  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "overlayTextOnVideo command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(
						__FILEREF__ + "overlayTextOnVideo: Executed ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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

					_logger->info(__FILEREF__ + "Remove" + ", textTemporaryFileName: " + textTemporaryFileName);
					fs::remove_all(textTemporaryFileName);

					_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				_logger->info(__FILEREF__ + "Remove" + ", textTemporaryFileName: " + textTemporaryFileName);
				fs::remove_all(textTemporaryFileName);

				_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				fs::remove_all(_outputFfmpegPathFileName);
			}

			_logger->info(
				__FILEREF__ + "Drawtext file generated" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
			__FILEREF__ + "ffmpeg: ffmpeg drawtext failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
			__FILEREF__ + "ffmpeg: ffmpeg drawtext failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
			__FILEREF__ + "ffmpeg: ffmpeg drawtext failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
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

