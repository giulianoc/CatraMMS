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
#include "JSONUtils.h"
#include "catralibraries/ProcessUtility.h"
#include <fstream>
/*
#include "FFMpegFilters.h"
#include "MMSCURL.h"
#include "catralibraries/StringUtils.h"
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
*/

void FFMpeg::slideShow(
	int64_t ingestionJobKey, int64_t encodingJobKey, float durationOfEachSlideInSeconds, string frameRateMode, json encodingProfileDetailsRoot,
	vector<string> &imagesSourcePhysicalPaths, vector<string> &audiosSourcePhysicalPaths,
	float shortestAudioDurationInSeconds, // the shortest duration among the audios
	string encodedStagingAssetPathName, pid_t *pChildPid
)
{
	_currentApiName = APIName::SlideShow;

	// IN CASE WE HAVE AUDIO
	//	We will stop the video at the shortest between
	//		- the duration of the slide show (sum of the duration of the images)
	//		- shortest audio
	//
	//	So, if the duration of the picture is longest than the duration of the shortest audio
	//			we have to reduce the duration of the pictures (1)
	//	    if the duration of the shortest audio is longest than the duration of the pictures
	//			we have to increase the duration of the last pictures (2)

	// CAPIRE COME MAI LA PERCENTUALE E' SEMPRE ZERO. Eliminare videoDurationInSeconds se non serve
	int64_t videoDurationInSeconds;
	if (audiosSourcePhysicalPaths.size() > 0)
	{
		if (durationOfEachSlideInSeconds * imagesSourcePhysicalPaths.size() < shortestAudioDurationInSeconds)
			videoDurationInSeconds = durationOfEachSlideInSeconds * imagesSourcePhysicalPaths.size();
		else
			videoDurationInSeconds = shortestAudioDurationInSeconds;
	}
	else
		videoDurationInSeconds = durationOfEachSlideInSeconds * imagesSourcePhysicalPaths.size();

	setStatus(
		ingestionJobKey, encodingJobKey, videoDurationInSeconds * 1000
		/*
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	_logger->info(
		__FILEREF__ + "Received " + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " +
		to_string(encodingJobKey) + ", frameRateMode: " + frameRateMode + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName +
		", durationOfEachSlideInSeconds: " + to_string(durationOfEachSlideInSeconds) + ", shortestAudioDurationInSeconds: " +
		to_string(shortestAudioDurationInSeconds) + ", videoDurationInSeconds: " + to_string(videoDurationInSeconds)
	);

	int iReturnedStatus = 0;

	string slideshowListImagesPathName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + ".slideshowListImages.txt";

	{
		ofstream slideshowListFile(slideshowListImagesPathName.c_str(), ofstream::trunc);
		string lastSourcePhysicalPath;
		for (int imageIndex = 0; imageIndex < imagesSourcePhysicalPaths.size(); imageIndex++)
		{
			string sourcePhysicalPath = imagesSourcePhysicalPaths[imageIndex];
			double slideDurationInSeconds;

			if (!fs::exists(sourcePhysicalPath))
			{
				string errorMessage = string("Source asset path name not existing") + ", ingestionJobKey: " +
									  to_string(ingestionJobKey)
									  // + ", encodingJobKey: " + to_string(encodingJobKey)
									  + ", sourcePhysicalPath: " + sourcePhysicalPath;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			if (audiosSourcePhysicalPaths.size() > 0)
			{
				if (imageIndex + 1 >= imagesSourcePhysicalPaths.size() &&
					durationOfEachSlideInSeconds * (imageIndex + 1) < shortestAudioDurationInSeconds)
				{
					// we are writing the last image and the duration of all the slides
					// is less than the shortest audio duration (2)
					slideDurationInSeconds = shortestAudioDurationInSeconds - (durationOfEachSlideInSeconds * imageIndex);
				}
				else
				{
					// check case (1)

					if (durationOfEachSlideInSeconds * (imageIndex + 1) <= shortestAudioDurationInSeconds)
						slideDurationInSeconds = durationOfEachSlideInSeconds;
					else if (durationOfEachSlideInSeconds * (imageIndex + 1) > shortestAudioDurationInSeconds)
					{
						// if we are behind shortestAudioDurationInSeconds, we have to add
						// the remaining secondsand we have to terminate (next 'if' checks if before
						// we were behind just break)
						if (durationOfEachSlideInSeconds * imageIndex >= shortestAudioDurationInSeconds)
							break;
						else
							slideDurationInSeconds = (durationOfEachSlideInSeconds * (imageIndex + 1)) - shortestAudioDurationInSeconds;
					}
				}
			}
			else
				slideDurationInSeconds = durationOfEachSlideInSeconds;

			slideshowListFile << "file '" << sourcePhysicalPath << "'" << endl;
			_logger->info(
				__FILEREF__ + "slideShow" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
				", line: " + ("file '" + sourcePhysicalPath + "'")
			);
			slideshowListFile << "duration " << slideDurationInSeconds << endl;
			_logger->info(
				__FILEREF__ + "slideShow" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
				", line: " + ("duration " + to_string(slideDurationInSeconds))
			);

			lastSourcePhysicalPath = sourcePhysicalPath;
		}
		slideshowListFile << "file '" << lastSourcePhysicalPath << "'" << endl;
		_logger->info(
			__FILEREF__ + "slideShow" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
			", line: " + ("file '" + lastSourcePhysicalPath + "'")
		);
		slideshowListFile.close();
	}

	string slideshowListAudiosPathName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + ".slideshowListAudios.txt";

	if (audiosSourcePhysicalPaths.size() > 1)
	{
		ofstream slideshowListFile(slideshowListAudiosPathName.c_str(), ofstream::trunc);
		string lastSourcePhysicalPath;
		for (string sourcePhysicalPath : audiosSourcePhysicalPaths)
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

			slideshowListFile << "file '" << sourcePhysicalPath << "'" << endl;

			lastSourcePhysicalPath = sourcePhysicalPath;
		}
		slideshowListFile << "file '" << lastSourcePhysicalPath << "'" << endl;
		slideshowListFile.close();
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

				ffmpegAudioCodecParameter, ffmpegAudioOtherParameters, ffmpegAudioChannelsParameter, ffmpegAudioSampleRateParameter, audioBitRatesInfo
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
				string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", twoPasses: " + to_string(twoPasses)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
				*/
				twoPasses = false;

				string errorMessage = __FILEREF__ +
									  "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to false" +
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
								  ", e.what(): " + e.what() + ", encodingProfileDetailsRoot: " + JSONUtils::toString(encodingProfileDetailsRoot);
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

		_outputFfmpegPathFileName = fmt::format("{}/{}_{}_{}.log", _ffmpegTempDir, "slideShow", _currentIngestionJobKey, sUtcTimestamp);
	}

	// Then you can stream copy or re-encode your files
	// The -safe 0 above is not required if the paths are relative
	// ffmpeg -f concat -safe 0 -i mylist.txt -c copy output

	// https://www.imakewebsites.ca/posts/2016/10/30/ffmpeg-concatenating-with-image-sequences-and-audio/
	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;

	ffmpegArgumentList.push_back("ffmpeg");
	{
		ffmpegArgumentList.push_back("-f");
		ffmpegArgumentList.push_back("concat");
		ffmpegArgumentList.push_back("-safe");
		ffmpegArgumentList.push_back("0");
		ffmpegArgumentList.push_back("-i");
		ffmpegArgumentList.push_back(slideshowListImagesPathName);
	}
	if (audiosSourcePhysicalPaths.size() == 1)
	{
		ffmpegArgumentList.push_back("-i");
		ffmpegArgumentList.push_back(audiosSourcePhysicalPaths[0]);
	}
	else if (audiosSourcePhysicalPaths.size() > 1)
	{
		ffmpegArgumentList.push_back("-f");
		ffmpegArgumentList.push_back("concat");
		ffmpegArgumentList.push_back("-safe");
		ffmpegArgumentList.push_back("0");
		ffmpegArgumentList.push_back("-i");
		ffmpegArgumentList.push_back(slideshowListAudiosPathName);
	}

	// encoding parameters
	if (encodingProfileDetailsRoot != nullptr)
	{
		for (string parameter : ffmpegEncodingProfileArgumentList)
			FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);
	}
	else
	{
		ffmpegArgumentList.push_back("-c:v");
		ffmpegArgumentList.push_back("libx264");
		ffmpegArgumentList.push_back("-r");
		ffmpegArgumentList.push_back("25");
	}

	ffmpegArgumentList.push_back("-fps_mode");
	ffmpegArgumentList.push_back(frameRateMode);
	ffmpegArgumentList.push_back("-pix_fmt");
	// yuv420p: the only option for broad compatibility
	ffmpegArgumentList.push_back("yuv420p");
	if (audiosSourcePhysicalPaths.size() > 0)
		ffmpegArgumentList.push_back("-shortest");
	ffmpegArgumentList.push_back(encodedStagingAssetPathName);

	try
	{
		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

		_logger->info(
			__FILEREF__ + "slideShow: Executing ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
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
			string errorMessage = __FILEREF__ + "slideShow: ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", iReturnedStatus: " + to_string(iReturnedStatus) + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "slideShow: command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey);
			throw runtime_error(errorMessage);
		}

		chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "slideShow: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ffmpegArgumentList: " + ffmpegArgumentListStream.str() + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" +
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
						   ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName + ", encodingJobKey: " + to_string(encodingJobKey) +
						   ", ingestionJobKey: " + to_string(ingestionJobKey) + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
						   ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
		else
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
						   ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						   ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile +
						   ", e.what(): " + e.what();
		_logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		fs::remove_all(_outputFfmpegPathFileName);

		if (fs::exists(slideshowListImagesPathName.c_str()))
		{
			_logger->info(__FILEREF__ + "Remove" + ", slideshowListImagesPathName: " + slideshowListImagesPathName);
			fs::remove_all(slideshowListImagesPathName);
		}
		if (fs::exists(slideshowListAudiosPathName.c_str()))
		{
			_logger->info(__FILEREF__ + "Remove" + ", slideshowListAudiosPathName: " + slideshowListAudiosPathName);
			fs::remove_all(slideshowListAudiosPathName);
		}

		if (iReturnedStatus == 9) // 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else
			throw e;
	}

	_logger->info(__FILEREF__ + "Remove" + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
	fs::remove_all(_outputFfmpegPathFileName);

	if (fs::exists(slideshowListImagesPathName.c_str()))
	{
		_logger->info(__FILEREF__ + "Remove" + ", slideshowListImagesPathName: " + slideshowListImagesPathName);
		fs::remove_all(slideshowListImagesPathName);
	}
	if (fs::exists(slideshowListAudiosPathName.c_str()))
	{
		_logger->info(__FILEREF__ + "Remove" + ", slideshowListAudiosPathName: " + slideshowListAudiosPathName);
		fs::remove_all(slideshowListAudiosPathName);
	}
}
