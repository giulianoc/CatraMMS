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

void FFMpeg::introOutroOverlay(
	string introVideoAssetPathName, int64_t introVideoDurationInMilliSeconds, string mainVideoAssetPathName, int64_t mainVideoDurationInMilliSeconds,
	string outroVideoAssetPathName, int64_t outroVideoDurationInMilliSeconds,

	int64_t introOverlayDurationInSeconds, int64_t outroOverlayDurationInSeconds,

	bool muteIntroOverlay, bool muteOutroOverlay,

	json encodingProfileDetailsRoot,

	string stagingEncodedAssetPathName, int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::IntroOutroOverlay;

	_logger->info(
		__FILEREF__ + "Received " + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", encodingJobKey: " + to_string(encodingJobKey) + ", introVideoAssetPathName: " + introVideoAssetPathName +
		", introVideoDurationInMilliSeconds: " + to_string(introVideoDurationInMilliSeconds) + ", mainVideoAssetPathName: " + mainVideoAssetPathName +
		", mainVideoDurationInMilliSeconds: " + to_string(mainVideoDurationInMilliSeconds) + ", outroVideoAssetPathName: " + outroVideoAssetPathName +
		", outroVideoDurationInMilliSeconds: " + to_string(outroVideoDurationInMilliSeconds) +
		", introOverlayDurationInSeconds: " + to_string(introOverlayDurationInSeconds) +
		", outroOverlayDurationInSeconds: " + to_string(outroOverlayDurationInSeconds) + ", muteIntroOverlay: " + to_string(muteIntroOverlay) +
		", muteOutroOverlay: " + to_string(muteOutroOverlay) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
	);

	setStatus(
		ingestionJobKey, encodingJobKey,
		mainVideoDurationInMilliSeconds + (introVideoDurationInMilliSeconds - introOverlayDurationInSeconds) +
			(outroVideoDurationInMilliSeconds - outroOverlayDurationInSeconds),
		mainVideoAssetPathName, stagingEncodedAssetPathName
	);

	try
	{
		if (!fs::exists(introVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", introVideoAssetPathName: " + introVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		else if (!fs::exists(mainVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", mainVideoAssetPathName: " + mainVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		else if (!fs::exists(outroVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", outroVideoAssetPathName: " + outroVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (encodingProfileDetailsRoot == nullptr)
		{
			string errorMessage = __FILEREF__ + "encodingProfileDetailsRoot is mandatory" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		FFMpegEncodingParameters ffmpegEncodingParameters(
			ingestionJobKey, encodingJobKey, encodingProfileDetailsRoot,
			true, // isVideo,
			-1,	  // videoTrackIndexToBeUsed,
			-1,	  // audioTrackIndexToBeUsed,
			stagingEncodedAssetPathName,
			nullptr, // videoTracksRoot,
			nullptr, // audioTracksRoot,

			_twoPasses, // out

			_ffmpegTempDir, _ffmpegTtfFontDir, _logger
		);

		/*
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
					_logger,
					encodingProfileDetailsRoot,
					encodingProfileIsVideo,

					httpStreamingFileFormat,
					ffmpegHttpStreamingParameter,

					ffmpegFileFormatParameter,

					ffmpegVideoCodecParameter,
					ffmpegVideoProfileParameter,
					ffmpegVideoOtherParameters,
					twoPasses,
					ffmpegVideoFrameRateParameter,
					ffmpegVideoKeyFramesRateParameter,
					videoBitRatesInfo,

					ffmpegAudioCodecParameter,
					ffmpegAudioOtherParameters,
					ffmpegAudioChannelsParameter,
					ffmpegAudioSampleRateParameter,
					audioBitRatesInfo
				);

				tuple<string, int, int, int, string, string, string> videoBitRateInfo
					= videoBitRatesInfo[0];
				tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore,
					ffmpegVideoBitRateParameter,
					ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter) = videoBitRateInfo;

				ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

				if (twoPasses)
				{
					// siamo sicuri che non sia possibile?
					twoPasses = false;

					string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to
		false"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", twoPasses: " + to_string(twoPasses)
					;
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
			catch(runtime_error& e)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: encodingProfileParameter retrieving failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				throw e;
			}
		}
		*/

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
				"{}/{}_{}_{}_{}.log", _ffmpegTempDir, "introOutroOverlay", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp
			);
		}

		/*
		ffmpeg \
			-i 2_conAlphaBianco/ICML_INTRO.mov \
			-i intervista.mts \
			-i 2_conAlphaBianco/ICML_OUTRO.mov \
			-filter_complex \
				"[0:a]volume=enable='between(t,8,12)':volume=0[intro_overlay_muted]; \
				[1:v]tpad=start_duration=8:start_mode=add:color=white[main_video_moved]; \
				[1:a]adelay=delays=8s:all=1[main_audio_moved]; \
				[2:v]setpts=PTS+125/TB[outro_video_moved]; \
				[2:a]volume=enable='between(t,0,3)':volume=0,adelay=delays=125s:all=1[outro_audio_overlayMuted_and_moved]; \
				[main_video_moved][0:v]overlay=eof_action=pass[overlay_intro_main]; \
				[overlay_intro_main][outro_video_moved]overlay=enable='gte(t,125)'[final_video]; \
				[main_audio_moved][intro_overlay_muted][outro_audio_overlayMuted_and_moved]amix=inputs=3[final_audio]" \
				-map "[final_video]" -map "[final_audio]" -c:v libx264 -profile:v high -bf 2 -g 30 -crf 18 \
				-pix_fmt yuv420p \
				output.mp4 -y
		*/
		string ffmpegFilterComplex = "-filter_complex ";
		{
			long introStartOverlayInSeconds = (introVideoDurationInMilliSeconds - (introOverlayDurationInSeconds * 1000)) / 1000;
			long introVideoDurationInSeconds = introVideoDurationInMilliSeconds / 1000;
			long outroStartOverlayInSeconds = introStartOverlayInSeconds + (mainVideoDurationInMilliSeconds / 1000) - outroOverlayDurationInSeconds;

			if (introStartOverlayInSeconds < 0 || outroStartOverlayInSeconds < 0)
			{
				string errorMessage = __FILEREF__ + "introOutroOverlay: wrong durations" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", introStartOverlayInSeconds: " + to_string(introStartOverlayInSeconds) +
									  ", outroStartOverlayInSeconds: " + to_string(outroStartOverlayInSeconds);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (muteIntroOverlay)
				ffmpegFilterComplex += "[0:a]volume=enable='between(t," + to_string(introStartOverlayInSeconds) + "," +
									   to_string(introVideoDurationInSeconds) + ")':volume=0[intro_overlay_muted];";
			ffmpegFilterComplex +=
				"[1:v]tpad=start_duration=" + to_string(introStartOverlayInSeconds) + ":start_mode=add:color=white[main_video_moved];";
			ffmpegFilterComplex += "[1:a]adelay=delays=" + to_string(introStartOverlayInSeconds) + "s:all=1[main_audio_moved];";
			ffmpegFilterComplex += "[2:v]setpts=PTS+" + to_string(outroStartOverlayInSeconds) + "/TB[outro_video_moved];";
			ffmpegFilterComplex += "[2:a]";
			if (muteOutroOverlay)
				ffmpegFilterComplex += "volume=enable='between(t,0," + to_string(outroOverlayDurationInSeconds) + ")':volume=0,";
			ffmpegFilterComplex += "adelay=delays=" + to_string(outroStartOverlayInSeconds) + "s:all=1[outro_audio_overlayMuted_and_moved];";
			ffmpegFilterComplex += "[main_video_moved][0:v]overlay=eof_action=pass[overlay_intro_main];";
			ffmpegFilterComplex +=
				"[overlay_intro_main][outro_video_moved]overlay=enable='gte(t," + to_string(outroStartOverlayInSeconds) + ")'[final_video];";
			ffmpegFilterComplex += "[main_audio_moved]";
			if (muteIntroOverlay)
				ffmpegFilterComplex += "[intro_overlay_muted]";
			else
				ffmpegFilterComplex += "[0:a]";
			ffmpegFilterComplex += "[outro_audio_overlayMuted_and_moved]amix=inputs=3[final_audio]";
		}

		vector<string> ffmpegArgumentList;
		ostringstream ffmpegArgumentListStream;

		if (ffmpegEncodingParameters._httpStreamingFileFormat != "")
		{
		}
		else
		{
			if (_twoPasses)
			{
			}
			else
			{
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");

				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(introVideoAssetPathName);
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mainVideoAssetPathName);
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(outroVideoAssetPathName);

				// output options
				FFMpegEncodingParameters::addToArguments(ffmpegFilterComplex, ffmpegArgumentList);

				ffmpegArgumentList.push_back("-map");
				ffmpegArgumentList.push_back("[final_video]");
				ffmpegArgumentList.push_back("-map");
				ffmpegArgumentList.push_back("[final_audio]");

				ffmpegArgumentList.push_back("-pix_fmt");
				// yuv420p: the only option for broad compatibility
				ffmpegArgumentList.push_back("yuv420p");

				ffmpegEncodingParameters.applyEncoding(
					-1,	   // -1: NO two passes
					true,  // outputFileToBeAdded
					false, // videoResolutionToBeAdded
					nullptr,
					ffmpegArgumentList // out
				);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(
						__FILEREF__ + "introOutroOverlay: Executing ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
						string errorMessage = __FILEREF__ + "introOutroOverlay: ffmpeg command failed" +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", iReturnedStatus: " + to_string(iReturnedStatus) +
											  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "introOutroOverlay command failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
									   ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(
						__FILEREF__ + "introOutroOverlay: Executed ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
				__FILEREF__ + "introOutroOverlay file generated" + ", encodingJobKey: " + to_string(encodingJobKey) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			unsigned long ulFileSize = fs::file_size(stagingEncodedAssetPathName);

			if (ulFileSize == 0)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, pictureInPicture encoded file size is 0" +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "command failed, pictureInPicture encoded file size is 0" +
							   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg introOutroOverlay failed" + ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
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
			__FILEREF__ + "ffmpeg: ffmpeg introOutroOverlay failed" + ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
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
			__FILEREF__ + "ffmpeg: ffmpeg introOutroOverlay failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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

void FFMpeg::introOverlay(
	string introVideoAssetPathName, int64_t introVideoDurationInMilliSeconds, string mainVideoAssetPathName, int64_t mainVideoDurationInMilliSeconds,

	int64_t introOverlayDurationInSeconds,

	bool muteIntroOverlay,

	json encodingProfileDetailsRoot,

	string stagingEncodedAssetPathName, int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::IntroOverlay;

	_logger->info(
		__FILEREF__ + "Received " + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", encodingJobKey: " + to_string(encodingJobKey) + ", introVideoAssetPathName: " + introVideoAssetPathName +
		", introVideoDurationInMilliSeconds: " + to_string(introVideoDurationInMilliSeconds) + ", mainVideoAssetPathName: " + mainVideoAssetPathName +
		", mainVideoDurationInMilliSeconds: " + to_string(mainVideoDurationInMilliSeconds) +
		", introOverlayDurationInSeconds: " + to_string(introOverlayDurationInSeconds) + ", muteIntroOverlay: " + to_string(muteIntroOverlay) +
		", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
	);

	setStatus(
		ingestionJobKey, encodingJobKey, mainVideoDurationInMilliSeconds + (introVideoDurationInMilliSeconds - introOverlayDurationInSeconds),
		mainVideoAssetPathName, stagingEncodedAssetPathName
	);

	try
	{
		if (!fs::exists(introVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", introVideoAssetPathName: " + introVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		else if (!fs::exists(mainVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", mainVideoAssetPathName: " + mainVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (encodingProfileDetailsRoot == nullptr)
		{
			string errorMessage = __FILEREF__ + "encodingProfileDetailsRoot is mandatory" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		FFMpegEncodingParameters ffmpegEncodingParameters(
			ingestionJobKey, encodingJobKey, encodingProfileDetailsRoot,
			true, // isVideo,
			-1,	  // videoTrackIndexToBeUsed,
			-1,	  // audioTrackIndexToBeUsed,
			stagingEncodedAssetPathName,
			nullptr, // videoTracksRoot,
			nullptr, // audioTracksRoot,

			_twoPasses, // out

			_ffmpegTempDir, _ffmpegTtfFontDir, _logger
		);

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
				fmt::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "introOverlay", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
		}

		/*
		ffmpeg -y -i /var/catramms/storage/MMSRepository/MMS_0000/14/000/000/001/3450777_source.mov
		   -i /var/catramms/storage/MMSRepository/MMS_0000/14/000/001/234/4251052_28.mts
		   -filter_complex
			  [0:a]volume=enable='between(t,8,12)':volume=0[intro_overlay_muted];
			  [1:v]tpad=start_duration=8:start_mode=add:color=white[main_video_moved];
			  [1:a]adelay=delays=8s:all=1[main_audio_moved];
			  [main_video_moved][0:v]overlay=eof_action=pass[final_video];
			  [main_audio_moved][intro_overlay_muted]amix=inputs=2[final_audio]
			  -map [final_video] -map [final_audio]
			  -pix_fmt yuv420p -codec:v libx264 -profile:v main -b:v 2500k -preset medium -level 4.0 -crf 22 -r 25 -threads 0 -acodec aac -b:a 160k
		-ac 2 /var/catramms/storage/IngestionRepository/users/14/4251053_introOutroOverlay.mts
		*/
		string ffmpegFilterComplex = "-filter_complex ";
		{
			long introStartOverlayInSeconds = (introVideoDurationInMilliSeconds - (introOverlayDurationInSeconds * 1000)) / 1000;
			long introVideoDurationInSeconds = introVideoDurationInMilliSeconds / 1000;

			if (introStartOverlayInSeconds < 0)
			{
				string errorMessage = __FILEREF__ + toString(_currentApiName) + ": wrong durations" +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", introStartOverlayInSeconds: " + to_string(introStartOverlayInSeconds);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (muteIntroOverlay)
				ffmpegFilterComplex += "[0:a]volume=enable='between(t," + to_string(introStartOverlayInSeconds) + "," +
									   to_string(introVideoDurationInSeconds) + ")':volume=0[intro_overlay_muted];";
			ffmpegFilterComplex +=
				"[1:v]tpad=start_duration=" + to_string(introStartOverlayInSeconds) + ":start_mode=add:color=white[main_video_moved];";
			ffmpegFilterComplex += "[1:a]adelay=delays=" + to_string(introStartOverlayInSeconds) + "s:all=1[main_audio_moved];";
			ffmpegFilterComplex += "[main_video_moved][0:v]overlay=eof_action=pass[final_video];";
			ffmpegFilterComplex += "[main_audio_moved]";
			if (muteIntroOverlay)
				ffmpegFilterComplex += "[intro_overlay_muted]";
			else
				ffmpegFilterComplex += "[0:a]";
			ffmpegFilterComplex += "amix=inputs=2[final_audio]";
		}

		vector<string> ffmpegArgumentList;
		ostringstream ffmpegArgumentListStream;

		if (ffmpegEncodingParameters._httpStreamingFileFormat != "")
		{
		}
		else
		{
			if (_twoPasses)
			{
			}
			else
			{
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");

				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(introVideoAssetPathName);
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mainVideoAssetPathName);

				// output options
				FFMpegEncodingParameters::addToArguments(ffmpegFilterComplex, ffmpegArgumentList);

				ffmpegArgumentList.push_back("-map");
				ffmpegArgumentList.push_back("[final_video]");
				ffmpegArgumentList.push_back("-map");
				ffmpegArgumentList.push_back("[final_audio]");

				ffmpegArgumentList.push_back("-pix_fmt");
				// yuv420p: the only option for broad compatibility
				ffmpegArgumentList.push_back("yuv420p");

				ffmpegEncodingParameters.applyEncoding(
					-1,	   // -1: NO two passes
					true,  // outputFileToBeAdded
					false, // videoResolutionToBeAdded
					nullptr,
					ffmpegArgumentList // out
				);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(
						__FILEREF__ + toString(_currentApiName) + ": Executing ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
						string errorMessage = __FILEREF__ + toString(_currentApiName) + ": ffmpeg command failed" +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", iReturnedStatus: " + to_string(iReturnedStatus) +
											  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + toString(_currentApiName) + " command failed" +
									   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(
						__FILEREF__ + toString(_currentApiName) + ": Executed ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
				__FILEREF__ + toString(_currentApiName) + " file generated" + ", encodingJobKey: " + to_string(encodingJobKey) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			unsigned long ulFileSize = fs::file_size(stagingEncodedAssetPathName);

			if (ulFileSize == 0)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, " + toString(_currentApiName) + " encoded file size is 0" +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "command failed, " + toString(_currentApiName) + " encoded file size is 0" +
							   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg " + toString(_currentApiName) + " failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
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
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg " + toString(_currentApiName) + " failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
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
			__FILEREF__ + "ffmpeg: ffmpeg " + toString(_currentApiName) + " failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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

void FFMpeg::outroOverlay(
	string mainVideoAssetPathName, int64_t mainVideoDurationInMilliSeconds, string outroVideoAssetPathName, int64_t outroVideoDurationInMilliSeconds,

	int64_t outroOverlayDurationInSeconds,

	bool muteOutroOverlay,

	json encodingProfileDetailsRoot,

	string stagingEncodedAssetPathName, int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::OutroOverlay;

	_logger->info(
		__FILEREF__ + "Received " + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", encodingJobKey: " + to_string(encodingJobKey) + ", mainVideoAssetPathName: " + mainVideoAssetPathName +
		", mainVideoDurationInMilliSeconds: " + to_string(mainVideoDurationInMilliSeconds) + ", outroVideoAssetPathName: " + outroVideoAssetPathName +
		", outroVideoDurationInMilliSeconds: " + to_string(outroVideoDurationInMilliSeconds) +
		", outroOverlayDurationInSeconds: " + to_string(outroOverlayDurationInSeconds) + ", muteOutroOverlay: " + to_string(muteOutroOverlay) +
		", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
	);

	setStatus(
		ingestionJobKey, encodingJobKey, mainVideoDurationInMilliSeconds + (outroVideoDurationInMilliSeconds - outroOverlayDurationInSeconds),
		mainVideoAssetPathName, stagingEncodedAssetPathName
	);

	try
	{
		if (!fs::exists(mainVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", mainVideoAssetPathName: " + mainVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		else if (!fs::exists(outroVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", outroVideoAssetPathName: " + outroVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (encodingProfileDetailsRoot == nullptr)
		{
			string errorMessage = __FILEREF__ + "encodingProfileDetailsRoot is mandatory" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		FFMpegEncodingParameters ffmpegEncodingParameters(
			ingestionJobKey, encodingJobKey, encodingProfileDetailsRoot,
			true, // isVideo,
			-1,	  // videoTrackIndexToBeUsed,
			-1,	  // audioTrackIndexToBeUsed,
			stagingEncodedAssetPathName,
			nullptr, // videoTracksRoot,
			nullptr, // audioTracksRoot,

			_twoPasses, // out

			_ffmpegTempDir, _ffmpegTtfFontDir, _logger
		);

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
				fmt::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "outroOverlay", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
		}

		/*
		ffmpeg -y
			-i /var/catramms/storage/MMSRepository/MMS_0000/14/000/001/234/4251052_28.mts
			-i /var/catramms/storage/MMSRepository/MMS_0000/14/000/000/035/3770083_source.mov
			-filter_complex
				[1:a]volume=enable='between(t,0,2)':volume=0,adelay=delays=906s:all=1[outro_audio_overlayMuted_and_moved];
				[1:v]tpad=start_duration=906:start_mode=add:color=white[outro_video_moved];
				[0:a][outro_audio_overlayMuted_and_moved]amix=inputs=2[final_audio];
				[0:v][outro_video_moved]overlay=enable='gte(t,906)'[final_video]
				-map [final_video] -map [final_audio]
				-pix_fmt yuv420p -codec:v libx264 -profile:v main -b:v 2500k -preset medium -level 4.0 -crf 22 -r 25 -threads 0 -acodec aac -b:a 160k
		-ac 2 /var/catramms/storage/IngestionRepository/users/14/4251053_introOutroOverlay.mts
		*/
		string ffmpegFilterComplex = "-filter_complex ";
		{
			long outroStartOverlayInSeconds = (mainVideoDurationInMilliSeconds / 1000) - outroOverlayDurationInSeconds;

			if (outroStartOverlayInSeconds < 0)
			{
				string errorMessage = __FILEREF__ + "introOutroOverlay: wrong durations" + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", outroStartOverlayInSeconds: " + to_string(outroStartOverlayInSeconds);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			ffmpegFilterComplex += "[1:a]";
			if (muteOutroOverlay)
				ffmpegFilterComplex += "volume=enable='between(t,0," + to_string(outroOverlayDurationInSeconds) + ")':volume=0,";
			ffmpegFilterComplex += "adelay=delays=" + to_string(outroStartOverlayInSeconds) + "s:all=1[outro_audio_overlayMuted_and_moved];";

			ffmpegFilterComplex +=
				"[1:v]tpad=start_duration=" + to_string(outroStartOverlayInSeconds) + ":start_mode=add:color=white[outro_video_moved];";

			ffmpegFilterComplex += "[0:a][outro_audio_overlayMuted_and_moved]amix=inputs=2[final_audio];";
			ffmpegFilterComplex += "[0:v][outro_video_moved]overlay=enable='gte(t," + to_string(outroStartOverlayInSeconds) + ")'[final_video]";
		}

		vector<string> ffmpegArgumentList;
		ostringstream ffmpegArgumentListStream;

		if (ffmpegEncodingParameters._httpStreamingFileFormat != "")
		{
		}
		else
		{
			if (_twoPasses)
			{
			}
			else
			{
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");

				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mainVideoAssetPathName);
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(outroVideoAssetPathName);

				// output options
				FFMpegEncodingParameters::addToArguments(ffmpegFilterComplex, ffmpegArgumentList);

				ffmpegArgumentList.push_back("-map");
				ffmpegArgumentList.push_back("[final_video]");
				ffmpegArgumentList.push_back("-map");
				ffmpegArgumentList.push_back("[final_audio]");

				ffmpegArgumentList.push_back("-pix_fmt");
				// yuv420p: the only option for broad compatibility
				ffmpegArgumentList.push_back("yuv420p");

				ffmpegEncodingParameters.applyEncoding(
					-1,	   // -1: NO two passes
					true,  // outputFileToBeAdded
					false, // videoResolutionToBeAdded
					nullptr,
					ffmpegArgumentList // out
				);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(
						__FILEREF__ + "introOutroOverlay: Executing ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
						string errorMessage = __FILEREF__ + toString(_currentApiName) + ": ffmpeg command failed" +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", iReturnedStatus: " + to_string(iReturnedStatus) +
											  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + toString(_currentApiName) + " command failed" +
									   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(
						__FILEREF__ + toString(_currentApiName) + ": Executed ffmpeg command" + ", encodingJobKey: " + to_string(encodingJobKey) +
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
				__FILEREF__ + toString(_currentApiName) + " file generated" + ", encodingJobKey: " + to_string(encodingJobKey) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			unsigned long ulFileSize = fs::file_size(stagingEncodedAssetPathName);

			if (ulFileSize == 0)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, " + toString(_currentApiName) + " encoded file size is 0" +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "command failed, " + toString(_currentApiName) + " encoded file size is 0" +
							   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg " + toString(_currentApiName) + " failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
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
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "ffmpeg: ffmpeg " + toString(_currentApiName) + " failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
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
			__FILEREF__ + "ffmpeg: ffmpeg " + toString(_currentApiName) + " failed" + ", encodingJobKey: " + to_string(encodingJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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
