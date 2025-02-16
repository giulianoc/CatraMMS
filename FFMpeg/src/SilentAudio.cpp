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

void FFMpeg::silentAudio(
	string videoAssetPathName, int64_t videoDurationInMilliSeconds,

	string addType, // entireTrack, begin, end
	int seconds,

	json encodingProfileDetailsRoot,

	string stagingEncodedAssetPathName, int64_t encodingJobKey, int64_t ingestionJobKey, pid_t *pChildPid
)
{
	int iReturnedStatus = 0;

	_currentApiName = APIName::SilentAudio;

	SPDLOG_INFO(
		"Received {}"
		", ingestionJobKey: {}"
		", encodingJobKey: {}",
		toString(_currentApiName), ingestionJobKey, encodingJobKey
	);

	setStatus(ingestionJobKey, encodingJobKey, videoDurationInMilliSeconds, videoAssetPathName, stagingEncodedAssetPathName);

	try
	{
		if (!fs::exists(videoAssetPathName))
		{
			string errorMessage = std::format(
				"video asset path name not existing"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", videoAssetPathName: {}",
				ingestionJobKey, encodingJobKey, videoAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

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

			_ffmpegTempDir, _ffmpegTtfFontDir
		);

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
				std::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "silentAudio", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
				*/
			_outputFfmpegPathFileName = std::format(
				"{}/{}_{}_{}_{:0>4}-{:0>2}-{:0>2}-{:0>2}-{:0>2}-{:0>2}.log", _ffmpegTempDir, "silentAudio", _currentIngestionJobKey,
				_currentEncodingJobKey, tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday, tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
			);
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
				if (addType == "entireTrack")
				{
					/*
					add entire track:

					Se si aggiunge una traccia audio 'silent', non ha senso avere altre tracce audio, per cui il comando
					ignora eventuali tracce audio nel file originale a aggiunge la traccia audio silent

					-f lavfi -i anullsrc genera una sorgente audio virtuale con silenzio di lunghezza infinita. Ecco perché è
					importante specificare -shortest per limitare la durata dell'output alla durata del flusso video.
					In caso contrario, verrebbe creato un file di output infinito.

					ffmpeg -f lavfi -i anullsrc -i video.mov -c:v copy -c:a aac -map 0:a -map 1:v -shortest output.mp4
					*/

					ffmpegArgumentList.push_back("ffmpeg");
					// global options
					ffmpegArgumentList.push_back("-y");

					// input options
					ffmpegArgumentList.push_back("-f");
					ffmpegArgumentList.push_back("lavfi");
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back("anullsrc");
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(videoAssetPathName);

					// output options
					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back("0:a");
					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back("1:v");

					ffmpegArgumentList.push_back("-shortest");

					if (encodingProfileDetailsRoot != nullptr)
					{
						ffmpegEncodingParameters.applyEncoding(
							-1,	  // -1: NO two passes
							true, // outputFileToBeAdded
							true, // videoResolutionToBeAdded
							nullptr,
							ffmpegArgumentList // out
						);
					}
					else
					{
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("aac"); // default

						ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
					}
				}
				else if (addType == "begin")
				{
					/*
					begin:
					ffmpeg -i video.mov -af "adelay=1s:all=true" -c:v copy -c:a aac output.mp4
					*/
					ffmpegArgumentList.push_back("ffmpeg");
					// global options
					ffmpegArgumentList.push_back("-y");

					// input options
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(videoAssetPathName);

					// output options
					ffmpegArgumentList.push_back("-af");
					ffmpegArgumentList.push_back("adelay=" + to_string(seconds) + "s:all=true");

					if (encodingProfileDetailsRoot != nullptr)
					{
						ffmpegEncodingParameters.applyEncoding(
							-1,	  // -1: NO two passes
							true, // outputFileToBeAdded
							true, // videoResolutionToBeAdded
							nullptr,
							ffmpegArgumentList // out
						);
					}
					else
					{
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("aac"); // default

						ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
					}
				}
				else // if (addType == "end")
				{
					/*
					end:
					ffmpeg -i video.mov -af "apad=pad_dur=1" -c:v copy -c:a aac output.mp4
					*/
					ffmpegArgumentList.push_back("ffmpeg");
					// global options
					ffmpegArgumentList.push_back("-y");

					// input options
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(videoAssetPathName);

					// output options
					ffmpegArgumentList.push_back("-af");
					ffmpegArgumentList.push_back("apad=pad_dur=" + to_string(seconds));

					if (encodingProfileDetailsRoot != nullptr)
					{
						ffmpegEncodingParameters.applyEncoding(
							-1,	  // -1: NO two passes
							true, // outputFileToBeAdded
							true, // videoResolutionToBeAdded
							nullptr,
							ffmpegArgumentList // out
						);
					}
					else
					{
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("aac"); // default

						ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
					}
				}

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

					SPDLOG_INFO(
						"{}: Executing ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", ffmpegArgumentList: {}",
						toString(_currentApiName), encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str()
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
							"{}: ffmpeg command failed"
							", encodingJobKey: {}"
							", ingestionJobKey: {}"
							", iReturnedStatus: {}"
							", ffmpegArgumentList: {}",
							toString(_currentApiName), encodingJobKey, ingestionJobKey, iReturnedStatus, ffmpegArgumentListStream.str()
						);

						// to hide the ffmpeg staff
						string errorMessage = std::format(
							"{} command failed"
							", encodingJobKey: {}"
							", ingestionJobKey: {}",
							toString(_currentApiName), encodingJobKey, ingestionJobKey
						);
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					SPDLOG_INFO(
						"{}: Executed ffmpeg command"
						", encodingJobKey: {}"
						", ingestionJobKey: {}"
						", ffmpegArgumentList: {}"
						", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @{}@",
						toString(_currentApiName), encodingJobKey, ingestionJobKey, ffmpegArgumentListStream.str(),
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
				"{} file generated"
				", encodingJobKey: {}"
				", ingestionJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				toString(_currentApiName), encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName
			);

			unsigned long ulFileSize = fs::file_size(stagingEncodedAssetPathName);

			if (ulFileSize == 0)
			{
				SPDLOG_INFO(
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
			"{} ffmpeg failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", stagingEncodedAssetPathName: {}",
			toString(_currentApiName), encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName + ", e.what(): " + e.what()
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
			"{} ffmpeg failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", stagingEncodedAssetPathName: {}",
			toString(_currentApiName), encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName + ", e.what(): " + e.what()
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
			"{} ffmpeg failed"
			", encodingJobKey: {}"
			", ingestionJobKey: {}"
			", stagingEncodedAssetPathName: {}",
			toString(_currentApiName), encodingJobKey, ingestionJobKey, stagingEncodedAssetPathName + ", e.what(): " + e.what()
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
