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
#include "JSONUtils.h"
#include "catralibraries/ProcessUtility.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <regex>

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
			string errorMessage = std::format(
				"Source video asset path name not existing"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", mmsSourceVideoAssetPathName: {}",
				ingestionJobKey, encodingJobKey, mmsSourceVideoAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		if (!externalEncoder)
		{
			if (!fs::exists(mmsSourceImageAssetPathName))
			{
				string errorMessage = std::format(
					"Source image asset path name not existing"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", mmsSourceImageAssetPathName: {}",
					ingestionJobKey, encodingJobKey, mmsSourceImageAssetPathName
				);
				SPDLOG_ERROR(errorMessage);

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
					encodingProfileDetailsRoot, encodingProfileIsVideo,

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
					SPDLOG_ERROR(errorMessage);

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
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					SPDLOG_WARN(
						"in case of overlayImageOnVideo it is not possible to have a two passes encoding. Change it to false"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", twoPasses: {}",
						ingestionJobKey, encodingJobKey, twoPasses
					);
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
				SPDLOG_ERROR(
					"ffmpeg: encodingProfileParameter retrieving failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", e.what(): {}",
					ingestionJobKey, encodingJobKey, e.what()
				);

				throw e;
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
			*/

			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "overlayImageOnVideo", _currentIngestionJobKey,
				_currentEncodingJobKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
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

					SPDLOG_INFO(
						"overlayImageOnVideo: Executing ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", ffmpegArgumentList: ",
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
						SPDLOG_ERROR(
							"overlayImageOnVideo: ffmpeg command failed"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							encodingJobKey, ingestionJobKey, iReturnedStatus, ffmpegArgumentListStream.str()
						);

						// to hide the ffmpeg staff
						string errorMessage = std::format(
							"overlayImageOnVideo command failed"
							", encodingJobKey: {}"
							", ingestionJobKey: {}",
							encodingJobKey, ingestionJobKey
						);

						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"overlayImageOnVideo: Executed ffmpeg command"
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
						"remove"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", _outputFfmpegPathFileName: {}",
						ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				SPDLOG_INFO(
					"remove"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", _outputFfmpegPathFileName: {}",
					ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName
				);
				fs::remove_all(_outputFfmpegPathFileName);
			}

			SPDLOG_INFO(
				"Overlayed file generated"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName
			);

			unsigned long ulFileSize = fs::file_size(stagingEncodedAssetPathName);

			if (ulFileSize == 0)
			{
				SPDLOG_ERROR(
					"ffmpeg: ffmpeg command failed, encoded file size is 0"
					", encodingJobKey: {}"
					", ingestionJobKey: {}"
					", ffmpegArgumentList: {}",
					encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str()
				);

				// to hide the ffmpeg staff
				string errorMessage = std::format(
					"command failed, encoded file size is 0"
					", encodingJobKey: {}"
					", ingestionJobKey: {}",
					encodingJobKey, ingestionJobKey
				);
				throw runtime_error(errorMessage);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg overlay failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", mmsSourceVideoAssetPathName: {}"
			", mmsSourceImageAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, mmsSourceVideoAssetPathName, mmsSourceImageAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			SPDLOG_INFO(
				"remove"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				ingestionJobKey, encodingJobKey, stagingEncodedAssetPathName
			);
			fs::remove_all(stagingEncodedAssetPathName);
		}

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg overlay failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", mmsSourceVideoAssetPathName: {}"
			", mmsSourceImageAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, mmsSourceVideoAssetPathName, mmsSourceImageAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			// file in case of .3gp content OR directory in case of IPhone content
			SPDLOG_INFO(
				"remove"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				ingestionJobKey, encodingJobKey, stagingEncodedAssetPathName
			);
			fs::remove_all(stagingEncodedAssetPathName);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg overlay failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", mmsSourceVideoAssetPathName: {}"
			", mmsSourceImageAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, mmsSourceVideoAssetPathName, mmsSourceImageAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			SPDLOG_INFO(
				"remove"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				ingestionJobKey, encodingJobKey, stagingEncodedAssetPathName
			);
			fs::remove_all(stagingEncodedAssetPathName);
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

	try
	{
		if (!fs::exists(mmsSourceVideoAssetPathName))
		{
			string errorMessage = std::format(
				"Source video asset path name not existing"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", mmsSourceVideoAssetPathName: {}",
				ingestionJobKey, encodingJobKey, mmsSourceVideoAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

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
					encodingProfileDetailsRoot, encodingProfileIsVideo,

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
					SPDLOG_ERROR(errorMessage);

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
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					SPDLOG_WARN(
						"in case of overlayTextOnVideo it is not possible to have a two passes encoding. Change it to false"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", twoPasses: {}",
						ingestionJobKey, encodingJobKey, twoPasses
					);
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
				SPDLOG_ERROR(
					"ffmpeg: encodingProfileParameter retrieving failed"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", e.what(): {}",
					ingestionJobKey, encodingJobKey, e.what()
				);

				throw e;
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

			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{}.log", _ffmpegTempDir, "overlayTextOnVideo", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp
			);
			*/
			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "overlayTextOnVideo", _currentIngestionJobKey,
				_currentEncodingJobKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);
		}

		{
			string text = JSONUtils::asString(drawTextDetailsRoot, "text", "");

			string textTemporaryFileName = getDrawTextTemporaryPathName(_currentIngestionJobKey, _currentEncodingJobKey);
			{
				ofstream of(textTemporaryFileName, ofstream::trunc);
				of << text;
				of.flush();
			}

			SPDLOG_INFO(
				"overlayTextOnVideo: added text into a temporary file"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", textTemporaryFileName: {}",
				ingestionJobKey, encodingJobKey, textTemporaryFileName
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

					SPDLOG_INFO(
						"overlayTextOnVideo: Executing ffmpeg command"
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
						SPDLOG_ERROR(
							"overlayTextOnVideo: ffmpeg command failed"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							encodingJobKey, ingestionJobKey, iReturnedStatus, ffmpegArgumentListStream.str()
						);

						// to hide the ffmpeg staff
						string errorMessage = std::format(
							"overlayTextOnVideo command failed"
							", encodingJobKey: {}"
							", ingestionJobKey: {}",
							encodingJobKey, ingestionJobKey
						);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"overlayTextOnVideo: Executed ffmpeg command"
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
						"remove"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", :textTemporaryFileName {}",
						ingestionJobKey, encodingJobKey, textTemporaryFileName
					);
					fs::remove_all(textTemporaryFileName);

					SPDLOG_INFO(
						"remove"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", _outputFfmpegPathFileName: {}",
						ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName
					);
					fs::remove_all(_outputFfmpegPathFileName);

					if (iReturnedStatus == 9) // 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				SPDLOG_INFO(
					"remove"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", :textTemporaryFileName {}",
					ingestionJobKey, encodingJobKey, textTemporaryFileName
				);
				fs::remove_all(textTemporaryFileName);

				SPDLOG_INFO(
					"remove"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", _outputFfmpegPathFileName: {}",
					ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName
				);
				fs::remove_all(_outputFfmpegPathFileName);
			}

			SPDLOG_INFO(
				"Drawtext file generated"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName
			);

			unsigned long ulFileSize = fs::file_size(stagingEncodedAssetPathName);

			if (ulFileSize == 0)
			{
				SPDLOG_ERROR(
					"ffmpeg: ffmpeg command failed, encoded file size is 0"
					", encodingJobKey: {}"
					", ingestionJobKey: {}"
					", ffmpegArgumentList: {}",
					encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str()
				);

				// to hide the ffmpeg staff
				string errorMessage = std::format(
					"command failed, encoded file size is 0"
					", encodingJobKey: {}"
					", ingestionJobKey: {}",
					encodingJobKey, ingestionJobKey
				);
				throw runtime_error(errorMessage);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg drawtext failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", mmsSourceVideoAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, mmsSourceVideoAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			// file in case of .3gp content OR directory in case of IPhone content
			SPDLOG_INFO(
				"remove"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				ingestionJobKey, encodingJobKey, stagingEncodedAssetPathName
			);
			fs::remove_all(stagingEncodedAssetPathName);
		}

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg drawtext failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", mmsSourceVideoAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, mmsSourceVideoAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			// file in case of .3gp content OR directory in case of IPhone content
			SPDLOG_INFO(
				"remove"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				ingestionJobKey, encodingJobKey, stagingEncodedAssetPathName
			);
			fs::remove_all(stagingEncodedAssetPathName);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"ffmpeg: ffmpeg drawtext failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", mmsSourceVideoAssetPathName: {}"
			", stagingEncodedAssetPathName: {}"
			", e.what(): {}",
			encodingJobKey, ingestionJobKey, mmsSourceVideoAssetPathName, stagingEncodedAssetPathName, e.what()
		);

		if (fs::exists(stagingEncodedAssetPathName))
		{
			// file in case of .3gp content OR directory in case of IPhone content
			SPDLOG_INFO(
				"remove"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				ingestionJobKey, encodingJobKey, stagingEncodedAssetPathName
			);
			fs::remove_all(stagingEncodedAssetPathName);
		}

		throw e;
	}
}
