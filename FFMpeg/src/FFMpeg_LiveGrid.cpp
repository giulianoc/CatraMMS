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
#include "catralibraries/ProcessUtility.h"
#include <fstream>
/*
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "MMSCURL.h"
#include "catralibraries/StringUtils.h"
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
*/

void FFMpeg::liveGrid(
	int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder, string userAgent,
	json inputChannelsRoot, // name,url
	int gridColumns,
	int gridWidth,	// i.e.: 1024
	int gridHeight, // i.e.: 578

	json outputsRoot,

	pid_t *pChildPid
)
{
	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	string segmentListPath;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;
	time_t utcNow;

	_currentApiName = APIName::LiveGrid;

	setStatus(
		ingestionJobKey, encodingJobKey
		/*
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	try
	{
		_logger->info(
			__FILEREF__ + "Received " + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey)
		);

		// gestiamo solamente un outputsRoot
		if (outputsRoot.size() != 1)
		{
			string errorMessage = __FILEREF__ + toString(_currentApiName) + ". Wrong output parameters" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
								  ", outputsRoot.size: " + to_string(outputsRoot.size());
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegOutputArgumentList;
		try
		{
			_logger->info(
				__FILEREF__ + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", outputsRoot.size: " + to_string(outputsRoot.size())
			);
			outputsRootToFfmpeg(
				ingestionJobKey, encodingJobKey, externalEncoder,
				"",		 // otherOutputOptionsBecauseOfMaxWidth,
				nullptr, // inputDrawTextDetailsRoot,
				// inputVideoTracks, inputAudioTracks,
				-1, // streamingDurationInSeconds,
				outputsRoot, ffmpegOutputArgumentList
			);

			{
				ostringstream ffmpegOutputArgumentListStream;
				if (!ffmpegOutputArgumentList.empty())
					copy(
						ffmpegOutputArgumentList.begin(), ffmpegOutputArgumentList.end(),
						ostream_iterator<string>(ffmpegOutputArgumentListStream, " ")
					);
				_logger->info(
					__FILEREF__ + toString(_currentApiName) + ": ffmpegOutputArgumentList" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", ffmpegOutputArgumentList: " + ffmpegOutputArgumentListStream.str()
				);
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = __FILEREF__ + toString(_currentApiName) + ". Wrong output parameters" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
								  ", exception: " + e.what();
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = __FILEREF__ + toString(_currentApiName) + ". Wrong output parameters" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
								  ", exception: " + e.what();
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
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
				fmt::format("{}/{}_{}_{}_{}.log", _ffmpegTempDir, "liveGrid", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp);
		}

		/*
			option 1 (using overlay/pad)
			ffmpeg \
				-i https://1673829767.rsc.cdn77.org/1673829767/index.m3u8 \
				-i https://1696829226.rsc.cdn77.org/1696829226/index.m3u8 \
				-i https://1681769566.rsc.cdn77.org/1681769566/index.m3u8 \
				-i https://1452709105.rsc.cdn77.org/1452709105/index.m3u8 \
				-filter_complex \
				"[0:v]                 pad=width=$X:height=$Y                  [background]; \
				 [0:v]                 scale=width=$X/2:height=$Y/2            [1]; \
				 [1:v]                 scale=width=$X/2:height=$Y/2            [2]; \
				 [2:v]                 scale=width=$X/2:height=$Y/2            [3]; \
				 [3:v]                 scale=width=$X/2:height=$Y/2            [4]; \
				 [background][1]       overlay=shortest=1:x=0:y=0              [background+1];
				 [background+1][2]     overlay=shortest=1:x=$X/2:y=0           [1+2];
				 [1+2][3]              overlay=shortest=1:x=0:y=$Y/2           [1+2+3];
				 [1+2+3][4]            overlay=shortest=1:x=$X/2:y=$Y/2        [1+2+3+4]
				" -map "[1+2+3+4]" -c:v:0 libx264 \
				-map 0:a -c:a aac \
				-map 1:a -c:a aac \
				-map 2:a -c:a aac \
				-map 3:a -c:a aac \
				-t 30 multiple_input_grid.mp4

			option 2: using hstack/vstack (faster than overlay/pad)
			ffmpeg \
				-i https://1673829767.rsc.cdn77.org/1673829767/index.m3u8 \
				-i https://1696829226.rsc.cdn77.org/1696829226/index.m3u8 \
				-i https://1681769566.rsc.cdn77.org/1681769566/index.m3u8 \
				-i https://1452709105.rsc.cdn77.org/1452709105/index.m3u8 \
				-filter_complex \
				"[0:v]                  scale=width=$X/2:height=$Y/2            [0v]; \
				 [1:v]                  scale=width=$X/2:height=$Y/2            [1v]; \
				 [2:v]                  scale=width=$X/2:height=$Y/2            [2v]; \
				 [3:v]                  scale=width=$X/2:height=$Y/2            [3v]; \
				 [0v][1v]               hstack=inputs=2:shortest=1              [0r]; \	#r sta per row
				 [2v][3v]               hstack=inputs=2:shortest=1              [1r]; \
				 [0r][1r]               vstack=inputs=2:shortest=1              [0r+1r]
				 " -map "[0r+1r]" -codec:v libx264 -b:v 800k -preset veryfast -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags
		delete_segments -hls_start_number_source datetime -start_number 10 -hls_segment_filename
		/var/catramms/storage/MMSRepository-free/1/test/low/test_%04d.ts -f hls /var/catramms/storage/MMSRepository-free/1/test/low/test.m3u8 \
				-map 0:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments
		-hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/tv1/test_%04d.ts -f
		hls /var/catramms/storage/MMSRepository-free/1/test/tv1/test.m3u8 \
				-map 1:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments
		-hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/tv2/test_%04d.ts -f
		hls /var/catramms/storage/MMSRepository-free/1/test/tv2/test.m3u8 \
				-map 2:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments
		-hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/tv3/test_%04d.ts -f
		hls /var/catramms/storage/MMSRepository-free/1/test/tv3/test.m3u8 \ -map 3:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4
		-hls_delete_threshold 1 -hls_flags delete_segments -hls_start_number_source datetime -start_number 10 -hls_segment_filename
		/var/catramms/storage/MMSRepository-free/1/test/tv4/test_%04d.ts -f hls /var/catramms/storage/MMSRepository-free/1/test/tv4/test.m3u8

		In case of output SRT:
			ffmpeg \
				-i https://1673829767.rsc.cdn77.org/1673829767/index.m3u8 \
				-i https://1696829226.rsc.cdn77.org/1696829226/index.m3u8 \
				-i https://1681769566.rsc.cdn77.org/1681769566/index.m3u8 \
				-i https://1452709105.rsc.cdn77.org/1452709105/index.m3u8 \
				-filter_complex \
				"[0:v]                  scale=width=$X/2:height=$Y/2            [0v]; \
				 [1:v]                  scale=width=$X/2:height=$Y/2            [1v]; \
				 [2:v]                  scale=width=$X/2:height=$Y/2            [2v]; \
				 [3:v]                  scale=width=$X/2:height=$Y/2            [3v]; \
				 [0v][1v]               hstack=inputs=2:shortest=1              [0r]; \	#r sta per row
				 [2v][3v]               hstack=inputs=2:shortest=1              [1r]; \
				 [0r][1r]               vstack=inputs=2:shortest=1              [0r+1r]
				 " -map "[0r+1r]" -codec:v libx264 -b:v 800k -preset veryfast \
				-map 0:a -acodec aac -b:a 92k -ac 2 \
				-map 1:a -acodec aac -b:a 92k -ac 2 \
				-map 2:a -acodec aac -b:a 92k -ac 2 \
				-map 3:a -acodec aac -b:a 92k -ac 2 \
				-f mpegts "srt://Video-ret.srgssr.ch:32010?pkt_size=1316&mode=caller"
		 */
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		int inputChannelsNumber = inputChannelsRoot.size();

		ffmpegArgumentList.push_back("ffmpeg");
		// -re (input) Read input at native frame rate. By default ffmpeg attempts to read the input(s)
		//		as fast as possible. This option will slow down the reading of the input(s)
		//		to the native frame rate of the input(s). It is useful for real-time output
		//		(e.g. live streaming).
		// -hls_flags append_list: Append new segments into the end of old segment list
		//		and remove the #EXT-X-ENDLIST from the old segment list
		// -hls_time seconds: Set the target segment length in seconds. Segment will be cut on the next key frame
		//		after this time has passed.
		// -hls_list_size size: Set the maximum number of playlist entries. If set to 0 the list file
		//		will contain all the segments. Default value is 5.
		if (userAgent != "")
		{
			ffmpegArgumentList.push_back("-user_agent");
			ffmpegArgumentList.push_back(userAgent);
		}
		ffmpegArgumentList.push_back("-re");
		for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsNumber; inputChannelIndex++)
		{
			json inputChannelRoot = inputChannelsRoot[inputChannelIndex];
			string inputChannelURL = JSONUtils::asString(inputChannelRoot, "inputChannelURL", "");

			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(inputChannelURL);
		}
		int gridRows = inputChannelsNumber / gridColumns;
		if (inputChannelsNumber % gridColumns != 0)
			gridRows += 1;
		{
			string ffmpegFilterComplex;

			// [0:v]                  scale=width=$X/2:height=$Y/2            [0v];
			int scaleWidth = gridWidth / gridColumns;
			int scaleHeight = gridHeight / gridRows;

			// some codecs, like h264, requires even total width/heigth
			bool evenTotalWidth = true;
			if ((scaleWidth * gridColumns) % 2 != 0)
				evenTotalWidth = false;

			bool evenTotalHeight = true;
			if ((scaleHeight * gridRows) % 2 != 0)
				evenTotalHeight = false;

			for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsNumber; inputChannelIndex++)
			{
				bool lastColumn;
				if ((inputChannelIndex + 1) % gridColumns == 0)
					lastColumn = true;
				else
					lastColumn = false;

				bool lastRow;
				{
					int startChannelIndexOfLastRow = inputChannelsNumber / gridColumns;
					if (inputChannelsNumber % gridColumns == 0)
						startChannelIndexOfLastRow--;
					startChannelIndexOfLastRow *= gridColumns;

					if (inputChannelIndex >= startChannelIndexOfLastRow)
						lastRow = true;
					else
						lastRow = false;
				}

				int width;
				if (!evenTotalWidth && lastColumn)
					width = scaleWidth + 1;
				else
					width = scaleWidth;

				int height;
				if (!evenTotalHeight && lastRow)
					height = scaleHeight + 1;
				else
					height = scaleHeight;

				/*
				_logger->info(__FILEREF__ + "Widthhhhhhh"
					+ ", inputChannelIndex: " + to_string(inputChannelIndex)
					+ ", gridWidth: " + to_string(gridWidth)
					+ ", gridColumns: " + to_string(gridColumns)
					+ ", evenTotalWidth: " + to_string(evenTotalWidth)
					+ ", lastColumn: " + to_string(lastColumn)
					+ ", scaleWidth: " + to_string(scaleWidth)
					+ ", width: " + to_string(width)
				);

				_logger->info(__FILEREF__ + "Heightttttttt"
					+ ", inputChannelIndex: " + to_string(inputChannelIndex)
					+ ", gridHeight: " + to_string(gridHeight)
					+ ", gridRows: " + to_string(gridRows)
					+ ", evenTotalHeight: " + to_string(evenTotalHeight)
					+ ", lastRow: " + to_string(lastRow)
					+ ", scaleHeight: " + to_string(scaleHeight)
					+ ", height: " + to_string(height)
				);
				*/

				ffmpegFilterComplex +=
					("[" + to_string(inputChannelIndex) + ":v]" + "scale=width=" + to_string(width) + ":height=" + to_string(height) + "[" +
					 to_string(inputChannelIndex) + "v];");
			}
			// [0v][1v]               hstack=inputs=2:shortest=1              [0r]; #r sta per row
			for (int gridRowIndex = 0, inputChannelIndex = 0; gridRowIndex < gridRows; gridRowIndex++)
			{
				int columnsIntoTheRow;
				if (gridRowIndex + 1 < gridRows)
				{
					// it is not the last row --> we have all the columns
					columnsIntoTheRow = gridColumns;
				}
				else
				{
					if (inputChannelsNumber % gridColumns != 0)
						columnsIntoTheRow = inputChannelsNumber % gridColumns;
					else
						columnsIntoTheRow = gridColumns;
				}
				for (int gridColumnIndex = 0; gridColumnIndex < columnsIntoTheRow; gridColumnIndex++)
					ffmpegFilterComplex += ("[" + to_string(inputChannelIndex++) + "v]");

				ffmpegFilterComplex += ("hstack=inputs=" + to_string(columnsIntoTheRow) + ":shortest=1");

				if (gridRows == 1 && gridRowIndex == 0)
				{
					// in case there is just one row, vstack has NOT to be added
					ffmpegFilterComplex += ("[outVideo]");
				}
				else
				{
					ffmpegFilterComplex += ("[" + to_string(gridRowIndex) + "r];");
				}
			}

			if (gridRows > 1)
			{
				// [0r][1r]               vstack=inputs=2:shortest=1              [outVideo]
				for (int gridRowIndex = 0, inputChannelIndex = 0; gridRowIndex < gridRows; gridRowIndex++)
					ffmpegFilterComplex += ("[" + to_string(gridRowIndex) + "r]");
				ffmpegFilterComplex += ("vstack=inputs=" + to_string(gridRows) + ":shortest=1[outVideo]");
			}

			ffmpegArgumentList.push_back("-filter_complex");
			ffmpegArgumentList.push_back(ffmpegFilterComplex);
		}

		for (string parameter : ffmpegOutputArgumentList)
			ffmpegArgumentList.push_back(parameter);

		/*
				int videoBitRateInKbps = -1;
				{
					string httpStreamingFileFormat;
					string ffmpegHttpStreamingParameter = "";

					string ffmpegFileFormatParameter = "";

					string ffmpegVideoCodecParameter = "";
					string ffmpegVideoProfileParameter = "";
					string ffmpegVideoResolutionParameter = "";
					string ffmpegVideoBitRateParameter = "";
					string ffmpegVideoOtherParameters = "";
					string ffmpegVideoMaxRateParameter = "";
					string ffmpegVideoBufSizeParameter = "";
					string ffmpegVideoFrameRateParameter = "";
					string ffmpegVideoKeyFramesRateParameter = "";
					vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

					string ffmpegAudioCodecParameter = "";
					string ffmpegAudioBitRateParameter = "";
					string ffmpegAudioOtherParameters = "";
					string ffmpegAudioChannelsParameter = "";
					string ffmpegAudioSampleRateParameter = "";
					vector<string> audioBitRatesInfo;


					_currentlyAtSecondPass = false;

					// we will set by default _twoPasses to false otherwise, since the ffmpeg class is reused
					// it could remain set to true from a previous call
					_twoPasses = false;

					FFMpegEncodingParameters::settingFfmpegParameters(
						_logger,
						encodingProfileDetailsRoot,
						true,	// isVideo,

						httpStreamingFileFormat,
						ffmpegHttpStreamingParameter,

						ffmpegFileFormatParameter,

						ffmpegVideoCodecParameter,
						ffmpegVideoProfileParameter,
						ffmpegVideoOtherParameters,
						_twoPasses,
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

					// -map for video and audio
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back("[outVideo]");

						FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(ffmpegVideoResolutionParameter, ffmpegArgumentList);
						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");

						if (outputTypeLowerCase == "hls")
						{
							ffmpegArgumentList.push_back("-hls_time");
							ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));

							ffmpegArgumentList.push_back("-hls_list_size");
							ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

							// Segment files removed from the playlist are deleted after a period of time
							// equal to the duration of the segment plus the duration of the playlist
							ffmpegArgumentList.push_back("-hls_flags");
							ffmpegArgumentList.push_back("delete_segments");

							// Set the number of unreferenced segments to keep on disk
							// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
							// to download segments which were recently referenced in the playlist.
							// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
							ffmpegArgumentList.push_back("-hls_delete_threshold");
							ffmpegArgumentList.push_back(to_string(1));

							// 2020-07-11: without -hls_start_number_source we have video-audio out of sync
							ffmpegArgumentList.push_back("-hls_start_number_source");
							ffmpegArgumentList.push_back("datetime");

							ffmpegArgumentList.push_back("-start_number");
							ffmpegArgumentList.push_back(to_string(10));

							{
								string videoTrackDirectoryName = "0_video";

								string segmentPathFileName =
									manifestDirectoryPath
									+ "/"
									+ videoTrackDirectoryName
									+ "/"
									+ to_string(ingestionJobKey)
									+ "_"
									+ to_string(encodingJobKey)
									+ "_%04d.ts"
								;
								ffmpegArgumentList.push_back("-hls_segment_filename");
								ffmpegArgumentList.push_back(segmentPathFileName);

								ffmpegArgumentList.push_back("-f");
								ffmpegArgumentList.push_back("hls");

								string manifestFilePathName =
									manifestDirectoryPath
									+ "/"
									+ videoTrackDirectoryName
									+ "/"
									+ manifestFileName
								;
								ffmpegArgumentList.push_back(manifestFilePathName);
							}
						}
						else if (outputTypeLowerCase == "dash")
						{
							// non so come si deve gestire nel caso di multi audio con DASH
						}

						for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsNumber; inputChannelIndex++)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								to_string(inputChannelIndex) + ":a");

							FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

							if (outputTypeLowerCase == "hls")
							{
								{
									ffmpegArgumentList.push_back("-hls_time");
									ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));

									ffmpegArgumentList.push_back("-hls_list_size");
									ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

									// Segment files removed from the playlist are deleted after a period of time
									// equal to the duration of the segment plus the duration of the playlist
									ffmpegArgumentList.push_back("-hls_flags");
									ffmpegArgumentList.push_back("delete_segments");

									// Set the number of unreferenced segments to keep on disk
									// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
									// to download segments which were recently referenced in the playlist.
									// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
									ffmpegArgumentList.push_back("-hls_delete_threshold");
									ffmpegArgumentList.push_back(to_string(1));

									// 2020-07-11: without -hls_start_number_source we have video-audio out of sync
									ffmpegArgumentList.push_back("-hls_start_number_source");
									ffmpegArgumentList.push_back("datetime");

									ffmpegArgumentList.push_back("-start_number");
									ffmpegArgumentList.push_back(to_string(10));
								}

								string audioTrackDirectoryName = to_string(inputChannelIndex) + "_audio";

								{
									string segmentPathFileName =
										manifestDirectoryPath
										+ "/"
										+ audioTrackDirectoryName
										+ "/"
										+ to_string(ingestionJobKey)
										+ "_"
										+ to_string(encodingJobKey)
										+ "_%04d.ts"
									;
									ffmpegArgumentList.push_back("-hls_segment_filename");
									ffmpegArgumentList.push_back(segmentPathFileName);

									ffmpegArgumentList.push_back("-f");
									ffmpegArgumentList.push_back("hls");

									string manifestFilePathName =
										manifestDirectoryPath
										+ "/"
										+ audioTrackDirectoryName
										+ "/"
										+ manifestFileName
									;
									ffmpegArgumentList.push_back(manifestFilePathName);
								}
							}
							else if (outputTypeLowerCase == "dash")
							{
								 // non so come si deve gestire nel caso di multi audio con DASH
							}
						}

						if (outputTypeLowerCase == "srt")
						{
							ffmpegArgumentList.push_back("-f");
							ffmpegArgumentList.push_back("mpegts");
							ffmpegArgumentList.push_back(srtURL);
						}
					}
				}
		*/

		json outputRoot = outputsRoot[0];

		string outputType = JSONUtils::asString(outputRoot, "outputType", "");

		// We will create:
		//  - one m3u8 for each track (video and audio)
		//  - one main m3u8 having a group for AUDIO
		if (outputType == "HLS_Channel")
		{
			/*
			Manifest will be like:
			#EXTM3U
			#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="ita",NAME="ita",AUTOSELECT=YES, DEFAULT=YES,URI="ita/8896718_1509416.m3u8"
			#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES, DEFAULT=YES,URI="eng/8896718_1509416.m3u8"
			#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=195023,AUDIO="audio"
			0/8896718_1509416.m3u8

			https://developer.apple.com/documentation/http_live_streaming/example_playlists_for_http_live_streaming/adding_alternate_media_to_a_playlist#overview
			https://github.com/videojs/http-streaming/blob/master/docs/multiple-alternative-audio-tracks.md

			*/

			string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
			string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");
			{
				for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsNumber; inputChannelIndex++)
				{
					string audioTrackDirectoryName = to_string(inputChannelIndex) + "_audio";

					string audioPathName = manifestDirectoryPath + "/" + audioTrackDirectoryName;

					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory (if needed)" + ", audioPathName: " + audioPathName);
					fs::create_directories(audioPathName);
					fs::permissions(
						audioPathName,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
							fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace
					);
				}

				{
					string videoTrackDirectoryName = "0_video";
					string videoPathName = manifestDirectoryPath + "/" + videoTrackDirectoryName;

					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory (if needed)" + ", videoPathName: " + videoPathName);
					fs::create_directories(videoPathName);
					fs::permissions(
						videoPathName,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
							fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace
					);
				}
			}

			// create main manifest file
			{
				string mainManifestPathName = manifestDirectoryPath + "/" + manifestFileName;

				string mainManifest;

				mainManifest = string("#EXTM3U") + "\n";

				for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsNumber; inputChannelIndex++)
				{
					string audioTrackDirectoryName = to_string(inputChannelIndex) + "_audio";

					json inputChannelRoot = inputChannelsRoot[inputChannelIndex];
					string inputChannelName = JSONUtils::asString(inputChannelRoot, "inputConfigurationLabel", "");

					string audioManifestLine = "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",LANGUAGE=\"" + inputChannelName + "\",NAME=\"" +
											   inputChannelName + "\",AUTOSELECT=YES, DEFAULT=YES,URI=\"" + audioTrackDirectoryName + "/" +
											   manifestFileName + "\"";

					mainManifest += (audioManifestLine + "\n");
				}

				string videoManifestLine = "#EXT-X-STREAM-INF:PROGRAM-ID=1";
				// TO DO: recuperare videoBitRateInKbps da outputsRootToFfmpeg
				int videoBitRateInKbps = -1;
				if (videoBitRateInKbps != -1)
					videoManifestLine += (",BANDWIDTH=" + to_string(videoBitRateInKbps * 1000));
				videoManifestLine += ",AUDIO=\"audio\"";

				mainManifest += (videoManifestLine + "\n");

				string videoTrackDirectoryName = "0_video";
				mainManifest += (videoTrackDirectoryName + "/" + manifestFileName + "\n");

				ofstream manifestFile(mainManifestPathName);
				manifestFile << mainManifest;
			}
		}

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

		_logger->info(
			__FILEREF__ + "liveGrid: Executing ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
			", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
		);

		startFfmpegCommand = chrono::system_clock::now();

		bool redirectionStdOutput = true;
		bool redirectionStdError = true;

		ProcessUtility::forkAndExec(
			_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
			&iReturnedStatus
		);
		*pChildPid = 0;
		if (iReturnedStatus != 0)
		{
			string errorMessage = __FILEREF__ + "liveGrid: ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", iReturnedStatus: " + to_string(iReturnedStatus) +
								  ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
								  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "liveGrid: command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						   ", encodingJobKey: " + to_string(encodingJobKey);
			throw runtime_error(errorMessage);
		}

		endFfmpegCommand = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "liveGrid: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
			", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);

		try
		{
			outputsRootToFfmpeg_clean(ingestionJobKey, encodingJobKey, outputsRoot, externalEncoder);
		}
		catch (runtime_error &e)
		{
			string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
			_logger->error(errorMessage);

			// throw e;
		}
		catch (exception &e)
		{
			string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
			_logger->error(errorMessage);

			// throw e;
		}
	}
	catch (runtime_error &e)
	{
		*pChildPid = 0;

		string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9) // 9 means: SIGKILL
		{
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user" +
						   ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
						   ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
						   ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
		}
		else
		{
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						   ", encodingJobKey: " + to_string(encodingJobKey) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
						   ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile +
						   ", e.what(): " + e.what();

			/*
			{
				char		sEndFfmpegCommand [64];

				time_t	utcEndFfmpegCommand = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm		tmUtcEndFfmpegCommand;
				localtime_r (&utcEndFfmpegCommand, &tmUtcEndFfmpegCommand);
				sprintf (sEndFfmpegCommand, "%04d-%02d-%02d-%02d-%02d-%02d",
					tmUtcEndFfmpegCommand. tm_year + 1900,
					tmUtcEndFfmpegCommand. tm_mon + 1,
					tmUtcEndFfmpegCommand. tm_mday,
					tmUtcEndFfmpegCommand. tm_hour,
					tmUtcEndFfmpegCommand. tm_min,
					tmUtcEndFfmpegCommand. tm_sec);

				string debugOutputFfmpegPathFileName =
					_ffmpegTempDir + "/"
					+ to_string(ingestionJobKey) + "_"
					+ to_string(encodingJobKey) + "_"
					+ sEndFfmpegCommand
					+ ".liveGrid.log.debug"
				;

				_logger->info(__FILEREF__ + "Coping"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
					);
				fs::copy(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);
			}
			*/
		}
		_logger->error(errorMessage);

		/*
		_logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		bool exceptionInCaseOfError = false;
		fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
		*/

		try
		{
			outputsRootToFfmpeg_clean(ingestionJobKey, encodingJobKey, outputsRoot, externalEncoder);
		}
		catch (runtime_error &e)
		{
			string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
			_logger->error(errorMessage);

			// throw e;
		}
		catch (exception &e)
		{
			string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
			_logger->error(errorMessage);

			// throw e;
		}

		if (iReturnedStatus == 9) // 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
			throw FFMpegURLForbidden();
		else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
			throw FFMpegURLNotFound();
		else
			throw e;
	}

	renameOutputFfmpegPathFileName(ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName);

	/*
	_logger->info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
	bool exceptionInCaseOfError = false;
	fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
	*/
}
