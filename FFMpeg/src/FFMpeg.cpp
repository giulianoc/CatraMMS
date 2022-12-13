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
#include <fstream>
#include <sstream>
#include <regex>
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "FFMpegEncodingParameters.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/StringUtils.h"
#include "FFMpeg.h"


FFMpeg::FFMpeg(Json::Value configuration,
        shared_ptr<spdlog::logger> logger) 
{
    _logger             = logger;

    _ffmpegPath = configuration["ffmpeg"].get("path", "").asString();
    _ffmpegTempDir = configuration["ffmpeg"].get("tempDir", "").asString();
    _ffmpegEndlessRecursivePlaylistDir = configuration["ffmpeg"].get("endlessRecursivePlaylistDir", "").asString();
    _ffmpegTtfFontDir = configuration["ffmpeg"].get("ttfFontDir", "").asString();

    _youTubeDlPath = configuration["youTubeDl"].get("path", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", youTubeDl->path: " + _youTubeDlPath
    );
    _pythonPathName = configuration["youTubeDl"].get("pythonPathName", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", youTubeDl->pythonPathName: " + _pythonPathName
    );

    _waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(configuration["storage"],
		"waitingNFSSync_maxMillisecondsToWait", 60000);
	/*
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->waitingNFSSync_attemptNumber: "
		+ to_string(_waitingNFSSync_attemptNumber)
    );
	*/
    _waitingNFSSync_milliSecondsWaitingBetweenChecks = JSONUtils::asInt(configuration["storage"],
		"waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	/*
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->waitingNFSSync_sleepTimeInSeconds: "
		+ to_string(_waitingNFSSync_sleepTimeInSeconds)
    );
	*/

    _startCheckingFrameInfoInMinutes = JSONUtils::asInt(configuration["ffmpeg"],
		"startCheckingFrameInfoInMinutes", 5);

    _charsToBeReadFromFfmpegErrorOutput     = 1024 * 3;
    
    _twoPasses = false;
    _currentlyAtSecondPass = false;

    _currentIngestionJobKey             = -1;	// just for log
    _currentEncodingJobKey              = -1;	// just for log
    _currentDurationInMilliSeconds      = -1;	// in case of some functionalities, it is important for getEncodingProgress
    _currentMMSSourceAssetPathName      = "";	// just for log
    _currentStagingEncodedAssetPathName = "";	// just for log

	_startFFMpegMethod = chrono::system_clock::now();

	_incrontabConfigurationDirectory	= "/home/mms/mms/conf";
	_incrontabConfigurationFileName		= "incrontab.txt";
	_incrontabBinary					= "/usr/bin/incrontab";
}

FFMpeg::~FFMpeg() 
{
    
}

/*
void FFMpeg::encodeContent(
	string mmsSourceAssetPathName,
	int64_t durationInMilliSeconds,
	string stagingEncodedAssetPathName,
	Json::Value encodingProfileDetailsRoot,
	bool isVideo,   // if false it means is audio
	Json::Value videoTracksRoot,
	Json::Value audioTracksRoot,
	int videoTrackIndexToBeUsed, int audioTrackIndexToBeUsed,
	int64_t physicalPathKey,
	int64_t encodingJobKey,
	int64_t ingestionJobKey,
	pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "encodeContent";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		durationInMilliSeconds,
		mmsSourceAssetPathName,
		stagingEncodedAssetPathName
	);

    try
    {
		_logger->info(__FILEREF__ + "Received " + _currentApiName
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", isVideo: " + to_string(isVideo)
			+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
			+ ", videoTracksRoot.size: " + to_string(videoTracksRoot.size())
			+ ", audioTracksRoot.size: " + to_string(audioTracksRoot.size())
			+ ", videoTrackIndexToBeUsed: " + to_string(videoTrackIndexToBeUsed)
			+ ", audioTrackIndexToBeUsed: " + to_string(audioTrackIndexToBeUsed)
		);

		if (!FileIO::fileExisting(mmsSourceAssetPathName)        
			&& !FileIO::directoryExisting(mmsSourceAssetPathName)
		)
		{
			string errorMessage = string("Source asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

        string httpStreamingFileFormat;    
		string ffmpegHttpStreamingParameter = "";

        string ffmpegFileFormatParameter = "";

        string ffmpegVideoCodecParameter = "";
        string ffmpegVideoProfileParameter = "";
        string ffmpegVideoOtherParameters = "";
        string ffmpegVideoFrameRateParameter = "";
        string ffmpegVideoKeyFramesRateParameter = "";
		vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

        string ffmpegAudioCodecParameter = "";
        string ffmpegAudioOtherParameters = "";
        string ffmpegAudioChannelsParameter = "";
        string ffmpegAudioSampleRateParameter = "";
		vector<string> audioBitRatesInfo;


        // _currentDurationInMilliSeconds      = durationInMilliSeconds;
        // _currentMMSSourceAssetPathName      = mmsSourceAssetPathName;
        // _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        // _currentIngestionJobKey             = ingestionJobKey;
        // _currentEncodingJobKey              = encodingJobKey;
        
        _currentlyAtSecondPass = false;

        // we will set by default _twoPasses to false otherwise, since the ffmpeg class is reused
        // it could remain set to true from a previous call
        _twoPasses = false;
        
        settingFfmpegParameters(
            encodingProfileDetailsRoot,
            isVideo,

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

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + "_"
				+ sUtcTimestamp
                + ".ffmpegoutput.log";
		}

		// special case:
		//	- input is mp4 or ts
		//	- output is hls
		//	- more than 1 audio track
		//	- one video track
		// In this case we will create:
		//  - one m3u8 for each track (video and audio)
		//  - one main m3u8 having a group for AUDIO
		string mp4Suffix = ".mp4";
		string tsSuffix = ".ts";
		if (
			// input is mp4
			(
			(mmsSourceAssetPathName.size() >= mp4Suffix.size()
			&& 0 == mmsSourceAssetPathName.compare(mmsSourceAssetPathName.size()-mp4Suffix.size(), mp4Suffix.size(), mp4Suffix))
			||
			// input is ts
			(mmsSourceAssetPathName.size() >= tsSuffix.size()
			&& 0 == mmsSourceAssetPathName.compare(mmsSourceAssetPathName.size()-tsSuffix.size(), tsSuffix.size(), tsSuffix))
			)

			// output is hls
			&& httpStreamingFileFormat == "hls"

			// more than 1 audio track
			&& audioTracksRoot.size() > 1

			// one video track
			&& videoTracksRoot.size() == 1
		)
		{
			// The command will be like this:

			// ffmpeg -y -i /var/catramms/storage/MMSRepository/MMS_0000/ws2/000/228/001/1247989_source.mp4

			// 	-map 0:1 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/ita/1247992_384637_%04d.ts -f hls /home/mms/tmp/ita/1247992_384637.m3u8

			// 	-map 0:2 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/eng/1247992_384637_%04d.ts -f hls /home/mms/tmp/eng/1247992_384637.m3u8

			// 	-map 0:0 -codec:v libx264 -profile:v high422 -b:v 800k -preset veryfast -level 4.0 -crf 22 -r 25 -vf scale=640:360 -threads 0 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/low/1247992_384637_%04d.ts -f hls /home/mms/tmp/low/1247992_384637.m3u8

			// Manifest will be like:
			// #EXTM3U
			// #EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="ita",NAME="ita",AUTOSELECT=YES, DEFAULT=YES,URI="ita/8896718_1509416.m3u8"
			// #EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES, DEFAULT=YES,URI="eng/8896718_1509416.m3u8"
			// #EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO="audio"
			// 0/8896718_1509416.m3u8


			// https://developer.apple.com/documentation/http_live_streaming/example_playlists_for_http_live_streaming/adding_alternate_media_to_a_playlist#overview
			// https://github.com/videojs/http-streaming/blob/master/docs/multiple-alternative-audio-tracks.md

			string ffmpegVideoResolutionParameter = "";
			int videoBitRateInKbps = -1;
			string ffmpegVideoBitRateParameter = "";
			string ffmpegVideoMaxRateParameter = "";
			string ffmpegVideoBufSizeParameter = "";
			string ffmpegAudioBitRateParameter = "";

			tuple<string, int, int, int, string, string, string> videoBitRateInfo
				= videoBitRatesInfo[0];
			tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore,
				ffmpegVideoBitRateParameter,
				ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter) = videoBitRateInfo;

			ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

			_logger->info(__FILEREF__ + "Special encoding in order to allow audio/language selection by the player"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
			);

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;

			{
				bool noErrorIfExists = true;
				bool recursive = true;
				_logger->info(__FILEREF__ + "Creating directory (if needed)"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);
				FileIO::createDirectory(stagingEncodedAssetPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

				for (int index = 0; index < audioTracksRoot.size(); index++)
				{
					Json::Value audioTrack = audioTracksRoot[index];

					string audioTrackDirectoryName = audioTrack.get("language", "").asString();

					string audioPathName = stagingEncodedAssetPathName + "/"
						+ audioTrackDirectoryName;

					_logger->info(__FILEREF__ + "Creating directory (if needed)"
						+ ", audioPathName: " + audioPathName
					);
					FileIO::createDirectory(audioPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}

				{
					string videoTrackDirectoryName;
					{
						Json::Value videoTrack = videoTracksRoot[0];

						videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
					}

					string videoPathName = stagingEncodedAssetPathName + "/"
						+ videoTrackDirectoryName;

					_logger->info(__FILEREF__ + "Creating directory (if needed)"
						+ ", videoPathName: " + videoPathName
					);
					FileIO::createDirectory(videoPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}
			}

			// the manifestFileName naming convention is used also in EncoderVideoAudioProxy.cpp
			string manifestFileName = to_string(ingestionJobKey)
				+ "_" + to_string(encodingJobKey)
				+ ".m3u8";

            if (_twoPasses)
            {
                string passlogFileName = 
                    to_string(_currentIngestionJobKey)
                    + "_"
                    + to_string(_currentEncodingJobKey) + ".passlog";
                string ffmpegPassLogPathFileName = _ffmpegTempDir // string(stagingEncodedAssetPath)
                    + "/"
                    + passlogFileName
                    ;

                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				// It should be useless to add the audio parameters in phase 1 but,
				// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
				//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
				//  So, this is the reason, I'm adding phase 2 as well
				// + "-an "    // disable audio
				for (int index = 0; index < audioTracksRoot.size(); index++)
				{
					Json::Value audioTrack = audioTracksRoot[index];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(audioTrack.get("trackIndex", -1).asInt()));

					FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

					string audioTrackDirectoryName = audioTrack.get("language", "").asString();

					{
						string segmentPathFileName =
							stagingEncodedAssetPathName 
							+ "/"
							+ audioTrackDirectoryName
							+ "/"
							+ to_string(_currentIngestionJobKey)
							+ "_"
							+ to_string(_currentEncodingJobKey)
							+ "_%04d.ts"
						;
						ffmpegArgumentList.push_back("-hls_segment_filename");
						ffmpegArgumentList.push_back(segmentPathFileName);
					}

					addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
					{
						string stagingManifestAssetPathName =
							stagingEncodedAssetPathName
							+ "/" + audioTrackDirectoryName
							+ "/" + manifestFileName;
						ffmpegArgumentList.push_back(stagingManifestAssetPathName);
					}
				}

				{
					Json::Value videoTrack = videoTracksRoot[0];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(videoTrack.get("trackIndex", -1).asInt()));
				}
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
					ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("1");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
				// 2020-01-20: I removed the hls file format parameter because it was not working
				//	and added -f mp4. At the end it has to generate just the log file
				//	to be used in the second step
				// addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);
				//
				// addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-f");
				// 2020-08-21: changed from mp4 to null
				ffmpegArgumentList.push_back("null");

				ffmpegArgumentList.push_back("/dev/null");

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
                    removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegArgumentList.clear();
				ffmpegArgumentListStream.clear();

                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				// It should be useless to add the audio parameters in phase 1 but,
				// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
				//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
				//  So, this is the reason, I'm adding phase 2 as well
				// + "-an "    // disable audio
				for (int index = 0; index < audioTracksRoot.size(); index++)
				{
					Json::Value audioTrack = audioTracksRoot[index];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(audioTrack.get("trackIndex", -1).asInt()));

					addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

					string audioTrackDirectoryName = audioTrack.get("language", "").asString();

					{
						string segmentPathFileName =
							stagingEncodedAssetPathName 
							+ "/"
							+ audioTrackDirectoryName
							+ "/"
							+ to_string(_currentIngestionJobKey)
							+ "_"
							+ to_string(_currentEncodingJobKey)
							+ "_%04d.ts"
						;
						ffmpegArgumentList.push_back("-hls_segment_filename");
						ffmpegArgumentList.push_back(segmentPathFileName);
					}

					addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
					{
						string stagingManifestAssetPathName =
							stagingEncodedAssetPathName
							+ "/" + audioTrackDirectoryName
							+ "/" + manifestFileName;
						ffmpegArgumentList.push_back(stagingManifestAssetPathName);
					}
				}

				{
					Json::Value videoTrack = videoTracksRoot[0];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(videoTrack.get("trackIndex", -1).asInt()));
				}
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
					ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("2");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);

				addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

				string videoTrackDirectoryName;
				{
					Json::Value videoTrack = videoTracksRoot[0];

					videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
				}

				{
					string segmentPathFileName =
						stagingEncodedAssetPathName 
						+ "/"
						+ videoTrackDirectoryName
						+ "/"
						+ to_string(_currentIngestionJobKey)
						+ "_"
						+ to_string(_currentEncodingJobKey)
						+ "_%04d.ts"
					;
					ffmpegArgumentList.push_back("-hls_segment_filename");
					ffmpegArgumentList.push_back(segmentPathFileName);
				}

				addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				{
					string stagingManifestAssetPathName =
						stagingEncodedAssetPathName
						+ "/" + videoTrackDirectoryName
						+ "/" + manifestFileName;
					ffmpegArgumentList.push_back(stagingManifestAssetPathName);
				}

                _currentlyAtSecondPass = true;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;            
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
                    removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);

				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
			}
			else
            {
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				// It should be useless to add the audio parameters in phase 1 but,
				// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
				//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
				//  So, this is the reason, I'm adding phase 2 as well
				// + "-an "    // disable audio
				for (int index = 0; index < audioTracksRoot.size(); index++)
				{
					Json::Value audioTrack = audioTracksRoot[index];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(audioTrack.get("trackIndex", -1).asInt()));

					addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

					string audioTrackDirectoryName = audioTrack.get("language", "").asString();

					{
						string segmentPathFileName =
							stagingEncodedAssetPathName 
							+ "/"
							+ audioTrackDirectoryName
							+ "/"
							+ to_string(_currentIngestionJobKey)
							+ "_"
							+ to_string(_currentEncodingJobKey)
							+ "_%04d.ts"
						;
						ffmpegArgumentList.push_back("-hls_segment_filename");
						ffmpegArgumentList.push_back(segmentPathFileName);
					}

					addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
					{
						string stagingManifestAssetPathName =
							stagingEncodedAssetPathName
							+ "/" + audioTrackDirectoryName
							+ "/" + manifestFileName;
						ffmpegArgumentList.push_back(stagingManifestAssetPathName);
					}
				}

				{
					Json::Value videoTrack = videoTracksRoot[0];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(videoTrack.get("trackIndex", -1).asInt()));
				}
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
					ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");

				addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

				string videoTrackDirectoryName;
				{
					Json::Value videoTrack = videoTracksRoot[0];

					videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
				}

				{
					string segmentPathFileName =
						stagingEncodedAssetPathName 
						+ "/"
						+ videoTrackDirectoryName
						+ "/"
						+ to_string(_currentIngestionJobKey)
						+ "_"
						+ to_string(_currentEncodingJobKey)
						+ "_%04d.ts"
					;
					ffmpegArgumentList.push_back("-hls_segment_filename");
					ffmpegArgumentList.push_back(segmentPathFileName);
				}

				addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				{
					string stagingManifestAssetPathName =
						stagingEncodedAssetPathName
						+ "/" + videoTrackDirectoryName
						+ "/" + manifestFileName;
					ffmpegArgumentList.push_back(stagingManifestAssetPathName);
				}

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
			}

			long long llDirSize = -1;
			// if (FileIO::fileExisting(stagingEncodedAssetPathName))
			{
				llDirSize = FileIO::getDirectorySizeInBytes (
					stagingEncodedAssetPathName);
			}

            _logger->info(__FILEREF__ + "Encoded file generated"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				+ ", llDirSize: " + to_string(llDirSize)
				+ ", _twoPasses: " + to_string(_twoPasses)
            );

            if (llDirSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded dir size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded dir size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }

			// create manifest file
			{
				string mainManifestPathName = stagingEncodedAssetPathName + "/"
					+ manifestFileName;

				string mainManifest;

				mainManifest = string("#EXTM3U") + "\n";

				for (int index = 0; index < audioTracksRoot.size(); index++)
				{
					Json::Value audioTrack = audioTracksRoot[index];

					string audioTrackDirectoryName = audioTrack.get("language", "").asString();

					string audioLanguage = audioTrack.get("language", "").asString();

					string audioManifestLine = "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",LANGUAGE=\""
						+ audioLanguage + "\",NAME=\"" + audioLanguage + "\",AUTOSELECT=YES, DEFAULT=YES,URI=\""
						+ audioTrackDirectoryName + "/" + manifestFileName + "\"";
						
					mainManifest += (audioManifestLine + "\n");
				}

				string videoManifestLine = "#EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO=\"audio\"";
				mainManifest += (videoManifestLine + "\n");

				string videoTrackDirectoryName;
				{
					Json::Value videoTrack = videoTracksRoot[0];

					videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
				}
				mainManifest += (videoTrackDirectoryName + "/" + manifestFileName + "\n");

				ofstream manifestFile(mainManifestPathName);
				manifestFile << mainManifest;
			}
        }
		else if (httpStreamingFileFormat != "")
        {
			// hls or dash output

			vector<string> ffmpegArgumentList;

			{
				bool noErrorIfExists = true;
				bool recursive = true;
				_logger->info(__FILEREF__ + "Creating directory (if needed)"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);
				FileIO::createDirectory(stagingEncodedAssetPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
			}

			// the manifestFileName naming convention is used also in EncoderVideoAudioProxy.cpp
			string manifestFileName = to_string(ingestionJobKey) +
				"_" + to_string(encodingJobKey);
			if (httpStreamingFileFormat == "hls")
				manifestFileName += ".m3u8";
			else	// if (httpStreamingFileFormat == "dash")
				manifestFileName += ".mpd";

            if (_twoPasses)
            {
				string templateVariable = "__HEIGHT__";
				string templatePart = templateVariable + "p";

				// used as prefix to remove the temporary files
				string passlogFileName = 
					to_string(_currentIngestionJobKey)
					+ "_"
					+ to_string(_currentEncodingJobKey)
				;
                string ffmpegTemplatePassLogPathFileName = _ffmpegTempDir
					+ "/"
					+ passlogFileName
					+ "_" + templatePart + ".passlog";
				;

                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				// output options
				for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
				{
					tuple<string, int, int, int, string, string, string> videoBitRateInfo
						= videoBitRatesInfo [videoIndex];

					string ffmpegVideoResolutionParameter = "";
					int videoBitRateInKbps = -1;
					int videoHeight = -1;
					string ffmpegVideoBitRateParameter = "";
					string ffmpegVideoMaxRateParameter = "";
					string ffmpegVideoBufSizeParameter = "";
					string ffmpegAudioBitRateParameter = "";

					tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
						ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
						ffmpegVideoBufSizeParameter) = videoBitRateInfo;

					if (videoTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:v:") + to_string(videoTrackIndexToBeUsed));
					}
					if (audioTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:a:") + to_string(audioTrackIndexToBeUsed));
					}
					addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
					addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						ffmpegArgumentList);

					// It should be useless to add the audio parameters in phase 1 but,
					// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
					//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
					//  So, this is the reason, I'm adding phase 2 as well
					// + "-an "    // disable audio
					addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					if (audioBitRatesInfo.size() > videoIndex)
						ffmpegAudioBitRateParameter = audioBitRatesInfo[videoIndex];
					else 
						ffmpegAudioBitRateParameter = audioBitRatesInfo[
							audioBitRatesInfo.size() - 1];
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					ffmpegArgumentList.push_back("-threads");
					ffmpegArgumentList.push_back("0");
					ffmpegArgumentList.push_back("-pass");
					ffmpegArgumentList.push_back("1");
					ffmpegArgumentList.push_back("-passlogfile");
					{
						string ffmpegPassLogPathFileName =
							regex_replace(ffmpegTemplatePassLogPathFileName,
								regex(templateVariable), to_string(videoHeight));
						ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
					}

					// 2020-01-20: I removed the hls file format parameter
					//	because it was not working and added -f mp4.
					//	At the end it has to generate just the log file
					//	to be used in the second step
					// addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);
					//
					// addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
					ffmpegArgumentList.push_back("-f");
					// 2020-08-21: changed from mp4 to null
					ffmpegArgumentList.push_back("null");

					ffmpegArgumentList.push_back("/dev/null");
				}

				ostringstream ffmpegArgumentListStreamFirstStep;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStreamFirstStep, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
                    removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				string segmentTemplateDirectory;
				string segmentTemplatePathFileName;
				if (httpStreamingFileFormat == "hls")
				{
					segmentTemplateDirectory =
						stagingEncodedAssetPathName + "/" + templatePart;

					segmentTemplatePathFileName =
						segmentTemplateDirectory 
						+ "/"
						+ to_string(_currentIngestionJobKey)
						+ "_"
						+ to_string(_currentEncodingJobKey)
						+ "_%04d.ts"
					;
				}

				string stagingTemplateManifestAssetPathName =
					segmentTemplateDirectory
					+ "/"
					+ manifestFileName;

				ffmpegArgumentList.clear();

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				// output options
				for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
				{
					tuple<string, int, int, int, string, string, string> videoBitRateInfo
						= videoBitRatesInfo [videoIndex];

					string ffmpegVideoResolutionParameter = "";
					int videoBitRateInKbps = -1;
					int videoHeight = -1;
					string ffmpegVideoBitRateParameter = "";
					string ffmpegVideoMaxRateParameter = "";
					string ffmpegVideoBufSizeParameter = "";
					string ffmpegAudioBitRateParameter = "";

					tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
						ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
						ffmpegVideoBufSizeParameter) = videoBitRateInfo;

					{
						string segmentDirectory =
							regex_replace(segmentTemplateDirectory,
								regex(templateVariable), to_string(videoHeight));

						bool noErrorIfExists = true;
						bool recursive = true;
						_logger->info(__FILEREF__ + "Creating directory"
							+ ", : " + segmentDirectory
						);
						FileIO::createDirectory(segmentDirectory,
							S_IRUSR | S_IWUSR | S_IXUSR |
							S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
					}

					if (videoTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:v:") + to_string(videoTrackIndexToBeUsed));
					}
					if (audioTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:a:") + to_string(audioTrackIndexToBeUsed));
					}
					addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
					addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						ffmpegArgumentList);

					addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					if (audioBitRatesInfo.size() > videoIndex)
						ffmpegAudioBitRateParameter = audioBitRatesInfo[videoIndex];
					else 
						ffmpegAudioBitRateParameter = audioBitRatesInfo[
							audioBitRatesInfo.size() - 1];
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

					if (httpStreamingFileFormat == "hls")
					{
						string segmentPathFileName =
							regex_replace(segmentTemplatePathFileName,
								regex(templateVariable), to_string(videoHeight));
						ffmpegArgumentList.push_back("-hls_segment_filename");
						ffmpegArgumentList.push_back(segmentPathFileName);
					}

					{
						string stagingManifestAssetPathName =
							regex_replace(stagingTemplateManifestAssetPathName,
								regex(templateVariable), to_string(videoHeight));
						addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
						ffmpegArgumentList.push_back(stagingManifestAssetPathName);
					}

					ffmpegArgumentList.push_back("-threads");
					ffmpegArgumentList.push_back("0");
					ffmpegArgumentList.push_back("-pass");
					ffmpegArgumentList.push_back("2");
					ffmpegArgumentList.push_back("-passlogfile");
					{
						string ffmpegPassLogPathFileName =
							regex_replace(ffmpegTemplatePassLogPathFileName,
								regex(templateVariable), to_string(videoHeight));
						ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
					}
				}

				ostringstream ffmpegArgumentListStreamSecondStep;
                _currentlyAtSecondPass = true;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStreamSecondStep, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
                    removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);

				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				// create the master playlist
				{
						// #EXTM3U
						// #EXT-X-VERSION:3
						// #EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360
						// 360p.m3u8
						// #EXT-X-STREAM-INF:BANDWIDTH=1400000,RESOLUTION=842x480
						// 480p.m3u8
						// #EXT-X-STREAM-INF:BANDWIDTH=2800000,RESOLUTION=1280x720
						// 720p.m3u8
						// #EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080
						// 1080p.m3u8
					string endLine = "\n";
					string masterManifest =
						"#EXTM3U" + endLine
						+ "#EXT-X-VERSION:3" + endLine
					;

					for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= videoBitRatesInfo [videoIndex];

						int videoBitRateInKbps = -1;
						int videoWidth = -1;
						int videoHeight = -1;

						tie(ignore, videoBitRateInKbps, videoWidth, videoHeight,
							ignore, ignore,
							ignore) = videoBitRateInfo;

						masterManifest +=
							"#EXT-X-STREAM-INF:BANDWIDTH="
								+ to_string(videoBitRateInKbps * 1000)
								+ ",RESOLUTION=" + to_string(videoWidth)
								+ "x" + to_string(videoHeight) + endLine
							;

						string manifestRelativePathName;
						{
							manifestRelativePathName = templatePart
								+ "/" + manifestFileName;
							manifestRelativePathName =
								regex_replace(manifestRelativePathName,
									regex(templateVariable), to_string(videoHeight));
						}
						masterManifest +=
							manifestRelativePathName + endLine;
					}

					string masterManifestPathFileName = stagingEncodedAssetPathName + "/"
						+ manifestFileName;

					_logger->info(__FILEREF__ + "Writing Master Manifest File"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", masterManifestPathFileName: " + masterManifestPathFileName
						+ ", masterManifest: " + masterManifest
					);
					ofstream ofMasterManifestFile(masterManifestPathFileName);
					ofMasterManifestFile << masterManifest;
				}
			}
			else
            {
				string templateVariable = "__HEIGHT__";
				string templatePart = templateVariable + "p";

				string segmentTemplateDirectory;
				string segmentTemplatePathFileName;
				if (httpStreamingFileFormat == "hls")
				{
					segmentTemplateDirectory =
						stagingEncodedAssetPathName + "/" + templatePart;

					segmentTemplatePathFileName =
						segmentTemplateDirectory 
						+ "/"
						+ to_string(_currentIngestionJobKey)
						+ "_"
						+ to_string(_currentEncodingJobKey)
						+ "_%04d.ts"
					;
				}

				string stagingTemplateManifestAssetPathName =
					segmentTemplateDirectory
					+ "/"
					+ manifestFileName;

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
				{
					tuple<string, int, int, int, string, string, string> videoBitRateInfo
						= videoBitRatesInfo [videoIndex];

					string ffmpegVideoResolutionParameter = "";
					int videoBitRateInKbps = -1;
					int videoHeight = -1;
					string ffmpegVideoBitRateParameter = "";
					string ffmpegVideoMaxRateParameter = "";
					string ffmpegVideoBufSizeParameter = "";
					string ffmpegAudioBitRateParameter = "";

					tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
						ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
						ffmpegVideoBufSizeParameter) = videoBitRateInfo;

					{
						string segmentDirectory =
							regex_replace(segmentTemplateDirectory,
								regex(templateVariable), to_string(videoHeight));

						bool noErrorIfExists = true;
						bool recursive = true;
						_logger->info(__FILEREF__ + "Creating directory"
							+ ", : " + segmentDirectory
						);
						FileIO::createDirectory(segmentDirectory,
							S_IRUSR | S_IWUSR | S_IXUSR |
							S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
					}

					if (videoTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:v:") + to_string(videoTrackIndexToBeUsed));
					}
					if (audioTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:a:") + to_string(audioTrackIndexToBeUsed));
					}
					addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
					addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						ffmpegArgumentList);
					ffmpegArgumentList.push_back("-threads");
					ffmpegArgumentList.push_back("0");
					addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					if (audioBitRatesInfo.size() > videoIndex)
						ffmpegAudioBitRateParameter = audioBitRatesInfo[videoIndex];
					else 
						ffmpegAudioBitRateParameter = audioBitRatesInfo[
							audioBitRatesInfo.size() - 1];
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

					if (httpStreamingFileFormat == "hls")
					{
						string segmentPathFileName =
							regex_replace(segmentTemplatePathFileName,
								regex(templateVariable), to_string(videoHeight));
						ffmpegArgumentList.push_back("-hls_segment_filename");
						ffmpegArgumentList.push_back(segmentPathFileName);
					}

					{
						string stagingManifestAssetPathName =
							regex_replace(stagingTemplateManifestAssetPathName,
								regex(templateVariable), to_string(videoHeight));
						addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
						ffmpegArgumentList.push_back(stagingManifestAssetPathName);
					}
				}

				ostringstream ffmpegArgumentListStream;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				// create the master playlist
				{
						// #EXTM3U
						// #EXT-X-VERSION:3
						// #EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360
						// 360p.m3u8
						// #EXT-X-STREAM-INF:BANDWIDTH=1400000,RESOLUTION=842x480
						// 480p.m3u8
						// #EXT-X-STREAM-INF:BANDWIDTH=2800000,RESOLUTION=1280x720
						// 720p.m3u8
						// #EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080
						// 1080p.m3u8
					string endLine = "\n";
					string masterManifest =
						"#EXTM3U" + endLine
						+ "#EXT-X-VERSION:3" + endLine
					;

					for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= videoBitRatesInfo [videoIndex];

						int videoBitRateInKbps = -1;
						int videoWidth = -1;
						int videoHeight = -1;

						tie(ignore, videoBitRateInKbps, videoWidth, videoHeight,
							ignore, ignore,
							ignore) = videoBitRateInfo;

						masterManifest +=
							"#EXT-X-STREAM-INF:BANDWIDTH="
								+ to_string(videoBitRateInKbps * 1000)
								+ ",RESOLUTION=" + to_string(videoWidth)
								+ "x" + to_string(videoHeight) + endLine
							;

						string manifestRelativePathName;
						{
							manifestRelativePathName = templatePart
								+ "/" + manifestFileName;
							manifestRelativePathName =
								regex_replace(manifestRelativePathName,
									regex(templateVariable), to_string(videoHeight));
						}
						masterManifest +=
							manifestRelativePathName + endLine;
					}

					string masterManifestPathFileName = stagingEncodedAssetPathName + "/"
						+ manifestFileName;

					_logger->info(__FILEREF__ + "Writing Master Manifest File"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", masterManifestPathFileName: " + masterManifestPathFileName
						+ ", masterManifest: " + masterManifest
					);
					ofstream ofMasterManifestFile(masterManifestPathFileName);
					ofMasterManifestFile << masterManifest;
				}
			}

			long long llDirSize = -1;
			// if (FileIO::fileExisting(stagingEncodedAssetPathName))
			{
				llDirSize = FileIO::getDirectorySizeInBytes (
					stagingEncodedAssetPathName);
			}

            _logger->info(__FILEREF__ + "Encoded file generated"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				+ ", llDirSize: " + to_string(llDirSize)
				+ ", _twoPasses: " + to_string(_twoPasses)
            );

            if (llDirSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded dir size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded dir size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }

            // changes to be done to the manifest, see EncoderThread.cpp
        }
        else
        {
			// 2021-09-10: In case videoBitRatesInfo has more than one bitrates,
			//	it has to be created one file for each bit rate and than
			//	merge all in the last file with a copy command, i.e.:
			//		- ffmpeg -i ./1.mp4 -i ./2.mp4 -c copy -map 0 -map 1 ./3.mp4

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;

            if (_twoPasses)
            {
				string templateVariable = "__HEIGHT__";
				string templatePart = templateVariable + "p";

				// used as prefix to remove the temporary files
				string passlogFileName = 
					to_string(_currentIngestionJobKey)
					+ "_"
					+ to_string(_currentEncodingJobKey)
				;
                string ffmpegTemplatePassLogPathFileName = _ffmpegTempDir
					+ "/"
					+ passlogFileName
					+ "_" + templatePart + ".passlog";
				;

                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
				{
					tuple<string, int, int, int, string, string, string> videoBitRateInfo
						= videoBitRatesInfo [videoIndex];

					string ffmpegVideoResolutionParameter = "";
					int videoBitRateInKbps = -1;
					int videoHeight = -1;
					string ffmpegVideoBitRateParameter = "";
					string ffmpegVideoMaxRateParameter = "";
					string ffmpegVideoBufSizeParameter = "";
					string ffmpegAudioBitRateParameter = "";

					tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
						ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
						ffmpegVideoBufSizeParameter) = videoBitRateInfo;

					if (videoTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:v:") + to_string(videoTrackIndexToBeUsed));
					}
					if (audioTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:a:") + to_string(audioTrackIndexToBeUsed));
					}
					addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
					addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						ffmpegArgumentList);
					ffmpegArgumentList.push_back("-threads");
					ffmpegArgumentList.push_back("0");
					ffmpegArgumentList.push_back("-pass");
					ffmpegArgumentList.push_back("1");
					ffmpegArgumentList.push_back("-passlogfile");
					{
						string ffmpegPassLogPathFileName =
							regex_replace(ffmpegTemplatePassLogPathFileName,
								regex(templateVariable), to_string(videoHeight));
						ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
					}
					// It should be useless to add the audio parameters in phase 1 but,
					// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
					//	it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
					//	So, this is the reason, I'm adding phase 2 as well
					// + "-an "    // disable audio
					addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					if (audioBitRatesInfo.size() > videoIndex)
						ffmpegAudioBitRateParameter = audioBitRatesInfo[videoIndex];
					else 
						ffmpegAudioBitRateParameter = audioBitRatesInfo[
							audioBitRatesInfo.size() - 1];
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					// 2020-08-21: changed from ffmpegFileFormatParameter to -f null
					// addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
					ffmpegArgumentList.push_back("-f");
					ffmpegArgumentList.push_back("null");

					ffmpegArgumentList.push_back("/dev/null");
				}

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();
                    
					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (first step)"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "encodeContent command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);
                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

				string stagingTemplateEncodedAssetPathName;
				{
					size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No extension found"
							+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					// I tried the string::insert method but it did not work
					stagingTemplateEncodedAssetPathName =
						stagingEncodedAssetPathName.substr(0, extensionIndex)
						+ "_" + templatePart
						+ stagingEncodedAssetPathName.substr(extensionIndex)
					;
				}

				ffmpegArgumentList.clear();
				ffmpegArgumentListStream.clear();

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
				{
					tuple<string, int, int, int, string, string, string> videoBitRateInfo
						= videoBitRatesInfo [videoIndex];

					string ffmpegVideoResolutionParameter = "";
					int videoBitRateInKbps = -1;
					int videoHeight = -1;
					string ffmpegVideoBitRateParameter = "";
					string ffmpegVideoMaxRateParameter = "";
					string ffmpegVideoBufSizeParameter = "";
					string ffmpegAudioBitRateParameter = "";

					tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
						ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
						ffmpegVideoBufSizeParameter) = videoBitRateInfo;

					if (videoTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:v:") + to_string(videoTrackIndexToBeUsed));
					}
					if (audioTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:a:") + to_string(audioTrackIndexToBeUsed));
					}
					addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
					addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						ffmpegArgumentList);
					ffmpegArgumentList.push_back("-threads");
					ffmpegArgumentList.push_back("0");
					ffmpegArgumentList.push_back("-pass");
					ffmpegArgumentList.push_back("2");
					ffmpegArgumentList.push_back("-passlogfile");
					{
						string ffmpegPassLogPathFileName =
							regex_replace(ffmpegTemplatePassLogPathFileName,
								regex(templateVariable), to_string(videoHeight));
						ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
					}
					addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
					if (audioBitRatesInfo.size() > videoIndex)
						ffmpegAudioBitRateParameter = audioBitRatesInfo[videoIndex];
					else 
						ffmpegAudioBitRateParameter = audioBitRatesInfo[
							audioBitRatesInfo.size() - 1];
					addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
					if (videoBitRatesInfo.size() > 1)
					{
						string newStagingEncodedAssetPathName =
							regex_replace(stagingTemplateEncodedAssetPathName,
								regex(templateVariable), to_string(videoHeight));
						ffmpegArgumentList.push_back(newStagingEncodedAssetPathName);
					}
					else
						ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
				}

                _currentlyAtSecondPass = true;
                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (second step)"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed (second step)"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "encodeContent command failed (second step)"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }
                    
                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);
                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                bool exceptionInCaseOfError = false;
                removeHavingPrefixFileName(_ffmpegTempDir, passlogFileName);
                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				if (videoBitRatesInfo.size() > 1)
				{
					// all the tracks generated in different files have to be copied
					// into the stagingEncodedAssetPathName file
					// The command willl be:
					//		ffmpeg -i ... -i ... -c copy -map 0 -map 1 ... <dest file>

					vector<string> sourcesPathName;
					for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= videoBitRatesInfo [videoIndex];

						int videoHeight = -1;

						tie(ignore, ignore,
							ignore, videoHeight,
							ignore, ignore,
							ignore) = videoBitRateInfo;

						string newStagingEncodedAssetPathName =
							regex_replace(stagingTemplateEncodedAssetPathName,
								regex(templateVariable), to_string(videoHeight));
						sourcesPathName.push_back(newStagingEncodedAssetPathName);
					}

					try
					{
						muxAllFiles(ingestionJobKey, sourcesPathName,
							stagingEncodedAssetPathName);

						for (string sourcePathName: sourcesPathName)
						{
							bool exceptionInCaseOfError = false;
							_logger->info(__FILEREF__ + "Remove"
								+ ", sourcePathName: " + sourcePathName);
							FileIO::remove(sourcePathName, exceptionInCaseOfError);
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__ + "muxAllFiles failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						for (string sourcePathName: sourcesPathName)
						{
							bool exceptionInCaseOfError = false;
							_logger->info(__FILEREF__ + "Remove"
								+ ", sourcePathName: " + sourcePathName);
							FileIO::remove(sourcePathName, exceptionInCaseOfError);
						}

						throw e;
					}
				}
            }
            else
            {
				// used in case of multiple bitrate
				string templateVariable = "__HEIGHT__";
				string templatePart = templateVariable + "p";

				string stagingTemplateEncodedAssetPathName;
				{
					size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No extension found"
							+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					// I tried the string::insert method but it did not work
					stagingTemplateEncodedAssetPathName =
						stagingEncodedAssetPathName.substr(0, extensionIndex)
						+ "_" + templatePart
						+ stagingEncodedAssetPathName.substr(extensionIndex)
					;
				}

				ffmpegArgumentList.clear();
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				if (isVideo)
				{
					for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size();
						videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= videoBitRatesInfo [videoIndex];

						string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						int videoHeight = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegAudioBitRateParameter = "";

						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps,
							ignore, videoHeight,
							ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
							ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						if (videoTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:v:") + to_string(videoTrackIndexToBeUsed));
						}
						if (audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(audioTrackIndexToBeUsed));
						}
						addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
						addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
						addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
						addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
						addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
						addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
						addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
						addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
						addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
							ffmpegArgumentList);
						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
						if (audioBitRatesInfo.size() > videoIndex)
							ffmpegAudioBitRateParameter = audioBitRatesInfo[videoIndex];
						else 
							ffmpegAudioBitRateParameter = audioBitRatesInfo[
								audioBitRatesInfo.size() - 1];
						addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
						addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
						addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

						addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
						if (videoBitRatesInfo.size() > 1)
						{
							string newStagingEncodedAssetPathName =
								regex_replace(stagingTemplateEncodedAssetPathName,
									regex(templateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back(newStagingEncodedAssetPathName);
						}
						else
							ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
					}
				}
				else
				{
					for (int audioIndex = 0; audioIndex < audioBitRatesInfo.size();
						audioIndex++)
					{
						string ffmpegAudioBitRateParameter = audioBitRatesInfo[audioIndex];

						if (audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(audioTrackIndexToBeUsed));
						}
						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
						addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
						addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
						addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

						addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
						// if (videoBitRatesInfo.size() > 1)
						// {
						// 	string newStagingEncodedAssetPathName =
						// 		regex_replace(stagingTemplateEncodedAssetPathName,
						// 			regex(templateVariable), to_string(videoHeight));
						// 	ffmpegArgumentList.push_back(newStagingEncodedAssetPathName);
						// }
						// else
							ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
					}
				}

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "encodeContent command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();
                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				if (videoBitRatesInfo.size() > 1)
				{
					// all the tracks generated in different files have to be copied
					// into the stagingEncodedAssetPathName file
					// The command willl be:
					//		ffmpeg -i ... -i ... -c copy -map 0 -map 1 ... <dest file>

					vector<string> sourcesPathName;
					for (int videoIndex = 0; videoIndex < videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= videoBitRatesInfo [videoIndex];

						int videoHeight = -1;

						tie(ignore, ignore,
							ignore, videoHeight,
							ignore, ignore,
							ignore) = videoBitRateInfo;

						string newStagingEncodedAssetPathName =
							regex_replace(stagingTemplateEncodedAssetPathName,
								regex(templateVariable), to_string(videoHeight));
						sourcesPathName.push_back(newStagingEncodedAssetPathName);
					}

					try
					{
						muxAllFiles(ingestionJobKey, sourcesPathName,
							stagingEncodedAssetPathName);

						for (string sourcePathName: sourcesPathName)
						{
							bool exceptionInCaseOfError = false;
							_logger->info(__FILEREF__ + "Remove"
								+ ", sourcePathName: " + sourcePathName);
							FileIO::remove(sourcePathName, exceptionInCaseOfError);
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__ + "muxAllFiles failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						for (string sourcePathName: sourcesPathName)
						{
							bool exceptionInCaseOfError = false;
							_logger->info(__FILEREF__ + "Remove"
								+ ", sourcePathName: " + sourcePathName);
							FileIO::remove(sourcePathName, exceptionInCaseOfError);
						}

						throw e;
					}
				}
            }

			long long llFileSize = -1;
			// if (FileIO::fileExisting(stagingEncodedAssetPathName))
			{
				bool inCaseOfLinkHasItToBeRead = false;
				llFileSize = FileIO::getFileSizeInBytes (
					stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);
			}

            _logger->info(__FILEREF__ + "Encoded file generated"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				+ ", llFileSize: " + to_string(llFileSize)
				+ ", _twoPasses: " + to_string(_twoPasses)
            );

            if (llFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded file size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }
        }
    }
    catch(FFMpegEncodingKilledByUser e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
			|| FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
}
*/
void FFMpeg::encodeContent(
	string mmsSourceAssetPathName,
	int64_t durationInMilliSeconds,
	string encodedStagingAssetPathName,
	Json::Value encodingProfileDetailsRoot,
	bool isVideo,   // if false it means is audio
	Json::Value videoTracksRoot,
	Json::Value audioTracksRoot,
	int videoTrackIndexToBeUsed, int audioTrackIndexToBeUsed,
	int64_t physicalPathKey,
	int64_t encodingJobKey,
	int64_t ingestionJobKey,
	pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "encodeContent";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		durationInMilliSeconds,
		mmsSourceAssetPathName,
		encodedStagingAssetPathName
	);

    try
    {
		_logger->info(__FILEREF__ + "Received " + _currentApiName
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", isVideo: " + to_string(isVideo)
			+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
			+ ", videoTracksRoot.size: " + to_string(videoTracksRoot.size())
			+ ", audioTracksRoot.size: " + to_string(audioTracksRoot.size())
			+ ", videoTrackIndexToBeUsed: " + to_string(videoTrackIndexToBeUsed)
			+ ", audioTrackIndexToBeUsed: " + to_string(audioTrackIndexToBeUsed)
		);

		if (!FileIO::fileExisting(mmsSourceAssetPathName)        
			&& !FileIO::directoryExisting(mmsSourceAssetPathName)
		)
		{
			string errorMessage = string("Source asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		/*
        string httpStreamingFileFormat;    
		string ffmpegHttpStreamingParameter = "";

        string ffmpegFileFormatParameter = "";

        string ffmpegVideoCodecParameter = "";
        string ffmpegVideoProfileParameter = "";
        string ffmpegVideoOtherParameters = "";
        string ffmpegVideoFrameRateParameter = "";
        string ffmpegVideoKeyFramesRateParameter = "";
		vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

        string ffmpegAudioCodecParameter = "";
        string ffmpegAudioOtherParameters = "";
        string ffmpegAudioChannelsParameter = "";
        string ffmpegAudioSampleRateParameter = "";
		vector<string> audioBitRatesInfo;
		*/


        // _currentDurationInMilliSeconds      = durationInMilliSeconds;
        // _currentMMSSourceAssetPathName      = mmsSourceAssetPathName;
        // _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        // _currentIngestionJobKey             = ingestionJobKey;
        // _currentEncodingJobKey              = encodingJobKey;
        
        _currentlyAtSecondPass = false;

        // we will set by default _twoPasses to false otherwise, since the ffmpeg class is reused
        // it could remain set to true from a previous call
        _twoPasses = false;
        
		FFMpegEncodingParameters ffmpegEncodingParameters (
			ingestionJobKey,
			encodingJobKey,
			encodedStagingAssetPathName,
			encodingProfileDetailsRoot,
			isVideo,   // if false it means is audio
			videoTracksRoot,
			audioTracksRoot,
			videoTrackIndexToBeUsed,
			audioTrackIndexToBeUsed,

			_twoPasses,	// out

			_ffmpegTempDir,
			_logger) ;

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + "_"
				+ sUtcTimestamp
                + ".ffmpegoutput.log";
		}

		// special case:
		//	- input is mp4 or ts
		//	- output is hls
		//	- more than 1 audio track
		//	- one video track
		// In this case we will create:
		//  - one m3u8 for each track (video and audio)
		//  - one main m3u8 having a group for AUDIO
		string mp4Suffix = ".mp4";
		string tsSuffix = ".ts";
		if (
			// input is mp4
			(
			(mmsSourceAssetPathName.size() >= mp4Suffix.size()
			&& 0 == mmsSourceAssetPathName.compare(mmsSourceAssetPathName.size()-mp4Suffix.size(), mp4Suffix.size(), mp4Suffix))
			||
			// input is ts
			(mmsSourceAssetPathName.size() >= tsSuffix.size()
			&& 0 == mmsSourceAssetPathName.compare(mmsSourceAssetPathName.size()-tsSuffix.size(), tsSuffix.size(), tsSuffix))
			)

			// output is hls
			&& ffmpegEncodingParameters._httpStreamingFileFormat == "hls"

			// more than 1 audio track
			&& audioTracksRoot.size() > 1

			// one video track
			&& videoTracksRoot.size() == 1
		)
		{
			/*
			 * The command will be like this:

			ffmpeg -y -i /var/catramms/storage/MMSRepository/MMS_0000/ws2/000/228/001/1247989_source.mp4

				-map 0:1 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/ita/1247992_384637_%04d.ts -f hls /home/mms/tmp/ita/1247992_384637.m3u8

				-map 0:2 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/eng/1247992_384637_%04d.ts -f hls /home/mms/tmp/eng/1247992_384637.m3u8

				-map 0:0 -codec:v libx264 -profile:v high422 -b:v 800k -preset veryfast -level 4.0 -crf 22 -r 25 -vf scale=640:360 -threads 0 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/low/1247992_384637_%04d.ts -f hls /home/mms/tmp/low/1247992_384637.m3u8

			Manifest will be like:
			#EXTM3U
			#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="ita",NAME="ita",AUTOSELECT=YES, DEFAULT=YES,URI="ita/8896718_1509416.m3u8"
			#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES, DEFAULT=YES,URI="eng/8896718_1509416.m3u8"
			#EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO="audio"
			0/8896718_1509416.m3u8


			https://developer.apple.com/documentation/http_live_streaming/example_playlists_for_http_live_streaming/adding_alternate_media_to_a_playlist#overview
			https://github.com/videojs/http-streaming/blob/master/docs/multiple-alternative-audio-tracks.md

			*/

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;

			{
				bool noErrorIfExists = true;
				bool recursive = true;
				_logger->info(__FILEREF__ + "Creating directory (if needed)"
					+ ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
				);
				FileIO::createDirectory(encodedStagingAssetPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

				for (int index = 0; index < audioTracksRoot.size(); index++)
				{
					Json::Value audioTrack = audioTracksRoot[index];

					string audioTrackDirectoryName = audioTrack.get("language", "").asString();

					string audioPathName = encodedStagingAssetPathName + "/"
						+ audioTrackDirectoryName;

					_logger->info(__FILEREF__ + "Creating directory (if needed)"
						+ ", audioPathName: " + audioPathName
					);
					FileIO::createDirectory(audioPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}

				{
					string videoTrackDirectoryName;
					{
						Json::Value videoTrack = videoTracksRoot[0];

						videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
					}

					string videoPathName = encodedStagingAssetPathName + "/"
						+ videoTrackDirectoryName;

					_logger->info(__FILEREF__ + "Creating directory (if needed)"
						+ ", videoPathName: " + videoPathName
					);
					FileIO::createDirectory(videoPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}
			}

            if (_twoPasses)
            {
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding_audioGroup(
					0,	// YES two passes, first step
					ffmpegArgumentList);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegArgumentList.clear();
				ffmpegArgumentListStream.clear();

                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding_audioGroup(
					1,	// YES two passes, second step
					ffmpegArgumentList);

                _currentlyAtSecondPass = true;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;            
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
                    ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
			}
			else
            {
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding_audioGroup(
					-1,	// NO two passes
					ffmpegArgumentList);

				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
			}

			long long llDirSize = -1;
			// if (FileIO::fileExisting(encodedStagingAssetPathName))
			{
				llDirSize = FileIO::getDirectorySizeInBytes (
					encodedStagingAssetPathName);
			}

            _logger->info(__FILEREF__ + "Encoded file generated"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
				+ ", llDirSize: " + to_string(llDirSize)
				+ ", _twoPasses: " + to_string(_twoPasses)
            );

            if (llDirSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded dir size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded dir size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }

			ffmpegEncodingParameters.createManifestFile_audioGroup();
        }
		else if (ffmpegEncodingParameters._httpStreamingFileFormat != "")
        {
			// hls or dash output

			vector<string> ffmpegArgumentList;

			{
				bool noErrorIfExists = true;
				bool recursive = true;
				_logger->info(__FILEREF__ + "Creating directory (if needed)"
					+ ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
				);
				FileIO::createDirectory(encodedStagingAssetPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
			}

            if (_twoPasses)
            {
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					0,	// 0: YES two passes, first step
					ffmpegArgumentList	// out
				);

				ostringstream ffmpegArgumentListStreamFirstStep;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStreamFirstStep, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamFirstStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				ffmpegArgumentList.clear();

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					1,	// 1: YES two passes, second step
					ffmpegArgumentList	// out
				);

				ostringstream ffmpegArgumentListStreamSecondStep;
                _currentlyAtSecondPass = true;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStreamSecondStep, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStreamSecondStep.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				ffmpegEncodingParameters.createManifestFile();
			}
			else
            {
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					-1,	// -1: NO two passes
					ffmpegArgumentList	// out
				);

				ostringstream ffmpegArgumentListStream;
				try
				{
					chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

					_logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					);

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
					{
						string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__ + "encodeContent command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
					);
				}
				catch(runtime_error e)
				{
					string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					_logger->error(errorMessage);

					bool exceptionInCaseOfError = false;
					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				ffmpegEncodingParameters.createManifestFile();
			}

			long long llDirSize = -1;
			// if (FileIO::fileExisting(encodedStagingAssetPathName))
			{
				llDirSize = FileIO::getDirectorySizeInBytes (
					encodedStagingAssetPathName);
			}

            _logger->info(__FILEREF__ + "Encoded file generated"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
				+ ", llDirSize: " + to_string(llDirSize)
				+ ", _twoPasses: " + to_string(_twoPasses)
            );

            if (llDirSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded dir size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded dir size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }

            // changes to be done to the manifest, see EncoderThread.cpp
        }
        else
        {
			/* 2021-09-10: In case videoBitRatesInfo has more than one bitrates,
			 *	it has to be created one file for each bit rate and than
			 *	merge all in the last file with a copy command, i.e.:
			 *		- ffmpeg -i ./1.mp4 -i ./2.mp4 -c copy -map 0 -map 1 ./3.mp4
			*/

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;

            if (_twoPasses)
            {
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					0,	// 1: YES two passes, first step
					ffmpegArgumentList	// out
				);

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();
                    
					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (first step)"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "encodeContent command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

				ffmpegArgumentList.clear();
				ffmpegArgumentListStream.clear();

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					1,	// 1: YES two passes, second step
					ffmpegArgumentList	// out
				);

                _currentlyAtSecondPass = true;
                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command (second step)"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed (second step)"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "encodeContent command failed (second step)"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }
                    
                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
					ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                bool exceptionInCaseOfError = false;
				ffmpegEncodingParameters.removeTwoPassesTemporaryFiles();

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				vector<string> sourcesPathName;
				if (ffmpegEncodingParameters.getMultiTrackPathNames(sourcesPathName))
				{
					// all the tracks generated in different files have to be copied
					// into the encodedStagingAssetPathName file
					// The command willl be:
					//		ffmpeg -i ... -i ... -c copy -map 0 -map 1 ... <dest file>

					try
					{
						muxAllFiles(ingestionJobKey, sourcesPathName,
							encodedStagingAssetPathName);

						ffmpegEncodingParameters.removeMultiTrackPathNames();
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__ + "muxAllFiles failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						ffmpegEncodingParameters.removeMultiTrackPathNames();

						throw e;
					}
				}
            }
            else
            {
				// used in case of multiple bitrate

				ffmpegArgumentList.clear();
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);

				ffmpegEncodingParameters.applyEncoding(
					-1,	// -1: NO two passes
					ffmpegArgumentList	// out
				);

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "encodeContent: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "encodeContent: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "encodeContent command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();
                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				vector<string> sourcesPathName;
				if (ffmpegEncodingParameters.getMultiTrackPathNames(sourcesPathName))
				{
					// all the tracks generated in different files have to be copied
					// into the encodedStagingAssetPathName file
					// The command willl be:
					//		ffmpeg -i ... -i ... -c copy -map 0 -map 1 ... <dest file>

					try
					{
						muxAllFiles(ingestionJobKey, sourcesPathName,
							encodedStagingAssetPathName);

						ffmpegEncodingParameters.removeMultiTrackPathNames();
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__ + "muxAllFiles failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						ffmpegEncodingParameters.removeMultiTrackPathNames();

						throw e;
					}
				}
            }

			long long llFileSize = -1;
			// if (FileIO::fileExisting(encodedStagingAssetPathName))
			{
				bool inCaseOfLinkHasItToBeRead = false;
				llFileSize = FileIO::getFileSizeInBytes (
					encodedStagingAssetPathName, inCaseOfLinkHasItToBeRead);
			}

            _logger->info(__FILEREF__ + "Encoded file generated"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
				+ ", llFileSize: " + to_string(llFileSize)
				+ ", _twoPasses: " + to_string(_twoPasses)
            );

            if (llFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded file size is 0"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }
        }
    }
    catch(FFMpegEncodingKilledByUser e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(encodedStagingAssetPathName)
			|| FileIO::directoryExisting(encodedStagingAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(encodedStagingAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(encodedStagingAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName);
                FileIO::remove(encodedStagingAssetPathName);
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(encodedStagingAssetPathName)
                || FileIO::directoryExisting(encodedStagingAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(encodedStagingAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(encodedStagingAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName);
                FileIO::remove(encodedStagingAssetPathName);
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
        );

        if (FileIO::fileExisting(encodedStagingAssetPathName)
                || FileIO::directoryExisting(encodedStagingAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(encodedStagingAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(encodedStagingAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName);
                FileIO::remove(encodedStagingAssetPathName);
            }
        }

        throw e;
    }
}


void FFMpeg::overlayImageOnVideo(
	bool externalEncoder,
	string mmsSourceVideoAssetPathName,
	int64_t videoDurationInMilliSeconds,
	string mmsSourceImageAssetPathName,
	string imagePosition_X_InPixel,
	string imagePosition_Y_InPixel,
	string stagingEncodedAssetPathName,
	Json::Value encodingProfileDetailsRoot,
	int64_t encodingJobKey,
	int64_t ingestionJobKey,
	pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "overlayImageOnVideo";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		videoDurationInMilliSeconds,
		mmsSourceVideoAssetPathName,
		stagingEncodedAssetPathName
	);

    try
    {
		if (!FileIO::fileExisting(mmsSourceVideoAssetPathName)        
			&& !FileIO::directoryExisting(mmsSourceVideoAssetPathName)
		)
		{
			string errorMessage = string("Source video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (!externalEncoder)
		{
			if (!FileIO::fileExisting(mmsSourceImageAssetPathName))
			{
				string errorMessage = string("Source image asset path name not existing")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		vector<string> ffmpegEncodingProfileArgumentList;
		if (encodingProfileDetailsRoot != Json::nullValue)
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

					string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to false"
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
			catch(runtime_error e)
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

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + "_"
				+ sUtcTimestamp
                + ".ffmpegoutput.log";
		}


        {
            string ffmpegImagePosition_X_InPixel = 
                    regex_replace(imagePosition_X_InPixel, regex("video_width"), "main_w");
            ffmpegImagePosition_X_InPixel = 
                    regex_replace(ffmpegImagePosition_X_InPixel, regex("image_width"), "overlay_w");
            
            string ffmpegImagePosition_Y_InPixel = 
                    regex_replace(imagePosition_Y_InPixel, regex("video_height"), "main_h");
            ffmpegImagePosition_Y_InPixel = 
                    regex_replace(ffmpegImagePosition_Y_InPixel, regex("image_height"), "overlay_h");

			/*
            string ffmpegFilterComplex = string("-filter_complex 'overlay=")
                    + ffmpegImagePosition_X_InPixel + ":"
                    + ffmpegImagePosition_Y_InPixel + "'"
                    ;
			*/
            string ffmpegFilterComplex = string("-filter_complex overlay=")
                    + ffmpegImagePosition_X_InPixel + ":"
                    + ffmpegImagePosition_Y_InPixel
                    ;
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
				if (encodingProfileDetailsRoot != Json::nullValue)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);
				}

				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "overlayImageOnVideo: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "overlayImageOnVideo: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "overlayImageOnVideo command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "overlayImageOnVideo: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }

            _logger->info(__FILEREF__ + "Overlayed file generated"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);

            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }
        }        
    }
    catch(FFMpegEncodingKilledByUser e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg overlay failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg overlay failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg overlay failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", mmsSourceImageAssetPathName: " + mmsSourceImageAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
}

void FFMpeg::overlayTextOnVideo(
	string mmsSourceVideoAssetPathName,
	int64_t videoDurationInMilliSeconds,

	string text,
	int reloadAtFrameInterval,
	string textPosition_X_InPixel,
	string textPosition_Y_InPixel,
	string fontType,
	int fontSize,
	string fontColor,
	int textPercentageOpacity,
	int shadowX,
	int shadowY,
	bool boxEnable,
	string boxColor,
	int boxPercentageOpacity,

	Json::Value encodingProfileDetailsRoot,
	string stagingEncodedAssetPathName,
	int64_t encodingJobKey,
	int64_t ingestionJobKey,
	pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "overlayTextOnVideo";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		videoDurationInMilliSeconds,
		mmsSourceVideoAssetPathName,
		stagingEncodedAssetPathName
	);

	string textTemporaryFileName;
    try
    {
		if (!FileIO::fileExisting(mmsSourceVideoAssetPathName)        
			&& !FileIO::directoryExisting(mmsSourceVideoAssetPathName)
		)
		{
			string errorMessage = string("Source video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegEncodingProfileArgumentList;
		if (encodingProfileDetailsRoot != Json::nullValue)
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

					string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to false"
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
				// addToArguments(ffmpegVideoResolutionParameter, ffmpegEncodingProfileArgumentList);
				ffmpegEncodingProfileArgumentList.push_back("-threads");
				ffmpegEncodingProfileArgumentList.push_back("0");
				FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
			}
			catch(runtime_error e)
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

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + "_"
				+ sUtcTimestamp
                + ".ffmpegoutput.log";
		}

        {
			{
				textTemporaryFileName =
					_ffmpegTempDir + "/"
					+ to_string(_currentIngestionJobKey)
					+ "_"
					+ to_string(_currentEncodingJobKey)
					+ ".overlayText";
				ofstream of(textTemporaryFileName, ofstream::trunc);
				of << text;
				of.flush();
			}

			_logger->info(__FILEREF__ + "overlayTextOnVideo: added text into a temporary file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", textTemporaryFileName: " + textTemporaryFileName
				+ ", text: " + text
			);

			string ffmpegDrawTextFilter = getDrawTextVideoFilterDescription(ingestionJobKey,
				"", textTemporaryFileName, reloadAtFrameInterval,
				textPosition_X_InPixel, textPosition_Y_InPixel, fontType, fontSize,
				fontColor, textPercentageOpacity, shadowX, shadowY,
				boxEnable, boxColor, boxPercentageOpacity, -1);

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
				if (encodingProfileDetailsRoot != Json::nullValue)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);
				}

				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "overlayTextOnVideo: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "overlayTextOnVideo: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "overlayTextOnVideo command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "overlayTextOnVideo: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @"
							+ to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", textTemporaryFileName: " + textTemporaryFileName);
                    bool exceptionInCaseOfError = false;
                    FileIO::remove(textTemporaryFileName, exceptionInCaseOfError);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

				_logger->info(__FILEREF__ + "Remove"
					+ ", textTemporaryFileName: " + textTemporaryFileName);
				bool exceptionInCaseOfError = false;
				FileIO::remove(textTemporaryFileName, exceptionInCaseOfError);

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }

            _logger->info(__FILEREF__ + "Drawtext file generated"
                + ", encodingJobKey: " + to_string(encodingJobKey)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);

            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }
        }        
    }
    catch(FFMpegEncodingKilledByUser e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg drawtext failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg drawtext failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg drawtext failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
}

string FFMpeg::getDrawTextVideoFilterDescription(
	int64_t ingestionJobKey,
	string text,				// text or textFilePathName has to be filled
	string textFilePathName,
	int reloadAtFrameInterval,
	string textPosition_X_InPixel,
	string textPosition_Y_InPixel,
	string fontType,
	int fontSize,
	string fontColor,
	int textPercentageOpacity,
	int shadowX,
	int shadowY,
	bool boxEnable,
	string boxColor,
	int boxPercentageOpacity,
	int64_t streamingDurationInSeconds	// used in case of countdown
)
{

	string ffmpegDrawTextFilter;

	{
		// management of the text, many processing is in case of a countdown
		string ffmpegText = text;
		if (streamingDurationInSeconds != -1)
		{
			// see https://ffmpeg.org/ffmpeg-filters.html
			// see https://ffmpeg.org/ffmpeg-utils.html
			//
			// expr_int_format, eif
			//	Evaluate the expression’s value and output as formatted integer.
			//	The first argument is the expression to be evaluated, just as for the expr function.
			//	The second argument specifies the output format. Allowed values are ‘x’, ‘X’, ‘d’ and ‘u’. They are treated exactly as in the printf function.
			//	The third parameter is optional and sets the number of positions taken by the output. It can be used to add padding with zeros from the left.
			//

			if (textFilePathName != "")
			{
				ifstream ifPathFileName(textFilePathName);
				if (ifPathFileName)
				{
					// get size/length of file:
					ifPathFileName.seekg (0, ifPathFileName.end);
					int fileSize = ifPathFileName.tellg();
					ifPathFileName.seekg (0, ifPathFileName.beg);

					char* buffer = new char [fileSize];
					ifPathFileName.read (buffer, fileSize);
					if (ifPathFileName)
					{
						// all characters read successfully
						ffmpegText.assign(buffer, fileSize);                                                 
					}
					else
					{
						// error: only is.gcount() could be read";
						ffmpegText.assign(buffer, ifPathFileName.gcount());
					}
					ifPathFileName.close();
					delete[] buffer;
				}
				else
				{
					_logger->error(__FILEREF__ + "ffmpeg: drawtext file cannot be read"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", textFilePathName: " + textFilePathName
					);
				}
			}

			string escape = "\\";
			if (textFilePathName != "")
				escape = "";	// in case of file, there is no need of escape

			{
				ffmpegText = regex_replace(ffmpegText, regex(":"), escape + ":");
				ffmpegText = regex_replace(ffmpegText,
					regex("days_counter"), "%{eif" + escape + ":trunc((countDownDurationInSecs-t)/86400)" + escape + ":d" + escape + ":2}");
				ffmpegText = regex_replace(ffmpegText,
					regex("hours_counter"), "%{eif" + escape + ":trunc(mod(((countDownDurationInSecs-t)/3600),24))" + escape + ":d" + escape + ":2}");
				ffmpegText = regex_replace(ffmpegText,
					regex("mins_counter"), "%{eif" + escape + ":trunc(mod(((countDownDurationInSecs-t)/60),60))" + escape + ":d" + escape + ":2}");
				ffmpegText = regex_replace(ffmpegText,
					regex("secs_counter"), "%{eif" + escape + ":trunc(mod(countDownDurationInSecs-t" + escape + ",60))" + escape + ":d" + escape + ":2}");
				ffmpegText = regex_replace(ffmpegText,
					regex("cents_counter"), "%{eif" + escape + ":(mod(countDownDurationInSecs-t" + escape + ",1)*pow(10,2))" + escape + ":d" + escape + ":2}");
				ffmpegText = regex_replace(ffmpegText,
					regex("countDownDurationInSecs"), to_string(streamingDurationInSeconds));
			}

			if (textFilePathName != "")
			{
				ofstream of(textFilePathName, ofstream::trunc);
				of << ffmpegText;
				of.flush();
			}
		}

		/*
		* -vf "drawtext=fontfile='C\:\\Windows\\fonts\\Arial.ttf':
		fontcolor=yellow:fontsize=45:x=100:y=65:
		text='%{eif\:trunc((5447324-t)/86400)\:d\:2} days 
		%{eif\:trunc(mod(((5447324-t)/3600),24))\:d\:2} hrs
		%{eif\:trunc(mod(((5447324-t)/60),60))\:d\:2} m
		%{eif\:trunc(mod(5447324-t\,60))\:d\:2} s'"

		* 5447324 is the countdown duration expressed in seconds
		*/
		string ffmpegTextPosition_X_InPixel = 
			regex_replace(textPosition_X_InPixel, regex("video_width"), "w");
		ffmpegTextPosition_X_InPixel = 
			regex_replace(ffmpegTextPosition_X_InPixel, regex("text_width"), "text_w"); // text_w or tw
		ffmpegTextPosition_X_InPixel = 
			regex_replace(ffmpegTextPosition_X_InPixel, regex("line_width"), "line_w");
		ffmpegTextPosition_X_InPixel = 
			regex_replace(ffmpegTextPosition_X_InPixel, regex("timestampInSeconds"), "t");

		string ffmpegTextPosition_Y_InPixel = 
			regex_replace(textPosition_Y_InPixel, regex("video_height"), "h");
		ffmpegTextPosition_Y_InPixel = 
			regex_replace(ffmpegTextPosition_Y_InPixel, regex("text_height"), "text_h");
		ffmpegTextPosition_Y_InPixel = 
			regex_replace(ffmpegTextPosition_Y_InPixel, regex("line_height"), "line_h");
		ffmpegTextPosition_Y_InPixel = 
			regex_replace(ffmpegTextPosition_Y_InPixel, regex("timestampInSeconds"), "t");

		if (textFilePathName != "")
		{
			ffmpegDrawTextFilter = string("drawtext=textfile='") + textFilePathName + "'";
			if (reloadAtFrameInterval > 0)
				ffmpegDrawTextFilter += (":reload=" + to_string(reloadAtFrameInterval));
		}
		else
			ffmpegDrawTextFilter = string("drawtext=text='") + ffmpegText + "'";
		if (textPosition_X_InPixel != "")
			ffmpegDrawTextFilter += (":x=" + ffmpegTextPosition_X_InPixel);
		if (textPosition_Y_InPixel != "")
			ffmpegDrawTextFilter += (":y=" + ffmpegTextPosition_Y_InPixel);               
		if (fontType != "")
			ffmpegDrawTextFilter += (":fontfile='" + _ffmpegTtfFontDir + "/" + fontType + "'");
		if (fontSize != -1)
			ffmpegDrawTextFilter += (":fontsize=" + to_string(fontSize));
		if (fontColor != "")
		{
			ffmpegDrawTextFilter += (":fontcolor=" + fontColor);                
			if (textPercentageOpacity != -1)
			{
				char opacity[64];

				sprintf(opacity, "%.1f", ((float) textPercentageOpacity) / 100.0);

				ffmpegDrawTextFilter += ("@" + string(opacity));                
			}
		}
		ffmpegDrawTextFilter += (":shadowx=" + to_string(shadowX));
		ffmpegDrawTextFilter += (":shadowy=" + to_string(shadowY));
		if (boxEnable)
		{
			ffmpegDrawTextFilter += (":box=1");

			if (boxColor != "")
			{
				ffmpegDrawTextFilter += (":boxcolor=" + boxColor);                
				if (boxPercentageOpacity != -1)
				{
					char opacity[64];

					sprintf(opacity, "%.1f", ((float) boxPercentageOpacity) / 100.0);

					ffmpegDrawTextFilter += ("@" + string(opacity));                
				}
			}
		}
	}

	_logger->info(__FILEREF__ + "getDrawTextVideoFilterDescription"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", text: " + text
		+ ", textPosition_X_InPixel: " + textPosition_X_InPixel
		+ ", textPosition_Y_InPixel: " + textPosition_Y_InPixel
		+ ", fontType: " + fontType
		+ ", fontSize: " + to_string(fontSize)
		+ ", fontColor: " + fontColor
		+ ", textPercentageOpacity: " + to_string(textPercentageOpacity)
		+ ", boxEnable: " + to_string(boxEnable)
		+ ", boxColor: " + boxColor
		+ ", boxPercentageOpacity: " + to_string(boxPercentageOpacity)
		+ ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
		+ ", ffmpegDrawTextFilter: " + ffmpegDrawTextFilter
	);

	return ffmpegDrawTextFilter;
}

void FFMpeg::videoSpeed(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,

        string videoSpeedType,
        int videoSpeedSize,

		Json::Value encodingProfileDetailsRoot,

        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "videoSpeed";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		videoDurationInMilliSeconds,
		mmsSourceVideoAssetPathName,
		stagingEncodedAssetPathName
	);

    try
    {
		if (!FileIO::fileExisting(mmsSourceVideoAssetPathName)        
			&& !FileIO::directoryExisting(mmsSourceVideoAssetPathName)
		)
		{
			string errorMessage = string("Source video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegEncodingProfileArgumentList;
		if (encodingProfileDetailsRoot != Json::nullValue)
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

					string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to false"
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
			catch(runtime_error e)
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

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + "_"
				+ sUtcTimestamp
                + ".ffmpegoutput.log";
		}


        {
			string videoPTS;
			string audioTempo;

			if (videoSpeedType == "SlowDown")
			{
				switch(videoSpeedSize)
				{
					case 1:
						videoPTS = "1.1";
						audioTempo = "(1/1.1)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds / 100);

						break;
					case 2:
						videoPTS = "1.2";
						audioTempo = "(1/1.2)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 20 / 100);

						break;
					case 3:
						videoPTS = "1.3";
						audioTempo = "(1/1.3)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 30 / 100);

						break;
					case 4:
						videoPTS = "1.4";
						audioTempo = "(1/1.4)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 40 / 100);

						break;
					case 5:
						videoPTS = "1.5";
						audioTempo = "(1/1.5)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 50 / 100);

						break;
					case 6:
						videoPTS = "1.6";
						audioTempo = "(1/1.6)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 60 / 100);

						break;
					case 7:
						videoPTS = "1.7";
						audioTempo = "(1/1.7)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 70 / 100);

						break;
					case 8:
						videoPTS = "1.8";
						audioTempo = "(1/1.8)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 80 / 100);

						break;
					case 9:
						videoPTS = "1.9";
						audioTempo = "(1/1.9)";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 90 / 100);

						break;
					case 10:
						videoPTS = "2";
						audioTempo = "0.5";
						_currentDurationInMilliSeconds      += (videoDurationInMilliSeconds * 100 / 100);

						break;
					default:
						videoPTS = "1.3";
						audioTempo = "(1/1.3)";

						break;
				}
			}
			else // if (videoSpeedType == "SpeedUp")
			{
				switch(videoSpeedSize)
				{
					case 1:
						videoPTS = "(1/1.1)";
						audioTempo = "1.1";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 10 / 100);

						break;
					case 2:
						videoPTS = "(1/1.2)";
						audioTempo = "1.2";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 20 / 100);

						break;
					case 3:
						videoPTS = "(1/1.3)";
						audioTempo = "1.3";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 30 / 100);

						break;
					case 4:
						videoPTS = "(1/1.4)";
						audioTempo = "1.4";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 40 / 100);

						break;
					case 5:
						videoPTS = "(1/1.5)";
						audioTempo = "1.5";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 50 / 100);

						break;
					case 6:
						videoPTS = "(1/1.6)";
						audioTempo = "1.6";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 60 / 100);

						break;
					case 7:
						videoPTS = "(1/1.7)";
						audioTempo = "1.7";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 70 / 100);

						break;
					case 8:
						videoPTS = "(1/1.8)";
						audioTempo = "1.8";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 80 / 100);

						break;
					case 9:
						videoPTS = "(1/1.9)";
						audioTempo = "1.9";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 90 / 100);

						break;
					case 10:
						videoPTS = "0.5";
						audioTempo = "2";
						_currentDurationInMilliSeconds      -= (videoDurationInMilliSeconds * 100 / 100);

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
				if (encodingProfileDetailsRoot != Json::nullValue)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);
				}

				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "videoSpeed: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "videoSpeed: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "videoSpeed command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "videoSpeed: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }

            _logger->info(__FILEREF__ + "VideoSpeed file generated"
                + ", encodingJobKey: " + to_string(encodingJobKey)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);

            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;

                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }
        }        
    }
    catch(FFMpegEncodingKilledByUser e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg VideoSpeed failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg VideoSpeed failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg VideoSpeed failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
}

void FFMpeg::pictureInPicture(
	string mmsMainVideoAssetPathName,
	int64_t mainVideoDurationInMilliSeconds,
	string mmsOverlayVideoAssetPathName,
	int64_t overlayVideoDurationInMilliSeconds,
	bool soundOfMain,
	string overlayPosition_X_InPixel,
	string overlayPosition_Y_InPixel,
	string overlay_Width_InPixel,
	string overlay_Height_InPixel,

	Json::Value encodingProfileDetailsRoot,

	string stagingEncodedAssetPathName,
	int64_t encodingJobKey,
	int64_t ingestionJobKey,
	pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "pictureInPicture";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		mainVideoDurationInMilliSeconds,
		mmsMainVideoAssetPathName,
		stagingEncodedAssetPathName
	);

    try
    {
		if (!FileIO::fileExisting(mmsMainVideoAssetPathName)        
			&& !FileIO::directoryExisting(mmsMainVideoAssetPathName)
		)
		{
			string errorMessage = string("Main video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsMainVideoAssetPathName: " + mmsMainVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (!FileIO::fileExisting(mmsOverlayVideoAssetPathName)        
			&& !FileIO::directoryExisting(mmsOverlayVideoAssetPathName)
		)
		{
			string errorMessage = string("Overlay video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsOverlayVideoAssetPathName: " + mmsOverlayVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2022-12-09: Aggiunto "- 1000" perchè in un caso era stato generato l'errore anche
		// 	per pochi millisecondi di video overlay superiore al video main
		if (mainVideoDurationInMilliSeconds < overlayVideoDurationInMilliSeconds - 1000)
		{
			string errorMessage = __FILEREF__ + "pictureInPicture: overlay video duration cannot be bigger than main video diration"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mainVideoDurationInMilliSeconds: " + to_string(mainVideoDurationInMilliSeconds)
				+ ", overlayVideoDurationInMilliSeconds: " + to_string(overlayVideoDurationInMilliSeconds)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegEncodingProfileArgumentList;
		if (encodingProfileDetailsRoot != Json::nullValue)
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

					string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to false"
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
			catch(runtime_error e)
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

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + "_"
				+ sUtcTimestamp
                + ".ffmpegoutput.log";
		}


        {
            string ffmpegOverlayPosition_X_InPixel = 
                    regex_replace(overlayPosition_X_InPixel, regex("mainVideo_width"), "main_w");
            ffmpegOverlayPosition_X_InPixel = 
                    regex_replace(ffmpegOverlayPosition_X_InPixel, regex("overlayVideo_width"), "overlay_w");
            
            string ffmpegOverlayPosition_Y_InPixel = 
                    regex_replace(overlayPosition_Y_InPixel, regex("mainVideo_height"), "main_h");
            ffmpegOverlayPosition_Y_InPixel = 
                    regex_replace(ffmpegOverlayPosition_Y_InPixel, regex("overlayVideo_height"), "overlay_h");

			string ffmpegOverlay_Width_InPixel = 
				regex_replace(overlay_Width_InPixel, regex("overlayVideo_width"), "iw");

			string ffmpegOverlay_Height_InPixel = 
				regex_replace(overlay_Height_InPixel, regex("overlayVideo_height"), "ih");

			/*
            string ffmpegFilterComplex = string("-filter_complex 'overlay=")
                    + ffmpegImagePosition_X_InPixel + ":"
                    + ffmpegImagePosition_Y_InPixel + "'"
                    ;
			*/
            string ffmpegFilterComplex = string("-filter_complex ");
			if (soundOfMain)
				ffmpegFilterComplex += "[1]scale=";
			else
				ffmpegFilterComplex += "[0]scale=";
			ffmpegFilterComplex +=
				(ffmpegOverlay_Width_InPixel + ":" + ffmpegOverlay_Height_InPixel)
			;
			ffmpegFilterComplex += "[pip];";

			if (soundOfMain)
			{
				ffmpegFilterComplex += "[0][pip]overlay=";
			}
			else
			{
				ffmpegFilterComplex += "[pip][0]overlay=";
			}
			ffmpegFilterComplex +=
				(ffmpegOverlayPosition_X_InPixel + ":" + ffmpegOverlayPosition_Y_InPixel)
			;
			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
            {
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				if (soundOfMain)
				{
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(mmsMainVideoAssetPathName);
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(mmsOverlayVideoAssetPathName);
				}
				else
				{
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(mmsOverlayVideoAssetPathName);
					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(mmsMainVideoAssetPathName);
				}
				// output options
				FFMpegEncodingParameters::addToArguments(ffmpegFilterComplex, ffmpegArgumentList);

				// encoding parameters
				if (encodingProfileDetailsRoot != Json::nullValue)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);
				}

				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "pictureInPicture: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "pictureInPicture: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "pictureInPicture command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "pictureInPicture: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(
							chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }

            _logger->info(__FILEREF__ + "pictureInPicture file generated"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);

            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, pictureInPicture encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, pictureInPicture encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }
        }        
    }
    catch(FFMpegEncodingKilledByUser e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg pictureInPicture failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsMainVideoAssetPathName: " + mmsMainVideoAssetPathName
            + ", mmsOverlayVideoAssetPathName: " + mmsOverlayVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg pictureInPicture failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsMainVideoAssetPathName: " + mmsMainVideoAssetPathName
            + ", mmsOverlayVideoAssetPathName: " + mmsOverlayVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg pictureInPicture failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsMainVideoAssetPathName: " + mmsMainVideoAssetPathName
            + ", mmsOverlayVideoAssetPathName: " + mmsOverlayVideoAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
}

void FFMpeg::introOutroOverlay(
        string introVideoAssetPathName,
        int64_t introVideoDurationInMilliSeconds,
        string mainVideoAssetPathName,
        int64_t mainVideoDurationInMilliSeconds,
        string outroVideoAssetPathName,
        int64_t outroVideoDurationInMilliSeconds,

		int64_t introOverlayDurationInSeconds,
		int64_t outroOverlayDurationInSeconds,

		bool muteIntroOverlay,
		bool muteOutroOverlay,

		Json::Value encodingProfileDetailsRoot,

        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "introOutroOverlay";

	_logger->info(__FILEREF__ + "Received " + _currentApiName
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", introVideoAssetPathName: " + introVideoAssetPathName
		+ ", introVideoDurationInMilliSeconds: " + to_string(introVideoDurationInMilliSeconds)
		+ ", mainVideoAssetPathName: " + mainVideoAssetPathName
		+ ", mainVideoDurationInMilliSeconds: " + to_string(mainVideoDurationInMilliSeconds)
		+ ", outroVideoAssetPathName: " + outroVideoAssetPathName
		+ ", outroVideoDurationInMilliSeconds: " + to_string(outroVideoDurationInMilliSeconds)
		+ ", introOverlayDurationInSeconds: " + to_string(introOverlayDurationInSeconds)
		+ ", outroOverlayDurationInSeconds: " + to_string(outroOverlayDurationInSeconds)
		+ ", muteIntroOverlay: " + to_string(muteIntroOverlay)
		+ ", muteOutroOverlay: " + to_string(muteOutroOverlay)
		+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
	);

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		mainVideoDurationInMilliSeconds
			+ (introVideoDurationInMilliSeconds - introOverlayDurationInSeconds)
			+ (outroVideoDurationInMilliSeconds - outroOverlayDurationInSeconds),
		mainVideoAssetPathName,
		stagingEncodedAssetPathName
	);

    try
    {
		if (!FileIO::fileExisting(introVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", introVideoAssetPathName: " + introVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		else if (!FileIO::fileExisting(mainVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mainVideoAssetPathName: " + mainVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		else if (!FileIO::fileExisting(outroVideoAssetPathName))
		{
			string errorMessage = string("video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", outroVideoAssetPathName: " + outroVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (encodingProfileDetailsRoot == Json::nullValue)
		{
			string errorMessage = __FILEREF__ + "encodingProfileDetailsRoot is mandatory"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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

					string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to false"
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
			catch(runtime_error e)
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

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + "_"
				+ sUtcTimestamp
                + ".ffmpegoutput.log";
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
		{
			string ffmpegFilterComplex = "-filter_complex ";
			{
				long introStartOverlayInSeconds =
					(introVideoDurationInMilliSeconds - (introOverlayDurationInSeconds * 1000)) / 1000;
				long introVideoDurationInSeconds = introVideoDurationInMilliSeconds / 1000;
				long outroStartOverlayInSeconds = introStartOverlayInSeconds +
					(mainVideoDurationInMilliSeconds / 1000) - outroOverlayDurationInSeconds;

				if (introStartOverlayInSeconds < 0 || outroStartOverlayInSeconds < 0)
				{
					string errorMessage = __FILEREF__ + "introOutroOverlay: wrong durations"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", introStartOverlayInSeconds: " + to_string(introStartOverlayInSeconds)
						+ ", outroStartOverlayInSeconds: " + to_string(outroStartOverlayInSeconds)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				if (muteIntroOverlay)
					ffmpegFilterComplex += "[0:a]volume=enable='between(t," +
						to_string(introStartOverlayInSeconds) + "," +
						to_string(introVideoDurationInSeconds) + ")':volume=0[intro_overlay_muted];";
				ffmpegFilterComplex += "[1:v]tpad=start_duration=" + to_string(introStartOverlayInSeconds) +
					":start_mode=add:color=white[main_video_moved];";
				ffmpegFilterComplex += "[1:a]adelay=delays=" + to_string(introStartOverlayInSeconds) +
					"s:all=1[main_audio_moved];";
				ffmpegFilterComplex += "[2:v]setpts=PTS+" + to_string(outroStartOverlayInSeconds) +
					"/TB[outro_video_moved];";
				ffmpegFilterComplex += "[2:a]";
				if (muteOutroOverlay)
					ffmpegFilterComplex += "volume=enable='between(t,0," +
						to_string(outroOverlayDurationInSeconds) + ")':volume=0,";
				ffmpegFilterComplex += "adelay=delays=" + to_string(outroStartOverlayInSeconds) +
					"s:all=1[outro_audio_overlayMuted_and_moved];";
				ffmpegFilterComplex +=
					"[main_video_moved][0:v]overlay=eof_action=pass[overlay_intro_main];";
				ffmpegFilterComplex += "[overlay_intro_main][outro_video_moved]overlay=enable='gte(t," +
					to_string(outroStartOverlayInSeconds) + ")'[final_video];";
				ffmpegFilterComplex += "[main_audio_moved]";
				if (muteIntroOverlay)
					ffmpegFilterComplex += "[intro_overlay_muted]";
				else
					ffmpegFilterComplex += "[0:a]";
				ffmpegFilterComplex += "[outro_audio_overlayMuted_and_moved]amix=inputs=3[final_audio]";
			}

			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
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

				// encoding parameters
				for (string parameter: ffmpegEncodingProfileArgumentList)
					FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

				ffmpegArgumentList.push_back("-pix_fmt");
				// yuv420p: the only option for broad compatibility
				ffmpegArgumentList.push_back("yuv420p");

				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

					if (!ffmpegArgumentList.empty())
						copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
							ostream_iterator<string>(ffmpegArgumentListStream, " "));

                    _logger->info(__FILEREF__ + "introOutroOverlay: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                    );

					bool redirectionStdOutput = true;
					bool redirectionStdError = true;

					ProcessUtility::forkAndExec (
						_ffmpegPath + "/ffmpeg",
						ffmpegArgumentList,
						_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
						pChildPid, &iReturnedStatus);
					if (iReturnedStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "introOutroOverlay: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;
                        _logger->error(errorMessage);

						// to hide the ffmpeg staff
                        errorMessage = __FILEREF__ + "introOutroOverlay command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        ;
                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "introOutroOverlay: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" +
							to_string(chrono::duration_cast<chrono::seconds>(
							endFfmpegCommand - startFfmpegCommand).count()) + "@"
                    );
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
					string errorMessage;
					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
					else
						errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
							+ ", e.what(): " + e.what()
						;
                    _logger->error(errorMessage);

                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }

            _logger->info(__FILEREF__ + "introOutroOverlay file generated"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);

            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, pictureInPicture encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "command failed, pictureInPicture encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                throw runtime_error(errorMessage);
            }
        }        
    }
    catch(FFMpegEncodingKilledByUser e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg introOutroOverlay failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType =
				FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg introOutroOverlay failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType =
				FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: ffmpeg introOutroOverlay failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType =
				FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        throw e;
    }
}

pair<int64_t, long> FFMpeg::getMediaInfo(
	int64_t ingestionJobKey,
	bool isMMSAssetPathName,	// false means it is a URL
	string mediaSource,
	vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
	vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks)
{
	_currentApiName = "getMediaInfo";

	_logger->info(__FILEREF__ + "getMediaInfo"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", isMMSAssetPathName: " + to_string(isMMSAssetPathName)
		+ ", mediaSource: " + mediaSource
	);

	if (mediaSource == "")
	{
		string errorMessage = string("Media Source is wrong")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", mediaSource: " + mediaSource
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	// milli secs to wait in case of nfs delay
	if (isMMSAssetPathName)
	{
		if (!FileIO::fileExisting(mediaSource,
			_waitingNFSSync_maxMillisecondsToWait, _waitingNFSSync_milliSecondsWaitingBetweenChecks)        
			&& !FileIO::directoryExisting(mediaSource)
		)
		{
			string errorMessage = string("Source asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaSource: " + mediaSource
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		/*
		size_t fileNameIndex = mediaSource.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "ffmpeg: No fileName find in the asset path name"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaSource: " + mediaSource;
			_logger->error(errorMessage);
        
			throw runtime_error(errorMessage);
		}
    
		string sourceFileName = mediaSource.substr(fileNameIndex + 1);
		*/
	}

    string      detailsPathFileName =
            _ffmpegTempDir + "/" + to_string(ingestionJobKey) + ".json";
    
    /*
     * ffprobe:
        "-v quiet": Don't output anything else but the desired raw data value
        "-print_format": Use a certain format to print out the data
        "compact=": Use a compact output format
        "print_section=0": Do not print the section name
        ":nokey=1": do not print the key of the key:value pair
        ":escape=csv": escape the value
        "-show_entries format=duration": Get entries of a field named duration inside a section named format
    */
    string ffprobeExecuteCommand = 
		_ffmpegPath + "/ffprobe "
		// + "-v quiet -print_format compact=print_section=0:nokey=1:escape=csv -show_entries format=duration "
		+ "-v quiet -print_format json -show_streams -show_format \""
		+ mediaSource + "\" "
		+ "> " + detailsPathFileName 
		+ " 2>&1"
	;

    #ifdef __APPLE__
        ffprobeExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

	chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();
    try
    {
        _logger->info(__FILEREF__ + "getMediaInfo: Executing ffprobe command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
        );

		// The check/retries below was done to manage the scenario where the file was created
		// by another MMSEngine and it is not found just because of nfs delay.
		// Really, looking the log, we saw the file is just missing and it is not an nfs delay
		int attemptIndex = 0;
		bool executeDone = false;
		while (!executeDone)
		{
			int executeCommandStatus = ProcessUtility::execute(ffprobeExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = __FILEREF__
					+ "getMediaInfo: ffmpeg: ffprobe command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", executeCommandStatus: " + to_string(executeCommandStatus)
					+ ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__
					+ "getMediaInfo command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				throw runtime_error(errorMessage);
			}
			else
			{
				executeDone = true;
			}
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "getMediaInfo: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffprobeExecuteCommand: @" + ffprobeExecuteCommand + "@"
            + ", @FFMPEG statistics@ - duration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(
				endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                detailsPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "getMediaInfo: Executed ffmpeg command failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
            + ", @FFMPEG statistics@ - duration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(
				endFfmpegCommand - startFfmpegCommand).count()) + "@"
			+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
			+ ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);

        throw e;
    }

	int64_t durationInMilliSeconds = -1;
	long bitRate = -1;

    try
    {
        // json output will be like:
        /*
            {
                "streams": [
                    {
                        "index": 0,
                        "codec_name": "mpeg4",
                        "codec_long_name": "MPEG-4 part 2",
                        "profile": "Advanced Simple Profile",
                        "codec_type": "video",
                        "codec_time_base": "1/25",
                        "codec_tag_string": "XVID",
                        "codec_tag": "0x44495658",
                        "width": 712,
                        "height": 288,
                        "coded_width": 712,
                        "coded_height": 288,
                        "has_b_frames": 1,
                        "sample_aspect_ratio": "1:1",
                        "display_aspect_ratio": "89:36",
                        "pix_fmt": "yuv420p",
                        "level": 5,
                        "chroma_location": "left",
                        "refs": 1,
                        "quarter_sample": "false",
                        "divx_packed": "false",
                        "r_frame_rate": "25/1",
                        "avg_frame_rate": "25/1",
                        "time_base": "1/25",
                        "start_pts": 0,
                        "start_time": "0.000000",
                        "duration_ts": 142100,
                        "duration": "5684.000000",
                        "bit_rate": "873606",
                        "nb_frames": "142100",
                        "disposition": {
                            "default": 0,
                            "dub": 0,
                            "original": 0,
                            "comment": 0,
                            "lyrics": 0,
                            "karaoke": 0,
                            "forced": 0,
                            "hearing_impaired": 0,
                            "visual_impaired": 0,
                            "clean_effects": 0,
                            "attached_pic": 0,
                            "timed_thumbnails": 0
                        }
                    },
                    {
                        "index": 1,
                        "codec_name": "mp3",
                        "codec_long_name": "MP3 (MPEG audio layer 3)",
                        "codec_type": "audio",
                        "codec_time_base": "1/48000",
                        "codec_tag_string": "U[0][0][0]",
                        "codec_tag": "0x0055",
                        "sample_fmt": "s16p",
                        "sample_rate": "48000",
                        "channels": 2,
                        "channel_layout": "stereo",
                        "bits_per_sample": 0,
                        "r_frame_rate": "0/0",
                        "avg_frame_rate": "0/0",
                        "time_base": "3/125",
                        "start_pts": 0,
                        "start_time": "0.000000",
                        "duration_ts": 236822,
                        "duration": "5683.728000",
                        "bit_rate": "163312",
                        "nb_frames": "236822",
                        "disposition": {
                            "default": 0,
                            "dub": 0,
                            "original": 0,
                            "comment": 0,
                            "lyrics": 0,
                            "karaoke": 0,
                            "forced": 0,
                            "hearing_impaired": 0,
                            "visual_impaired": 0,
                            "clean_effects": 0,
                            "attached_pic": 0,
                            "timed_thumbnails": 0
                        }
                    }
                ],
                "format": {
                    "filename": "/Users/multi/VitadaCamper.avi",
                    "nb_streams": 2,
                    "nb_programs": 0,
                    "format_name": "avi",
                    "format_long_name": "AVI (Audio Video Interleaved)",
                    "start_time": "0.000000",
                    "duration": "5684.000000",
                    "size": "745871360",
                    "bit_rate": "1049783",
                    "probe_score": 100,
                    "tags": {
                        "encoder": "VirtualDubMod 1.5.10.2 (build 2540/release)"
                    }
                }
            }
         */

        ifstream detailsFile(detailsPathFileName);
        stringstream buffer;
        buffer << detailsFile.rdbuf();

        _logger->info(__FILEREF__ + "Details found"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mediaSource: " + mediaSource
            + ", details: " + buffer.str()
        );

        string mediaDetails = buffer.str();
        // LF and CR create problems to the json parser...
        while (mediaDetails.size() > 0 && (mediaDetails.back() == 10 || mediaDetails.back() == 13))
            mediaDetails.pop_back();

        Json::Value detailsRoot = JSONUtils::toJson(ingestionJobKey, -1, mediaDetails);

        string field = "streams";
        if (!JSONUtils::isMetadataPresent(detailsRoot, field))
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaSource: " + mediaSource
				+ ", Field: " + field;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
            errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaSource: " + mediaSource
				+ ", Field: " + field;
            throw runtime_error(errorMessage);
        }
        Json::Value streamsRoot = detailsRoot[field];
        bool videoFound = false;
        bool audioFound = false;
		string firstVideoCodecName;
        for(int streamIndex = 0; streamIndex < streamsRoot.size(); streamIndex++) 
        {
            Json::Value streamRoot = streamsRoot[streamIndex];
            
            field = "codec_type";
            if (!JSONUtils::isMetadataPresent(streamRoot, field))
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", mediaSource: " + mediaSource
                    + ", Field: " + field;
                _logger->error(errorMessage);

				// to hide the ffmpeg staff
                errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", mediaSource: " + mediaSource
                    + ", Field: " + field;
                throw runtime_error(errorMessage);
            }
            string codecType = streamRoot.get(field, "XXX").asString();
            
            if (codecType == "video")
            {
                videoFound = true;

				int trackIndex;
				int64_t videoDurationInMilliSeconds = -1;
				string videoCodecName;
				string videoProfile;
				int videoWidth = -1;
				int videoHeight = -1;
				string videoAvgFrameRate;
				long videoBitRate = -1;

                field = "index";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                trackIndex = JSONUtils::asInt(streamRoot, field, 0);

                field = "codec_name";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
					field = "codec_tag_string";
					if (!JSONUtils::isMetadataPresent(streamRoot, field))
					{
						string errorMessage = __FILEREF__
							+ "ffmpeg: Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", mediaSource: " + mediaSource
                            + ", Field: " + field;
						_logger->error(errorMessage);

						// to hide the ffmpeg staff
						errorMessage = __FILEREF__
							+ "Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", mediaSource: " + mediaSource
                            + ", Field: " + field;
						throw runtime_error(errorMessage);
					}
                }
                videoCodecName = streamRoot.get(field, "").asString();

				if (firstVideoCodecName == "")
					firstVideoCodecName = videoCodecName;

                field = "profile";
                if (JSONUtils::isMetadataPresent(streamRoot, field))
                    videoProfile = streamRoot.get(field, "XXX").asString();
                else
                {
                    /*
                    if (videoCodecName != "mjpeg")
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                                + ", mediaSource: " + mediaSource
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                     */
                }

                field = "width";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                videoWidth = JSONUtils::asInt(streamRoot, field, 0);

                field = "height";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                videoHeight = JSONUtils::asInt(streamRoot, field, 0);
                
                field = "avg_frame_rate";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                videoAvgFrameRate = streamRoot.get(field, "XXX").asString();

                field = "bit_rate";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    if (videoCodecName != "mjpeg")
                    {
                        // I didn't find bit_rate also in a ts file, let's set it as a warning
                        
                        string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", mediaSource: " + mediaSource
                            + ", Field: " + field;
                        _logger->warn(errorMessage);

                        // throw runtime_error(errorMessage);
                    }
                }
                else
                    videoBitRate = stol(streamRoot.get(field, "").asString());

				field = "duration";
				if (!JSONUtils::isMetadataPresent(streamRoot, field))
				{
					// I didn't find it in a .avi file generated using OpenCV::VideoWriter
					// let's log it as a warning
					if (videoCodecName != "" && videoCodecName != "mjpeg")
					{
						string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", mediaSource: " + mediaSource
							+ ", Field: " + field;
						_logger->warn(errorMessage);

						// throw runtime_error(errorMessage);
					}            
				}
				else
				{
					string duration = streamRoot.get(field, "0").asString();

					// 2020-01-13: atoll remove the milliseconds and this is wrong
					// durationInMilliSeconds = atoll(duration.c_str()) * 1000;

					double dDurationInMilliSeconds = stod(duration);
					videoDurationInMilliSeconds = dDurationInMilliSeconds * 1000;
				}

				videoTracks.push_back(make_tuple(trackIndex, videoDurationInMilliSeconds,
					videoCodecName, videoProfile, videoWidth, videoHeight,
					videoAvgFrameRate, videoBitRate));
            }
            else if (codecType == "audio")
            {
                audioFound = true;

				int trackIndex;
				int64_t audioDurationInMilliSeconds = -1;
				string audioCodecName;
				long audioSampleRate = -1;
				int audioChannels = -1;
				long audioBitRate = -1;

                field = "index";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                trackIndex = JSONUtils::asInt(streamRoot, field, 0);

                field = "codec_name";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                audioCodecName = streamRoot.get(field, "XXX").asString();

                field = "sample_rate";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                audioSampleRate = stol(streamRoot.get(field, "XXX").asString());

                field = "channels";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->error(errorMessage);

					// to hide the ffmpeg staff
                    errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    throw runtime_error(errorMessage);
                }
                audioChannels = JSONUtils::asInt(streamRoot, field, 0);
                
                field = "bit_rate";
                if (!JSONUtils::isMetadataPresent(streamRoot, field))
                {
                    // I didn't find bit_rate in a webm file, let's set it as a warning

                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", mediaSource: " + mediaSource
                        + ", Field: " + field;
                    _logger->warn(errorMessage);

                    // throw runtime_error(errorMessage);
                }
				else
					audioBitRate = stol(streamRoot.get(field, "XXX").asString());

				field = "duration";
				if (JSONUtils::isMetadataPresent(streamRoot, field))
				{
					string duration = streamRoot.get(field, "0").asString();

					// 2020-01-13: atoll remove the milliseconds and this is wrong
					// durationInMilliSeconds = atoll(duration.c_str()) * 1000;

					double dDurationInMilliSeconds = stod(duration);
					audioDurationInMilliSeconds = dDurationInMilliSeconds * 1000;
				}

				string language;
                string tagsField = "tags";
                if (JSONUtils::isMetadataPresent(streamRoot, tagsField))
                {
					field = "language";
					if (JSONUtils::isMetadataPresent(streamRoot[tagsField], field))
					{
						language = streamRoot[tagsField].get(field, "").asString();
					}
                }

				audioTracks.push_back(make_tuple(trackIndex, audioDurationInMilliSeconds,
					audioCodecName, audioSampleRate, audioChannels, audioBitRate, language));
            }
        }

        field = "format";
        if (!JSONUtils::isMetadataPresent(detailsRoot, field))
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", mediaSource: " + mediaSource
                + ", Field: " + field;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaSource: " + mediaSource
				+ ", Field: " + field;
            throw runtime_error(errorMessage);
        }
        Json::Value formatRoot = detailsRoot[field];

        field = "duration";
        if (!JSONUtils::isMetadataPresent(formatRoot, field))
        {
			// I didn't find it in a .avi file generated using OpenCV::VideoWriter
			// let's log it as a warning
            if (firstVideoCodecName != "" && firstVideoCodecName != "mjpeg")
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", mediaSource: " + mediaSource
                    + ", Field: " + field;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }            
        }
        else
        {
            string duration = formatRoot.get(field, "XXX").asString();

			// 2020-01-13: atoll remove the milliseconds and this is wrong
            // durationInMilliSeconds = atoll(duration.c_str()) * 1000;

            double dDurationInMilliSeconds = stod(duration);
            durationInMilliSeconds = dDurationInMilliSeconds * 1000;
        }

        field = "bit_rate";
        if (!JSONUtils::isMetadataPresent(formatRoot, field))
        {
            if (firstVideoCodecName != "" && firstVideoCodecName != "mjpeg")
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", mediaSource: " + mediaSource
                    + ", firstVideoCodecName: " + firstVideoCodecName;
                    + ", Field: " + field;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }            
        }
        else
        {
            string bit_rate = formatRoot.get(field, "XXX").asString();
            bitRate = atoll(bit_rate.c_str());
        }

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);
    }
    catch(runtime_error e)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: error processing ffprobe output"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);

        throw e;
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: error processing ffprobe output"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);

        throw e;
    }

    /*
    if (durationInMilliSeconds == -1)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: durationInMilliSeconds was not able to be retrieved from media"
                + ", mediaSource: " + mediaSource
                + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds);
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    else if (width == -1 || height == -1)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: width/height were not able to be retrieved from media"
                + ", mediaSource: " + mediaSource
                + ", width: " + to_string(width)
                + ", height: " + to_string(height)
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
     */
    
	/*
    _logger->info(__FILEREF__ + "FFMpeg::getMediaInfo"
        + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
        + ", bitRate: " + to_string(bitRate)
        + ", videoCodecName: " + videoCodecName
        + ", videoProfile: " + videoProfile
        + ", videoWidth: " + to_string(videoWidth)
        + ", videoHeight: " + to_string(videoHeight)
        + ", videoAvgFrameRate: " + videoAvgFrameRate
        + ", videoBitRate: " + to_string(videoBitRate)
        + ", audioCodecName: " + audioCodecName
        + ", audioSampleRate: " + to_string(audioSampleRate)
        + ", audioChannels: " + to_string(audioChannels)
        + ", audioBitRate: " + to_string(audioBitRate)
    );
	*/

	return make_pair(durationInMilliSeconds, bitRate);
}

int FFMpeg::probeChannel(
	int64_t ingestionJobKey,
	string url
)
{
	_currentApiName = "probeChannel";

    string outputProbePathFileName =
		_ffmpegTempDir + "/"
		+ to_string(ingestionJobKey)
		+ ".probeChannel.log"
	;
        
	_logger->info(__FILEREF__ + _currentApiName
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url
			);

	// 2021-09-18: l'idea sarebbe che se il comando ritorna 124 (timeout), vuol dire
	//	che il play ha strimmato per i secondi indicati ed è stato fermato dal comando
	//	di timeout. Quindi in questo caso l'url funziona bene.
	//	Se invece ritorna 0 vuol dire che ffplay non è riuscito a strimmare, c'è
	//	stato un errore. Quindi in questo caso l'url NON funziona bene.
    string probeExecuteCommand = 
		string("timeout 10 ") + _ffmpegPath + "/ffplay \""
		+ url + "\" "
		+ "> " + outputProbePathFileName 
		+ " 2>&1"
	;

	int executeCommandStatus = 0;
    try
    {
        _logger->info(__FILEREF__ + _currentApiName + ": Executing ffplay command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", probeExecuteCommand: " + probeExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		executeCommandStatus = ProcessUtility::execute(probeExecuteCommand);
		if (executeCommandStatus != 124)
		{
			string errorMessage = __FILEREF__
				+ _currentApiName + ": ffmpeg: probe command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", executeCommandStatus: " + to_string(executeCommandStatus)
				+ ", probeExecuteCommand: " + probeExecuteCommand
			;
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__
				+ _currentApiName + ": probe command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			;
			throw runtime_error(errorMessage);
		}

        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + _currentApiName + ": Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", probeExecuteCommand: " + probeExecuteCommand
            + ", @FFMPEG statistics@ - duration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(
					endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
			outputProbePathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg: probe command failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", probeExecuteCommand: " + probeExecuteCommand
			+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
			+ ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", outputProbePathFileName: " + outputProbePathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(outputProbePathFileName, exceptionInCaseOfError);

        // throw e;
		return 1;
    }

	return 0;
}

void FFMpeg::muxAllFiles(
	int64_t ingestionJobKey,
	vector<string> sourcesPathName,
	string destinationPathName
)
{
	_currentApiName = "muxAllFiles";

	_logger->info(__FILEREF__ + _currentApiName
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", destinationPathName: " + destinationPathName
			);

	for (string sourcePathName: sourcesPathName)
	{
		// milli secs to wait in case of nfs delay
		if (!FileIO::fileExisting(sourcePathName,
			_waitingNFSSync_maxMillisecondsToWait, _waitingNFSSync_milliSecondsWaitingBetweenChecks)        
		)
		{
			string errorMessage = string("Source asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourcePathName: " + sourcePathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
	}

    string ffmpegExecuteCommand = 
		_ffmpegPath + "/ffmpeg "
	;
	for (string sourcePathName: sourcesPathName)
		ffmpegExecuteCommand += "-i " + sourcePathName + " ";
	ffmpegExecuteCommand += "-c copy ";
	for (int sourceIndex = 0; sourceIndex < sourcesPathName.size(); sourceIndex++)
		ffmpegExecuteCommand += "-map " + to_string(sourceIndex) + " ";
	ffmpegExecuteCommand += destinationPathName;

    try
    {
		_logger->info(__FILEREF__ + _currentApiName + ": Executing ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
		);

		chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__
				+ _currentApiName + ": ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", executeCommandStatus: " + to_string(executeCommandStatus)
				+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
			;
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__
				+ _currentApiName + ": command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			;
			throw runtime_error(errorMessage);
		}

        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + _currentApiName + ": Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", @FFMPEG statistics@ - duration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string errorMessage = __FILEREF__ + "ffmpeg command failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
			+ ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

		// to hide the ffmpeg staff
        errorMessage = __FILEREF__ + "command failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", e.what(): " + e.what()
        ;
        throw e;
    }
}

void FFMpeg::getLiveStreamingInfo(
	string liveURL,
	string userAgent,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	vector<tuple<int, string, string, string, string, int, int>>& videoTracks,
	vector<tuple<int, string, string, string, int, bool>>& audioTracks
)
{

	_logger->info(__FILEREF__ + "getLiveStreamingInfo"
		+ ", liveURL: " + liveURL
		+ ", userAgent: " + userAgent
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
	);

	string outputFfmpegPathFileName;
	string ffmpegExecuteCommand;

    try
    {
		outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "_"
			+ to_string(encodingJobKey)
			+ ".liveStreamingInfo.log"
		;

		int liveDurationInSeconds = 5;

		// user agent is an HTTP header and can be used only in case of http request
		bool userAgentToBeUsed = false;
		if (userAgent != "")
		{
			string httpPrefix = "http";	// it includes also https
			if (liveURL.size() >= httpPrefix.size()
				&& liveURL.compare(0, httpPrefix.size(), httpPrefix) == 0)
			{
				userAgentToBeUsed = true;
			}
			else
			{
				_logger->warn(__FILEREF__ + "getLiveStreamingInfo: user agent cannot be used if not http"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", liveURL: " + liveURL
				);
			}
		}

		ffmpegExecuteCommand = _ffmpegPath
			+ "/ffmpeg "
			+ "-nostdin "
			+ (userAgentToBeUsed ? ("-user_agent \"" + userAgent + "\" ") : "")
			+ "-re -i \"" + liveURL + "\" "
			+ "-t " + to_string(liveDurationInSeconds) + " "
			+ "-c:v copy "
			+ "-c:a copy "
			+ "-f null "
			+ "/dev/null "
			+ "> " + outputFfmpegPathFileName 
			+ " 2>&1"
		;

		#ifdef __APPLE__
			ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
		#endif

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "getLiveStreamingInfo: Executing ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );
		int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
		if (executeCommandStatus != 0)
		{
			string errorMessage = __FILEREF__ +
				"getLiveStreamingInfo failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", executeCommandStatus: " + to_string(executeCommandStatus)
				+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
			;
			_logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ +
				"getLiveStreamingInfo failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
			;
			throw runtime_error(errorMessage);
		}
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "getLiveStreamingInfo: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", @FFMPEG statistics@ - duration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(
			outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage = __FILEREF__ + "getLiveStreamingInfo failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
			+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
			+ ", e.what(): " + e.what()
		;
		_logger->error(errorMessage);

		if (FileIO::isFileExisting(outputFfmpegPathFileName.c_str()))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", outputFfmpegPathFileName: " + outputFfmpegPathFileName);
			bool exceptionInCaseOfError = false;
			FileIO::remove(outputFfmpegPathFileName, exceptionInCaseOfError);
		}

		throw e;
	}

	try
	{
		if (!FileIO::isFileExisting(outputFfmpegPathFileName.c_str()))
		{
			_logger->info(__FILEREF__ + "ffmpeg: ffmpeg status not available"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", outputFfmpegPathFileName: " + outputFfmpegPathFileName
			);

			throw FFMpegEncodingStatusNotAvailable();
		}

		ifstream ifPathFileName(outputFfmpegPathFileName);

		int charsToBeRead = 200000;

		// get size/length of file:
		ifPathFileName.seekg (0, ifPathFileName.end);
		int fileSize = ifPathFileName.tellg();
		if (fileSize <= charsToBeRead)
			charsToBeRead = fileSize;

		ifPathFileName.seekg (0, ifPathFileName.beg);

		string firstPartOfFile;
		{
			char* buffer = new char [charsToBeRead];
			ifPathFileName.read (buffer, charsToBeRead);
			if (ifPathFileName)
			{
				// all characters read successfully
				firstPartOfFile.assign(buffer, charsToBeRead);
			}
			else
			{
				// error: only is.gcount() could be read";
				firstPartOfFile.assign(buffer, ifPathFileName.gcount());
			}
			ifPathFileName.close();

			delete[] buffer;
		}

		/*
  Program 0
    Metadata:
      variant_bitrate : 200000
    Stream #0:0: Video: h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 320x180 [SAR 1:1 DAR 16:9], 15 fps, 15 tbr, 90k tbn, 30 tbc
    Metadata:
      variant_bitrate : 200000
    Stream #0:1: Audio: aac (LC) ([15][0][0][0] / 0x000F), 32000 Hz, stereo, fltp
    Metadata:
      variant_bitrate : 200000
    Stream #0:2: Data: timed_id3 (ID3  / 0x20334449)
    Metadata:
      variant_bitrate : 200000
  Program 1
    Metadata:
      variant_bitrate : 309000
    Stream #0:3: Video: h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 480x270 [SAR 1:1 DAR 16:9], 25 fps, 25 tbr, 90k tbn, 50 tbc
    Metadata:
      variant_bitrate : 309000
    Stream #0:4: Audio: aac (LC) ([15][0][0][0] / 0x000F), 32000 Hz, stereo, fltp
    Metadata:
      variant_bitrate : 309000
    Stream #0:5: Data: timed_id3 (ID3  / 0x20334449)
    Metadata:
      variant_bitrate : 309000
...
Output #0, flv, to 'rtmp://prg-1.s.cdn77.com:1936/static/1620280677?password=DMGBKQCH':
		 */
		string programLabel = "  Program ";
		string outputLabel = "Output #0,";
		string streamLabel = "    Stream #";
		string videoLabel = "Video: ";
		string audioLabel = "Audio: ";
		string stereoLabel = ", stereo,";

		int startToLookForProgram = 0;
		bool programFound = true;
		int programId = 0;
		while(programFound)
		{
			string program;
			{
				/*
				_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Looking Program"
					+ ", programId: " + to_string(programId)
					+ ", startToLookForProgram: " + to_string(startToLookForProgram)
				);
				*/
				size_t programBeginIndex = firstPartOfFile.find(programLabel, startToLookForProgram);
				if (programBeginIndex == string::npos)
				{
					programFound = false;

					continue;
				}

				startToLookForProgram = (programBeginIndex + programLabel.size());
				size_t programEndIndex = firstPartOfFile.find(programLabel, startToLookForProgram);
				if (programEndIndex == string::npos)
				{
					programEndIndex = firstPartOfFile.find(outputLabel, startToLookForProgram);
					if (programEndIndex == string::npos)
					{
						/*
						_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. This is the last Program"
							+ ", programId: " + to_string(programId)
							+ ", startToLookForProgram: " + to_string(startToLookForProgram)
						);
						*/

						programFound = false;

						continue;
					}
				}

				program = firstPartOfFile.substr(programBeginIndex,
					programEndIndex - programBeginIndex);

				/*
				_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Program"
					+ ", programId: " + to_string(programId)
					+ ", programBeginIndex: " + to_string(programBeginIndex)
					+ ", programEndIndex: " + to_string(programEndIndex)
					+ ", program: " + program
				);
				*/
			}

			stringstream programStream(program);
			string line;

			while (getline(programStream, line))
			{
				// start with
				if (line.size() >= streamLabel.size()
					&& 0 == line.compare(0, streamLabel.size(), streamLabel))
				{
					size_t codecStartIndex;
					if ((codecStartIndex = line.find(videoLabel)) != string::npos)
					{
						//     Stream #0:0: Video: h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 320x180 [SAR 1:1 DAR 16:9], 15 fps, 15 tbr, 90k tbn, 30 tbc

						// video
						string videoStreamId;
						string videoStreamDescription;
						string videoCodec;
						string videoYUV;
						int videoWidth = -1;
						int videoHeight = -1;

						// stream id
						{
							string streamIdEndLabel = ": ";
							size_t streamIdEndIndex;
							if ((streamIdEndIndex = line.find(streamIdEndLabel, streamLabel.size())) != string::npos)
							{
								size_t streamIdStartIndex = streamLabel.size();

								// Stream #0:2(des): Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								//	or
								// Stream #0:2: Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								string localStreamId = line.substr(streamIdStartIndex,
									streamIdEndIndex - streamIdStartIndex);

								string streamDescriptionStartLabel = "(";
								size_t streamDescriptionStartIndex;
								if ((streamDescriptionStartIndex = localStreamId.find(streamDescriptionStartLabel, 0)) != string::npos)
								{
									// 0:2(des)
									videoStreamId = localStreamId.substr(0, streamDescriptionStartIndex);
									videoStreamDescription = localStreamId.substr(streamDescriptionStartIndex + 1,
											(localStreamId.size() - 2) - streamDescriptionStartIndex);
								}
								else
								{
									// 0:2
									videoStreamId = localStreamId;
								}
							}
						}

						// codec
						size_t codecEndIndex;
						{
							string codecEndLabel = ", yuv";
							if ((codecEndIndex = line.find(codecEndLabel, codecStartIndex)) != string::npos)
							{
								codecStartIndex += videoLabel.size();

								videoCodec = line.substr(codecStartIndex, codecEndIndex - codecStartIndex);
							}
						}

						// yuv
						size_t yuvEndIndex;
						{
							// h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv), 624x352
							//	or
							// h264 (Main) ([27][0][0][0] / 0x001B), yuv420p, 928x522
							//  or
							// h264 (Main) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709), 320x180
							string yuvEndLabel_1 = "), ";
							string yuvEndLabel_2 = ", ";
							if ((yuvEndIndex = line.find(yuvEndLabel_1, codecEndIndex + 1)) != string::npos)
							{
								videoYUV = line.substr(codecEndIndex + 2,
										(yuvEndIndex + 1) - (codecEndIndex + 2));
							}
							else if ((yuvEndIndex = line.find(yuvEndLabel_2, codecEndIndex + 1)) != string::npos)
							{
								videoYUV = line.substr(codecEndIndex + 2,
										yuvEndIndex - (codecEndIndex + 2));
							}
						}

						// width & height
						if (yuvEndIndex != string::npos)
						{
							string widthEndLabel = "x";
							size_t widthEndIndex;
							if ((widthEndIndex = line.find(widthEndLabel, yuvEndIndex + 2)) != string::npos)
							{
								string sWidth = line.substr(yuvEndIndex + 2,
									widthEndIndex - (yuvEndIndex + 2));
								try
								{
									videoWidth = stoi(sWidth);
								}
								catch(exception e)
								{
									string errorMessage = __FILEREF__ + "getLiveStreamingInfo error"
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", line: " + line
										+ ", yuvEndIndex: " + to_string(yuvEndIndex)
										+ ", sWidth: " + sWidth
										+ ", e.what(): " + e.what()
									;
									_logger->error(errorMessage);
								}
							}

							string heightEndLabel = " ";
							size_t heightEndIndex;
							if ((heightEndIndex = line.find(heightEndLabel, widthEndIndex)) != string::npos)
							{
								string sHeight = line.substr(widthEndIndex + 1,
									heightEndIndex - (widthEndIndex + 1));
								try
								{
									videoHeight = stoi(sHeight);
								}
								catch(exception e)
								{
									string errorMessage = __FILEREF__ + "getLiveStreamingInfo error"
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", line: " + line
										+ ", sHeight: " + sHeight
										+ ", e.what(): " + e.what()
									;
									_logger->error(errorMessage);
								}
							}
						}

						{
							videoTracks.push_back(
								make_tuple(programId, videoStreamId, videoStreamDescription,
									videoCodec, videoYUV, videoWidth, videoHeight)
							);

							_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Video track"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", programId: " + to_string(programId)
								+ ", videoStreamId: " + videoStreamId
								+ ", videoStreamDescription: " + videoStreamDescription
								+ ", videoCodec: " + videoCodec
								+ ", videoYUV: " + videoYUV
								+ ", videoWidth: " + to_string(videoWidth)
								+ ", videoHeight: " + to_string(videoHeight)
							);
						}
					}
					else if ((codecStartIndex = line.find(audioLabel)) != string::npos)
					{
						// Stream #0:1: Audio: aac (LC) ([15][0][0][0] / 0x000F), 32000 Hz, stereo, fltp

						// audio
						string audioStreamId;
						string audioStreamDescription;
						string audioCodec;
						int audioSamplingRate = -1;
						bool audioStereo = false;

						// stream id
						{
							string streamIdEndLabel = ": ";
							size_t streamIdEndIndex;
							if ((streamIdEndIndex = line.find(streamIdEndLabel, streamLabel.size())) != string::npos)
							{
								size_t streamIdStartIndex = streamLabel.size();

								// Stream #0:2(des): Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								//	or
								// Stream #0:2: Audio: aac (LC), 44100 Hz, stereo, fltp, 168 kb/s
								string localStreamId = line.substr(streamIdStartIndex,
									streamIdEndIndex - streamIdStartIndex);

								string streamDescriptionStartLabel = "(";
								size_t streamDescriptionStartIndex;
								if ((streamDescriptionStartIndex = localStreamId.find(streamDescriptionStartLabel, 0)) != string::npos)
								{
									// 0:2(des)
									audioStreamId = localStreamId.substr(0, streamDescriptionStartIndex);
									audioStreamDescription = localStreamId.substr(streamDescriptionStartIndex + 1,
											(localStreamId.size() - 2) - streamDescriptionStartIndex);
								}
								else
								{
									// 0:2
									audioStreamId = localStreamId;
								}
							}
						}

						// codec
						size_t codecEndIndex;
						{
							string codecEndLabel = ", ";
							if ((codecEndIndex = line.find(codecEndLabel, codecStartIndex)) != string::npos)
							{
								codecStartIndex += audioLabel.size();

								audioCodec = line.substr(codecStartIndex, codecEndIndex - codecStartIndex);
							}
						}

						// samplingRate
						size_t samplingEndIndex = 0;
						{
							string samplingEndLabel = " Hz";
							if ((samplingEndIndex = line.find(samplingEndLabel, codecEndIndex)) != string::npos)
							{
								string sSamplingRate = line.substr(codecEndIndex + 2,
									samplingEndIndex - (codecEndIndex + 2));
								try
								{
									audioSamplingRate = stoi(sSamplingRate);
								}
								catch(exception e)
								{
									string errorMessage = __FILEREF__ + "getLiveStreamingInfo error"
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", line: " + line
										+ ", sSamplingRate: " + sSamplingRate
										+ ", e.what(): " + e.what()
									;
									_logger->error(errorMessage);
								}
							}
						}

						// stereo
						{
							if (line.find(stereoLabel, samplingEndIndex) != string::npos)
								audioStereo = true;
							else
								audioStereo = false;
						}

						{
							audioTracks.push_back(
								make_tuple(programId, audioStreamId, audioStreamDescription,
									audioCodec, audioSamplingRate, audioStereo)
							);

							_logger->info(__FILEREF__ + "FFMpeg::getLiveStreamingInfo. Audio track"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", programId: " + to_string(programId)
								+ ", audioStreamId: " + audioStreamId
								+ ", audioStreamDescription: " + audioStreamDescription
								+ ", audioCodec: " + audioCodec
								+ ", audioSamplingRate: " + to_string(audioSamplingRate)
								+ ", audioStereo: " + to_string(audioStereo)
							);
						}
					}
				}
			}

			programId++;
		}

		_logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", outputFfmpegPathFileName: " + outputFfmpegPathFileName);
		bool exceptionInCaseOfError = false;
		FileIO::remove(outputFfmpegPathFileName, exceptionInCaseOfError);
	}
    catch(runtime_error e)
    {
        string errorMessage = __FILEREF__ + "getLiveStreamingInfo error"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove"
			+ ", outputFfmpegPathFileName: " + outputFfmpegPathFileName);
		bool exceptionInCaseOfError = false;
		FileIO::remove(outputFfmpegPathFileName, exceptionInCaseOfError);

        throw e;
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "getLiveStreamingInfo error"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

		_logger->info(__FILEREF__ + "Remove"
			+ ", outputFfmpegPathFileName: " + outputFfmpegPathFileName);
		bool exceptionInCaseOfError = false;
		FileIO::remove(outputFfmpegPathFileName, exceptionInCaseOfError);

        throw e;
    }
}

void FFMpeg::generateFrameToIngest(
	int64_t ingestionJobKey,
	string mmsAssetPathName,
	int64_t videoDurationInMilliSeconds,
	double startTimeInSeconds,
	string frameAssetPathName,
	int imageWidth,
	int imageHeight,
	pid_t* pChildPid)
{
	_currentApiName = "generateFrameToIngest";

	setStatus(
		ingestionJobKey,
		-1,	// encodingJobKey,
		videoDurationInMilliSeconds,
		mmsAssetPathName
	);

    _logger->info(__FILEREF__ + "generateFrameToIngest"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", mmsAssetPathName: " + mmsAssetPathName
        + ", videoDurationInMilliSeconds: " + to_string(videoDurationInMilliSeconds)
        + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
        + ", frameAssetPathName: " + frameAssetPathName
        + ", imageWidth: " + to_string(imageWidth)
        + ", imageHeight: " + to_string(imageHeight)
    );

	if (!FileIO::fileExisting(mmsAssetPathName)
		&& !FileIO::directoryExisting(mmsAssetPathName)
	)
	{
		string errorMessage = string("Asset path name not existing")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", mmsAssetPathName: " + mmsAssetPathName
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	int iReturnedStatus = 0;

	{
		char	sUtcTimestamp [64];
		tm		tmUtcTimestamp;
		time_t	utcTimestamp = chrono::system_clock::to_time_t(
			chrono::system_clock::now());

		localtime_r (&utcTimestamp, &tmUtcTimestamp);
		sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
			tmUtcTimestamp.tm_year + 1900,
			tmUtcTimestamp.tm_mon + 1,
			tmUtcTimestamp.tm_mday,
			tmUtcTimestamp.tm_hour,
			tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec);

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(_currentIngestionJobKey)
			+ "_"
			+ sUtcTimestamp
			+ ".generateFrame.log";
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
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

        _logger->info(__FILEREF__ + "generateFramesToIngest: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
        );

		bool redirectionStdOutput = true;
		bool redirectionStdError = true;

		ProcessUtility::forkAndExec (
			_ffmpegPath + "/ffmpeg",
			ffmpegArgumentList,
			_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
			pChildPid, &iReturnedStatus);
		if (iReturnedStatus != 0)
        {
			string errorMessage = __FILEREF__ + "generateFrameToIngest: ffmpeg command failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            ;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "generateFrameToIngest: command failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
            ;
            throw runtime_error(errorMessage);
        }

        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();
        
        _logger->info(__FILEREF__ + "generateFrameToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(
				chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		else
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else
			throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
}

void FFMpeg::generateFramesToIngest(
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string imagesDirectory,
        string imageBaseFileName,
        double startTimeInSeconds,
        int framesNumber,
        string videoFilter,
        int periodInSeconds,
        bool mjpeg,
        int imageWidth,
        int imageHeight,
        string mmsAssetPathName,
        int64_t videoDurationInMilliSeconds,
		pid_t* pChildPid)
{
	_currentApiName = "generateFramesToIngest";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		videoDurationInMilliSeconds,
		mmsAssetPathName
		// stagingEncodedAssetPathName
	);

    _logger->info(__FILEREF__ + "generateFramesToIngest"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", encodingJobKey: " + to_string(encodingJobKey)
        + ", imagesDirectory: " + imagesDirectory
        + ", imageBaseFileName: " + imageBaseFileName
        + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
        + ", framesNumber: " + to_string(framesNumber)
        + ", videoFilter: " + videoFilter
        + ", periodInSeconds: " + to_string(periodInSeconds)
        + ", mjpeg: " + to_string(mjpeg)
        + ", imageWidth: " + to_string(imageWidth)
        + ", imageHeight: " + to_string(imageHeight)
        + ", mmsAssetPathName: " + mmsAssetPathName
        + ", videoDurationInMilliSeconds: " + to_string(videoDurationInMilliSeconds)
    );

	if (!FileIO::fileExisting(mmsAssetPathName)        
		&& !FileIO::directoryExisting(mmsAssetPathName)
	)
	{
		string errorMessage = string("Asset path name not existing")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", mmsAssetPathName: " + mmsAssetPathName
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	if (FileIO::directoryExisting(imagesDirectory))
	{
		_logger->info(__FILEREF__ + "Remove"
			+ ", imagesDirectory: " + imagesDirectory);
		Boolean_t bRemoveRecursively = true;
		FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
	}
	{
		_logger->info(__FILEREF__ + "Create directory"
               + ", imagesDirectory: " + imagesDirectory
           );
		bool noErrorIfExists = true;
		bool recursive = true;
		FileIO::createDirectory(imagesDirectory,
			S_IRUSR | S_IWUSR | S_IXUSR |
			S_IRGRP | S_IXGRP |
			S_IROTH | S_IXOTH, noErrorIfExists, recursive);
	}

	int iReturnedStatus = 0;

	{
		char	sUtcTimestamp [64];
		tm		tmUtcTimestamp;
		time_t	utcTimestamp = chrono::system_clock::to_time_t(
			chrono::system_clock::now());

		localtime_r (&utcTimestamp, &tmUtcTimestamp);
		sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
			tmUtcTimestamp.tm_year + 1900,
			tmUtcTimestamp.tm_mon + 1,
			tmUtcTimestamp.tm_mday,
			tmUtcTimestamp.tm_hour,
			tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec);

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(_currentIngestionJobKey)
			+ "_"
			+ to_string(_currentEncodingJobKey)
			+ "_"
			+ sUtcTimestamp
			+ ".generateFrames.log";
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

    string videoFilterParameters;
    if (videoFilter == "PeriodicFrame")
    {
        videoFilterParameters = "-vf fps=1/" + to_string(periodInSeconds) + " ";
    }
    else if (videoFilter == "All-I-Frames")
    {
        if (mjpeg)
            videoFilterParameters = "-vf select='eq(pict_type,PICT_TYPE_I)' ";
        else
            videoFilterParameters = "-vf select='eq(pict_type,PICT_TYPE_I)' -fps_mode vfr ";
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
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

        _logger->info(__FILEREF__ + "generateFramesToIngest: Executing ffmpeg command"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
        );

		bool redirectionStdOutput = true;
		bool redirectionStdError = true;

		ProcessUtility::forkAndExec (
			_ffmpegPath + "/ffmpeg",
			ffmpegArgumentList,
			_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
			pChildPid, &iReturnedStatus);
		if (iReturnedStatus != 0)
        {
			string errorMessage = __FILEREF__ + "generateFramesToIngest: ffmpeg command failed"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            ;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "generateFramesToIngest: command failed"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
            ;
            throw runtime_error(errorMessage);
        }

        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();
        
        _logger->info(__FILEREF__ + "generateFramesToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		else
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
        _logger->error(errorMessage);

		if (FileIO::directoryExisting(imagesDirectory))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", imagesDirectory: " + imagesDirectory);
			Boolean_t bRemoveRecursively = true;
			FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
		}

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else
			throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
}

void FFMpeg::concat(int64_t ingestionJobKey,
		bool isVideo,
        vector<string>& sourcePhysicalPaths,
        string concatenatedMediaPathName)
{
	_currentApiName = "concat";

	setStatus(
		ingestionJobKey
		/*
		encodingJobKey
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

    string concatenationListPathName =
		_ffmpegTempDir + "/"
		+ to_string(ingestionJobKey)
		+ ".concatList.txt"
	;
        
    ofstream concatListFile(concatenationListPathName.c_str(), ofstream::trunc);
    for (string sourcePhysicalPath: sourcePhysicalPaths)
    {
		_logger->info(__FILEREF__ + "ffmpeg: adding physical path"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
		);

		if (!FileIO::fileExisting(sourcePhysicalPath)        
			&& !FileIO::directoryExisting(sourcePhysicalPath)
		)
		{
			string errorMessage = string("Source asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				// + ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", sourcePhysicalPath: " + sourcePhysicalPath
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		concatListFile << "file '" << sourcePhysicalPath << "'" << endl;
    }
    concatListFile.close();

	{
		char	sUtcTimestamp [64];
		tm		tmUtcTimestamp;
		time_t	utcTimestamp = chrono::system_clock::to_time_t(
			chrono::system_clock::now());

		localtime_r (&utcTimestamp, &tmUtcTimestamp);
		sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
			tmUtcTimestamp.tm_year + 1900,
			tmUtcTimestamp.tm_mon + 1,
			tmUtcTimestamp.tm_mday,
			tmUtcTimestamp.tm_hour,
			tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec);

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "_"
			+ sUtcTimestamp
			+ ".concat.log";
	}

    // Then you can stream copy or re-encode your files
    // The -safe 0 above is not required if the paths are relative
    // ffmpeg -f concat -safe 0 -i mylist.txt -c copy output
	// 2019-10-10: added -fflags +genpts -async 1 for lipsync issue!!!
	// 2019-10-11: removed -fflags +genpts -async 1 because does not have inpact on lipsync issue!!!
    string ffmpegExecuteCommand;
	if (isVideo)
	{
		ffmpegExecuteCommand =
			_ffmpegPath + "/ffmpeg "
			+ "-f concat -safe 0 -i " + concatenationListPathName + " "
		;
		bool allVideoAudioTracks = true;
		if (allVideoAudioTracks)
			ffmpegExecuteCommand += "-map 0:v -c:v copy -map 0:a -c:a copy ";
		else
			ffmpegExecuteCommand += "-c copy ";
		ffmpegExecuteCommand +=
			(concatenatedMediaPathName + " "
			+ "> " + _outputFfmpegPathFileName + " "
			+ "2>&1")
		;
	}
	else
	{
		ffmpegExecuteCommand =
			_ffmpegPath + "/ffmpeg "
			+ "-f concat -safe 0 -i " + concatenationListPathName + " "
		;
		bool allVideoAudioTracks = true;
		if (allVideoAudioTracks)
			ffmpegExecuteCommand += "-map 0:a -c:a copy ";
		else
			ffmpegExecuteCommand += "-c copy ";
		ffmpegExecuteCommand +=
			(concatenatedMediaPathName + " "
			+ "> " + _outputFfmpegPathFileName + " "
			+ "2>&1")
		;
	}

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    try
    {
        _logger->info(__FILEREF__ + "concat: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
			// 2020-07-20: added the log of the input file
			string inputBuffer;
			{
				ifstream inputFile(concatenationListPathName);
				stringstream input;
				input << inputFile.rdbuf();

				inputBuffer = input.str();
			}
            string errorMessage = __FILEREF__ + "concat: ffmpeg command failed"
				+ ", executeCommandStatus: " + to_string(executeCommandStatus)
				+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
				+ ", inputBuffer: " + inputBuffer
            ;

            _logger->error(errorMessage);

			// to hide the ffmpeg staff
            errorMessage = __FILEREF__ + "concat: command failed"
				+ ", inputBuffer: " + inputBuffer
            ;
            throw runtime_error(errorMessage);
        }

        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "concat: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		// 2020-07-20: log of ffmpegExecuteCommand commented because already added into the catched exception
        string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                // + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        bool exceptionInCaseOfError = false;
        _logger->info(__FILEREF__ + "Remove"
            + ", concatenationListPathName: " + concatenationListPathName);
        FileIO::remove(concatenationListPathName, exceptionInCaseOfError);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

		// to hide the ffmpeg staff
        errorMessage = __FILEREF__ + "command failed"
			+ ", e.what(): " + e.what()
        ;
		throw runtime_error(errorMessage);
    }

    bool exceptionInCaseOfError = false;
    _logger->info(__FILEREF__ + "Remove"
        + ", concatenationListPathName: " + concatenationListPathName);
    FileIO::remove(concatenationListPathName, exceptionInCaseOfError);
    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

// audio and video 
void FFMpeg::cutWithoutEncoding(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
		string cutType,	// KeyFrameSeeking (input seeking) or FrameAccurateWithoutEncoding
		bool isVideo,
        double startTimeInSeconds,
        double endTimeInSeconds,
        int framesNumber,
        string cutMediaPathName)
{

	_currentApiName = "cut";

	setStatus(
		ingestionJobKey
		/*
		encodingJobKey
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	_logger->info(__FILEREF__ + "Received cutWithoutEncoding"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", sourcePhysicalPath: " + sourcePhysicalPath
		+ ", cutType: " + cutType
		+ ", isVideo: " + to_string(isVideo)
		+ ", startTimeInSeconds: " + to_string(startTimeInSeconds)
		+ ", endTimeInSeconds: " + to_string(endTimeInSeconds)
		+ ", framesNumber: " + to_string(framesNumber)
		+ ", cutMediaPathName: " + cutMediaPathName
		);

	if (!FileIO::fileExisting(sourcePhysicalPath)        
		&& !FileIO::directoryExisting(sourcePhysicalPath)
	)
	{
		string errorMessage = string("Source asset path name not existing")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			// + ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	{
		char	sUtcTimestamp [64];
		tm		tmUtcTimestamp;
		time_t	utcTimestamp = chrono::system_clock::to_time_t(
			chrono::system_clock::now());

		localtime_r (&utcTimestamp, &tmUtcTimestamp);
		sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
			tmUtcTimestamp.tm_year + 1900,
			tmUtcTimestamp.tm_mon + 1,
			tmUtcTimestamp.tm_mday,
			tmUtcTimestamp.tm_hour,
			tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec);

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "_"
			+ sUtcTimestamp
			+ ".cut.log";
	}


    /*
        -ss: When used as an output option (before an output url), decodes but discards input 
            until the timestamps reach position.
            Format: HH:MM:SS.xxx (xxx are decimals of seconds) or in seconds (sec.decimals)
		2019-09-24: Added -async 1 option because the Escenic transcoder (ffmpeg) was failing
			The generated error was: Too many packets buffered for output stream
			(look https://trac.ffmpeg.org/ticket/6375)
			-async samples_per_second
				Audio sync method. "Stretches/squeezes" the audio stream to match the timestamps, the parameter is
				the maximum samples per second by which the audio is changed.  -async 1 is a special case where only
				the start of the audio stream is corrected without any later correction.
			-af "aresample=async=1:min_hard_comp=0.100000:first_pts=0" helps to keep your audio lined up
				with the beginning of your video. It is common for a container to have the beginning
				of the video and the beginning of the audio start at different points. By using this your container
				should have little to no audio drift or offset as it will pad the audio with silence or trim audio
				with negative PTS timestamps if the audio does not actually start at the beginning of the video.
		2019-09-26: introduced the concept of 'Key-Frame Seeking' vs 'All-Frame Seeking' vs 'Full Re-Encoding'
			(see http://www.markbuckler.com/post/cutting-ffmpeg/)
    */
    string ffmpegExecuteCommand;
	if (isVideo)
	{
		// Putting the -ss parameter before the -i parameter is called input seeking and
		// is very fast because FFmpeg jumps from I-frame to I-frame to reach the seek-point.
		// Since the seeking operation jumps between I-frames, it is not going to accurately stop
		// on the frame (or time) that you requested. It will search for the nearest I-frame
		// and start the copy operation from that point.
		//
		// If we insert the -ss parameter after the -i parameter, it is called output seeking.
		// Here is a problem. In video compression, you have I-frames that are indepently encoded
		// and you have predicted frames (P, B) that depend on other frames for decoding.
		// If the start time that you specified falls on a Predicted Frame, the copy operation
		// will start with that frame (call it X). It is possible that the frames that “X”
		// requires in order to be decoded are missing in the output! Consequently,
		// it is possible that the output video will not start smoothly and
		// might have some stutter, or black video until the first I-frame is reached.
		// We are not using this option.

		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg ";
		if (cutType == "KeyFrameSeeking")	// input seeking
            ffmpegExecuteCommand += (string("-ss ") + to_string(startTimeInSeconds) + " "
				+ "-i " + sourcePhysicalPath + " ")
			;
		else // if (cutType == "FrameAccurateWithoutEncoding") output seeking
			ffmpegExecuteCommand += (string("-i ") + sourcePhysicalPath + " "
				+ "-ss " + to_string(startTimeInSeconds) + " ")
			;

		if (framesNumber != -1)
			ffmpegExecuteCommand += (string("-vframes ") + to_string(framesNumber) + " ");
		else
			ffmpegExecuteCommand += (string("-to ") + to_string(endTimeInSeconds) + " ");

		ffmpegExecuteCommand +=	(string("-async 1 ")
				// commented because aresample filtering requires encoding and here we are just streamcopy
			// + "-af \"aresample=async=1:min_hard_comp=0.100000:first_pts=0\" "
			// -map 0:v and -map 0:a is to get all video-audio tracks
			+ "-map 0:v -c:v copy -map 0:a -c:a copy " + cutMediaPathName + " "
			+ "> " + _outputFfmpegPathFileName + " "
			+ "2>&1")
		;
	}
	else
	{
		// audio

		ffmpegExecuteCommand = 
			_ffmpegPath + "/ffmpeg "
			+ "-ss " + to_string(startTimeInSeconds) + " "
			+ "-i " + sourcePhysicalPath + " "
			+ "-to " + to_string(endTimeInSeconds) + " "
			+ "-async 1 "
			// commented because aresample filtering requires encoding and here we are just streamcopy
			// + "-af \"aresample=async=1:min_hard_comp=0.100000:first_pts=0\" "
			// -map 0:v and -map 0:a is to get all video-audio tracks
			+ "-map 0:a -c:a copy " + cutMediaPathName + " "
			+ "> " + _outputFfmpegPathFileName + " "
			+ "2>&1"
		;
	}

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    try
    {
        _logger->info(__FILEREF__ + "cut: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "cut: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
            errorMessage = __FILEREF__ + "cut: command failed"
            ;
            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "cut: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

// only video
void FFMpeg::cutFrameAccurateWithEncoding(
	int64_t ingestionJobKey,
	string sourceVideoAssetPathName,
	// no keyFrameSeeking needs reencoding otherwise the key frame is always used
	// If you re-encode your video when you cut/trim, then you get a frame-accurate cut
	// because FFmpeg will re-encode the video and start with an I-frame.
	int64_t encodingJobKey,
	Json::Value encodingProfileDetailsRoot,
	double startTimeInSeconds,
	double endTimeInSeconds,
	int framesNumber,
	string stagingEncodedAssetPathName,
	pid_t* pChildPid)
{

	_currentApiName = "cut";

	setStatus(
		ingestionJobKey,
		encodingJobKey,
		framesNumber == -1 ? ((endTimeInSeconds - startTimeInSeconds) * 1000) : -1,
		sourceVideoAssetPathName,
		stagingEncodedAssetPathName
	);

	_logger->info(__FILEREF__ + "Received cutFrameAccurateWithEncoding"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", sourceVideoAssetPathName: " + sourceVideoAssetPathName
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", startTimeInSeconds: " + to_string(startTimeInSeconds)
		+ ", endTimeInSeconds: " + to_string(endTimeInSeconds)
		+ ", framesNumber: " + to_string(framesNumber)
		+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
		);

	try
	{
		if (!FileIO::fileExisting(sourceVideoAssetPathName)        
			&& !FileIO::directoryExisting(sourceVideoAssetPathName)
		)
		{
			string errorMessage = string("Source asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				// + ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", sourceVideoAssetPathName: " + sourceVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey)
				+ "_"
				+ sUtcTimestamp
				+ ".cut.log";
		}

		/*
			-ss: When used as an output option (before an output url), decodes but discards input 
				until the timestamps reach position.
				Format: HH:MM:SS.xxx (xxx are decimals of seconds) or in seconds (sec.decimals)
			2019-09-24: Added -async 1 option because the Escenic transcoder (ffmpeg) was failing
				The generated error was: Too many packets buffered for output stream
				(look https://trac.ffmpeg.org/ticket/6375)
				-async samples_per_second
					Audio sync method. "Stretches/squeezes" the audio stream to match the timestamps, the parameter is
					the maximum samples per second by which the audio is changed.  -async 1 is a special case where only
					the start of the audio stream is corrected without any later correction.
				-af "aresample=async=1:min_hard_comp=0.100000:first_pts=0" helps to keep your audio lined up
					with the beginning of your video. It is common for a container to have the beginning
					of the video and the beginning of the audio start at different points. By using this your container
					should have little to no audio drift or offset as it will pad the audio with silence or trim audio
					with negative PTS timestamps if the audio does not actually start at the beginning of the video.
			2019-09-26: introduced the concept of 'Key-Frame Seeking' vs 'All-Frame Seeking' vs 'Full Re-Encoding'
				(see http://www.markbuckler.com/post/cutting-ffmpeg/)
		*/

		if (encodingProfileDetailsRoot == Json::nullValue)
		{
			string errorMessage = __FILEREF__ + "encodingProfileDetailsRoot is mandatory"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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

				/*
				if (httpStreamingFileFormat != "")
				{
					// let's make it simple, in case of cut we do not use http streaming output
					string errorMessage = __FILEREF__ + "in case of cut we are not using an httpStreaming encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else */
				if (twoPasses)
				{
					// let's make it simple, in case of cut we do not use twoPasses
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

					string errorMessage = __FILEREF__ + "in case of cut we are not using two passes encoding. Changed it to false"
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
			catch(runtime_error e)
			{
				string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				throw e;
			}
		}

		vector<string> ffmpegArgumentList;
		ostringstream ffmpegArgumentListStream;
		{
			ffmpegArgumentList.push_back("ffmpeg");
			// global options
			ffmpegArgumentList.push_back("-y");
			// input options
			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(sourceVideoAssetPathName);
			ffmpegArgumentList.push_back("-ss");
			ffmpegArgumentList.push_back(to_string(startTimeInSeconds));
			if (framesNumber != -1)
			{
				ffmpegArgumentList.push_back("-vframes");
				ffmpegArgumentList.push_back(to_string(framesNumber));
			}
			else
			{
				ffmpegArgumentList.push_back("-to");
				ffmpegArgumentList.push_back(to_string(endTimeInSeconds));
			}
			// ffmpegArgumentList.push_back("-async");
			// ffmpegArgumentList.push_back(to_string(1));

			for (string parameter: ffmpegEncodingProfileArgumentList)
				FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

			ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

			int iReturnedStatus = 0;

			try
			{
				chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				if (!ffmpegArgumentList.empty())
					copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
						ostream_iterator<string>(ffmpegArgumentListStream, " "));

				_logger->info(__FILEREF__ + "cut with reencoding: Executing ffmpeg command"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				);

				bool redirectionStdOutput = true;
				bool redirectionStdError = true;

				ProcessUtility::forkAndExec (
					_ffmpegPath + "/ffmpeg",
					ffmpegArgumentList,
					_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
					pChildPid, &iReturnedStatus);
				if (iReturnedStatus != 0)
				{
					string errorMessage = __FILEREF__ + "cut with reencoding: ffmpeg command failed"      
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", iReturnedStatus: " + to_string(iReturnedStatus)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "cut with reencoding: command failed"      
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					throw runtime_error(errorMessage);
				}

				chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

				_logger->info(__FILEREF__ + "cut with reencoding: Executed ffmpeg command"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
				);
			}
			catch(runtime_error e)
			{
				string lastPartOfFfmpegOutputFile = getLastPartOfFile(
					_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				string errorMessage;
				if (iReturnedStatus == 9)   // 9 means: SIGKILL
					errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
						+ ", e.what(): " + e.what()
					;
				else
					errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
						+ ", e.what(): " + e.what()
					;
				_logger->error(errorMessage);

				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				bool exceptionInCaseOfError = false;
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

				if (iReturnedStatus == 9)   // 9 means: SIGKILL
					throw FFMpegEncodingKilledByUser();
				else
					throw e;
			}

			_logger->info(__FILEREF__ + "Remove"
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
			bool exceptionInCaseOfError = false;
			FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
		}
	}
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + "ffmpeg: ffmpeg cut with reencoding failed"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", sourceVideoAssetPathName: " + sourceVideoAssetPathName
			+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			+ ", e.what(): " + e.what()
		);

		if (FileIO::fileExisting(stagingEncodedAssetPathName)
			|| FileIO::directoryExisting(stagingEncodedAssetPathName))
		{
			FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

			_logger->info(__FILEREF__ + "Remove"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
			}
			else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				FileIO::remove(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "ffmpeg: ffmpeg cut with reencoding failed"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", sourceVideoAssetPathName: " + sourceVideoAssetPathName
			+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			+ ", e.what(): " + e.what()
		);

		if (FileIO::fileExisting(stagingEncodedAssetPathName)
			|| FileIO::directoryExisting(stagingEncodedAssetPathName))
		{
			FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

			_logger->info(__FILEREF__ + "Remove"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
			}
			else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				FileIO::remove(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
	catch(exception e)
	{
		_logger->error(__FILEREF__ + "ffmpeg: ffmpeg cut with reencoding failed"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", sourceVideoAssetPathName: " + sourceVideoAssetPathName
			+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
		);

		if (FileIO::fileExisting(stagingEncodedAssetPathName)
			|| FileIO::directoryExisting(stagingEncodedAssetPathName))
		{
			FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

			_logger->info(__FILEREF__ + "Remove"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			// file in case of .3gp content OR directory in case of IPhone content
			if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
			}
			else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				FileIO::remove(stagingEncodedAssetPathName);
			}
		}

		throw e;
	}
}

void FFMpeg::slideShow(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	float durationOfEachSlideInSeconds, 
	string frameRateMode,
	Json::Value encodingProfileDetailsRoot,
	vector<string>& imagesSourcePhysicalPaths,
	vector<string>& audiosSourcePhysicalPaths,
	float shortestAudioDurationInSeconds,	// the shortest duration among the audios
	string encodedStagingAssetPathName,
	pid_t* pChildPid)
{
	_currentApiName = "slideShow";

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
		ingestionJobKey,
		encodingJobKey,
		videoDurationInSeconds * 1000
		/*
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	_logger->info(__FILEREF__ + "Received " + _currentApiName
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", frameRateMode: " + frameRateMode
		+ ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
		+ ", durationOfEachSlideInSeconds: " + to_string(durationOfEachSlideInSeconds)
		+ ", shortestAudioDurationInSeconds: " + to_string(shortestAudioDurationInSeconds)
		+ ", videoDurationInSeconds: " + to_string(videoDurationInSeconds)
	);

	int iReturnedStatus = 0;

    string slideshowListImagesPathName =
		_ffmpegTempDir + "/"
		+ to_string(ingestionJobKey)
		+ ".slideshowListImages.txt"
	;

	{
		ofstream slideshowListFile(slideshowListImagesPathName.c_str(), ofstream::trunc);
		string lastSourcePhysicalPath;
		for (int imageIndex = 0; imageIndex < imagesSourcePhysicalPaths.size(); imageIndex++)
		{
			string sourcePhysicalPath = imagesSourcePhysicalPaths[imageIndex];
			double slideDurationInSeconds;

			if (!FileIO::fileExisting(sourcePhysicalPath)        
				&& !FileIO::directoryExisting(sourcePhysicalPath)
			)
			{
				string errorMessage = string("Source asset path name not existing")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sourcePhysicalPath: " + sourcePhysicalPath
				;
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
					slideDurationInSeconds = shortestAudioDurationInSeconds
						- (durationOfEachSlideInSeconds * imageIndex);
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
							slideDurationInSeconds = (durationOfEachSlideInSeconds * (imageIndex + 1))
								- shortestAudioDurationInSeconds;
					}
				}
			}
			else
				slideDurationInSeconds = durationOfEachSlideInSeconds;
        
			slideshowListFile << "file '" << sourcePhysicalPath << "'" << endl;
			_logger->info(__FILEREF__ + "slideShow"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", line: " + ("file '" + sourcePhysicalPath + "'")
			);
			slideshowListFile << "duration " << slideDurationInSeconds << endl;
			_logger->info(__FILEREF__ + "slideShow"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", line: " + ("duration " + to_string(slideDurationInSeconds))
			);

			lastSourcePhysicalPath = sourcePhysicalPath;
		}
		slideshowListFile << "file '" << lastSourcePhysicalPath << "'" << endl;
		_logger->info(__FILEREF__ + "slideShow"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", line: " + ("file '" + lastSourcePhysicalPath + "'")
			);
		slideshowListFile.close();
	}

	string slideshowListAudiosPathName =
		_ffmpegTempDir + "/"
		+ to_string(ingestionJobKey)
		+ ".slideshowListAudios.txt"
	;

	if (audiosSourcePhysicalPaths.size() > 1)
	{
		ofstream slideshowListFile(slideshowListAudiosPathName.c_str(), ofstream::trunc);
		string lastSourcePhysicalPath;
		for (string sourcePhysicalPath: audiosSourcePhysicalPaths)
		{
			if (!FileIO::fileExisting(sourcePhysicalPath)        
				&& !FileIO::directoryExisting(sourcePhysicalPath)
			)
			{
				string errorMessage = string("Source asset path name not existing")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sourcePhysicalPath: " + sourcePhysicalPath
				;
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
	if (encodingProfileDetailsRoot != Json::nullValue)
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

				string errorMessage = __FILEREF__ + "in case of introOutroOverlay it is not possible to have a two passes encoding. Change it to false"
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
		catch(runtime_error e)
		{
			string errorMessage = __FILEREF__ + "ffmpeg: encodingProfileParameter retrieving failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", e.what(): " + e.what()
				+ ", encodingProfileDetailsRoot: " + JSONUtils::toString(encodingProfileDetailsRoot)
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

	{
		char	sUtcTimestamp [64];
		tm		tmUtcTimestamp;
		time_t	utcTimestamp = chrono::system_clock::to_time_t(
			chrono::system_clock::now());

		localtime_r (&utcTimestamp, &tmUtcTimestamp);
		sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
			tmUtcTimestamp.tm_year + 1900,
			tmUtcTimestamp.tm_mon + 1,
			tmUtcTimestamp.tm_mday,
			tmUtcTimestamp.tm_hour,
			tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec);

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "_"
			+ sUtcTimestamp
			+ ".slideshow.log";
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
	if (encodingProfileDetailsRoot != Json::nullValue)
	{
		for (string parameter: ffmpegEncodingProfileArgumentList)
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
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

        _logger->info(__FILEREF__ + "slideShow: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
        );

		bool redirectionStdOutput = true;
		bool redirectionStdError = true;

		ProcessUtility::forkAndExec (
			_ffmpegPath + "/ffmpeg",
			ffmpegArgumentList,
			_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
			pChildPid, &iReturnedStatus);
		if (iReturnedStatus != 0)
        {
			string errorMessage = __FILEREF__ + "slideShow: ffmpeg command failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            ;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "slideShow: command failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
            ;
            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "slideShow: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
			string errorMessage;
			if (iReturnedStatus == 9)	// 9 means: SIGKILL
				errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
					+ ", e.what(): " + e.what()
				;
			else
				errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
					+ ", e.what(): " + e.what()
				;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        if (FileIO::isFileExisting(slideshowListImagesPathName.c_str()))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", slideshowListImagesPathName: " + slideshowListImagesPathName);
			FileIO::remove(slideshowListImagesPathName, exceptionInCaseOfError);
		}
        if (FileIO::isFileExisting(slideshowListAudiosPathName.c_str()))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", slideshowListAudiosPathName: " + slideshowListAudiosPathName);
			FileIO::remove(slideshowListAudiosPathName, exceptionInCaseOfError);
		}

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else
			throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
    
	if (FileIO::isFileExisting(slideshowListImagesPathName.c_str()))
	{
		_logger->info(__FILEREF__ + "Remove"
			+ ", slideshowListImagesPathName: " + slideshowListImagesPathName);
		FileIO::remove(slideshowListImagesPathName, exceptionInCaseOfError);
	}
	if (FileIO::isFileExisting(slideshowListAudiosPathName.c_str()))
	{
		_logger->info(__FILEREF__ + "Remove"
			+ ", slideshowListAudiosPathName: " + slideshowListAudiosPathName);
		FileIO::remove(slideshowListAudiosPathName, exceptionInCaseOfError);
	}
}

void FFMpeg::extractTrackMediaToIngest(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
        vector<pair<string,int>>& tracksToBeExtracted,
        string extractTrackMediaPathName)
{
	_currentApiName = "extractTrackMediaToIngest";

	setStatus(
		ingestionJobKey
		/*
		encodingJobKey
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	if (!FileIO::fileExisting(sourcePhysicalPath)        
		&& !FileIO::directoryExisting(sourcePhysicalPath)
	)
	{
		string errorMessage = string("Source asset path name not existing")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			// + ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	{
		char	sUtcTimestamp [64];
		tm		tmUtcTimestamp;
		time_t	utcTimestamp = chrono::system_clock::to_time_t(
			chrono::system_clock::now());

		localtime_r (&utcTimestamp, &tmUtcTimestamp);
		sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
			tmUtcTimestamp.tm_year + 1900,
			tmUtcTimestamp.tm_mon + 1,
			tmUtcTimestamp.tm_mday,
			tmUtcTimestamp.tm_hour,
			tmUtcTimestamp.tm_min,
			tmUtcTimestamp.tm_sec);

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "_"
			+ sUtcTimestamp
			+ ".extractTrack.log";
	}


    string mapParameters;
    bool videoTrackIsPresent = false;
    bool audioTrackIsPresent = false;
    for (pair<string,int>& trackToBeExtracted: tracksToBeExtracted)
    {
        string trackType;
        int trackNumber;
        
        tie(trackType,trackNumber) = trackToBeExtracted;
        
        mapParameters += (string("-map 0:") + (trackType == "video" ? "v" : "a") + ":" + to_string(trackNumber) + " ");
        
        if (trackType == "video")
            videoTrackIsPresent = true;
        else
            audioTrackIsPresent = true;
    }
    /*
        -map option: http://ffmpeg.org/ffmpeg.html#Advanced-options
        -c:a copy:      codec option for audio streams has been set to copy, so no decoding-filtering-encoding operations will occur
        -an:            disables audio stream selection for the output
    */
    // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
    string globalOptions = "-y ";
    string inputOptions = "";
    string outputOptions =
            mapParameters
            + (videoTrackIsPresent ? (string("-c:v") + " copy ") : "")
            + (audioTrackIsPresent ? (string("-c:a") + " copy ") : "")
            + (videoTrackIsPresent && !audioTrackIsPresent ? "-an " : "")
            + (!videoTrackIsPresent && audioTrackIsPresent ? "-vn " : "")
            ;

    string ffmpegExecuteCommand =
            _ffmpegPath + "/ffmpeg "
            + globalOptions
            + inputOptions
            + "-i " + sourcePhysicalPath + " "
            + outputOptions
            + extractTrackMediaPathName + " "
            + "> " + _outputFfmpegPathFileName 
            + " 2>&1"
    ;

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    try
    {
        _logger->info(__FILEREF__ + "extractTrackMediaToIngest: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "extractTrackMediaToIngest: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
            errorMessage = __FILEREF__ + "extractTrackMediaToIngest: command failed"
            ;
            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "extractTrackMediaToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

void FFMpeg::liveRecorder(
    int64_t ingestionJobKey,
    int64_t encodingJobKey,
	bool externalEncoder,
	string segmentListPathName,
	string recordedFileNamePrefix,

	string otherInputOptions,

	// if streamSourceType is IP_PUSH means the liveURL should be like
	//		rtmp://<local transcoder IP to bind>:<port>
	//		listening for an incoming connection
	// else if streamSourceType is CaptureLive, liveURL is not used
	// else means the liveURL is "any thing" referring a stream
	string streamSourceType,	// IP_PULL, TV, IP_PUSH, CaptureLive
    string liveURL,
	// Used only in case streamSourceType is IP_PUSH, Maximum time to wait for the incoming connection
	int pushListenTimeout,

	// parameters used only in case streamSourceType is CaptureLive
	int captureLive_videoDeviceNumber,
	string captureLive_videoInputFormat,
	int captureLive_frameRate,
	int captureLive_width,
	int captureLive_height,
	int captureLive_audioDeviceNumber,
	int captureLive_channelsNumber,

	string userAgent,
    time_t utcRecordingPeriodStart, 
    time_t utcRecordingPeriodEnd, 

    int segmentDurationInSeconds,
    string outputFileFormat,
	string segmenterType,	// streamSegmenter or hlsSegmenter

	Json::Value outputsRoot,

	Json::Value framesToBeDetectedRoot,

	pid_t* pChildPid,
	chrono::system_clock::time_point* pRecordingStart
)
{
	_currentApiName = "liveRecorder";

	_logger->info(__FILEREF__ + "Received " + _currentApiName
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", segmentListPathName: " + segmentListPathName
		+ ", recordedFileNamePrefix: " + recordedFileNamePrefix

		+ ", otherInputOptions: " + otherInputOptions

		+ ", streamSourceType: " + streamSourceType
		+ ", liveURL: " + liveURL
		+ ", pushListenTimeout: " + to_string(pushListenTimeout)
		+ ", captureLive_videoDeviceNumber: " + to_string(captureLive_videoDeviceNumber)
		+ ", captureLive_videoInputFormat: " + captureLive_videoInputFormat
		+ ", captureLive_frameRate: " + to_string(captureLive_frameRate)
		+ ", captureLive_width: " + to_string(captureLive_width)
		+ ", captureLive_height: " + to_string(captureLive_height)
		+ ", captureLive_audioDeviceNumber: " + to_string(captureLive_audioDeviceNumber)
		+ ", captureLive_channelsNumber: " + to_string(captureLive_channelsNumber)

		+ ", userAgent: " + userAgent
		+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
		+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
		+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
		+ ", outputFileFormat: " + outputFileFormat
		+ ", segmenterType: " + segmenterType
	);

	setStatus(
		ingestionJobKey,
		encodingJobKey
		/*
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	string segmentListPath;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;
	time_t utcNow;

    try
    {
		size_t segmentListPathIndex = segmentListPathName.find_last_of("/");
		if (segmentListPathIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "No segmentListPath find in the segment path name"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                   + ", segmentListPathName: " + segmentListPathName;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		segmentListPath = segmentListPathName.substr(0, segmentListPathIndex);

		// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
		// I saw just once that the directory was not created and the liveencoder remains in the loop
		// where:
		//	1. the encoder returns an error becaise of the missing directory
		//	2. EncoderVideoAudioProxy calls again the encoder
		// So, for this reason, the below check is done
		if (!FileIO::directoryExisting(segmentListPath))
		{
			_logger->warn(__FILEREF__ + "segmentListPath does not exist!!! It will be created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", segmentListPath: " + segmentListPath
					);

			_logger->info(__FILEREF__ + "Create directory"
                + ", segmentListPath: " + segmentListPath
            );
			bool noErrorIfExists = true;
			bool recursive = true;
			FileIO::createDirectory(segmentListPath,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH, noErrorIfExists, recursive);
		}

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		// 2021-03-06: I saw even if ffmpeg starts exactly at utcRecordingPeriodStart, the segments start time
		//	is about utcRecordingPeriodStart + 5 seconds.
		//	So, to be sure we have the recording at utcRecordingPeriodStart, we have to start ffmpeg
		//	at lease 5 seconds ahead
		int secondsAheadToStartFfmpeg = 10;
		time_t utcRecordingPeriodStartFixed = utcRecordingPeriodStart - secondsAheadToStartFfmpeg;
		if (utcNow < utcRecordingPeriodStartFixed)
		{
			// 2019-12-19: since the first chunk is removed, we will start a bit early
			// than utcRecordingPeriodStart (50% less than segmentDurationInSeconds)
			long secondsToStartEarly;
			if (segmenterType == "streamSegmenter")
				secondsToStartEarly = segmentDurationInSeconds * 50 / 100;
			else
				secondsToStartEarly = 0;

			while (utcNow + secondsToStartEarly < utcRecordingPeriodStartFixed)
			{
				time_t sleepTime = utcRecordingPeriodStartFixed - (utcNow + secondsToStartEarly);

				_logger->info(__FILEREF__ + "LiveRecorder timing. "
						+ "Too early to start the LiveRecorder, just sleep "
					+ to_string(sleepTime) + " seconds"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", secondsToStartEarly: " + to_string(secondsToStartEarly)
                    + ", utcRecordingPeriodStartFixed: " + to_string(utcRecordingPeriodStartFixed)
					);

				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}
			}
		}
		else if (utcRecordingPeriodEnd <= utcNow)
        {
			time_t tooLateTime = utcNow - utcRecordingPeriodEnd;

            string errorMessage = __FILEREF__ + "LiveRecorder timing. "
				+ "Too late to start the LiveRecorder"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcRecordingPeriodStartFixed: " + to_string(utcRecordingPeriodStartFixed)
                    + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
                    + ", tooLateTime: " + to_string(tooLateTime)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
		else
		{
			time_t delayTime = utcNow - utcRecordingPeriodStartFixed;

            string errorMessage = __FILEREF__ + "LiveRecorder timing. "
				+ "We are a bit late to start the LiveRecorder, let's start it"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcRecordingPeriodStartFixed: " + to_string(utcRecordingPeriodStartFixed)
                    + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
                    + ", delayTime: " + to_string(delayTime)
            ;
            _logger->warn(errorMessage);
		}

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey)
				+ "_"
				+ to_string(encodingJobKey)
				+ "_"
				+ sUtcTimestamp
				+ ".liveRecorder.log";
		}

    
		string recordedFileNameTemplate = recordedFileNamePrefix
			+ "_%Y-%m-%d_%H-%M-%S_%s." + outputFileFormat;

		time_t streamingDuration = utcRecordingPeriodEnd - utcNow;

		_logger->info(__FILEREF__ + "LiveRecording timing. "
			+ "Streaming duration"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", utcNow: " + to_string(utcNow)
			+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
			+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
			+ ", streamingDuration: " + to_string(streamingDuration)
		);

		int localPushListenTimeout = pushListenTimeout;
		if (streamSourceType == "IP_PUSH" || streamSourceType == "TV")
		{
			if (localPushListenTimeout > 0 && localPushListenTimeout > streamingDuration)
			{
				// 2021-02-02: sceanrio:
				//	streaming duration is 25 seconds
				//	timeout: 3600 seconds
				//	The result is that the process will finish after 3600 seconds, not after 25 seconds
				//	To avoid that, in this scenario, we will set the timeout equals to streamingDuration
				_logger->info(__FILEREF__ + "LiveRecorder timing. "
					+ "Listen timeout in seconds is reduced because max after 'streamingDuration' the process has to finish"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", utcNow: " + to_string(utcNow)
					+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
					+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
					+ ", streamingDuration: " + to_string(streamingDuration)
					+ ", localPushListenTimeout: " + to_string(localPushListenTimeout)
				);

				localPushListenTimeout = streamingDuration;
			}
		}

		// user agent is an HTTP header and can be used only in case of http request
		bool userAgentToBeUsed = false;
		if (streamSourceType == "IP_PULL" && userAgent != "")
		{
			string httpPrefix = "http";	// it includes also https
			if (liveURL.size() >= httpPrefix.size()
				&& liveURL.compare(0, httpPrefix.size(), httpPrefix) == 0)
			{
				userAgentToBeUsed = true;
			}
			else
			{
				_logger->warn(__FILEREF__ + "user agent cannot be used if not http"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", liveURL: " + liveURL
				);
			}
		}

		ffmpegArgumentList.push_back("ffmpeg");
		// FFMpegEncodingParameters::addToArguments("-loglevel repeat+level+trace", ffmpegArgumentList);
		if (userAgentToBeUsed)
		{
			ffmpegArgumentList.push_back("-user_agent");
			ffmpegArgumentList.push_back(userAgent);
		}

		if (otherInputOptions != "")
		{
			FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegArgumentList);
		}

		if (framesToBeDetectedRoot != Json::nullValue && framesToBeDetectedRoot.size() > 0)
		{
			// 2022-05-28; in caso di framedetection, we will "fix" PTS 
			//	otherwise the one logged seems are not correct.
			//	Using +genpts are OK
			ffmpegArgumentList.push_back("-fflags");
			ffmpegArgumentList.push_back("+genpts");
		}

		if (streamSourceType == "IP_PUSH")
		{
			// listen/timeout depend on the protocol (https://ffmpeg.org/ffmpeg-protocols.html)
			if (
				liveURL.find("http://") != string::npos
				|| liveURL.find("rtmp://") != string::npos
			)
			{
				ffmpegArgumentList.push_back("-listen");
				ffmpegArgumentList.push_back("1");
				if (localPushListenTimeout > 0)
				{
					// no timeout means it will listen infinitely
					ffmpegArgumentList.push_back("-timeout");
					ffmpegArgumentList.push_back(to_string(localPushListenTimeout));
				}
			}
			else if (
				liveURL.find("udp://") != string::npos
			)
			{
				if (localPushListenTimeout > 0)
				{
					int64_t listenTimeoutInMicroSeconds = localPushListenTimeout;
					listenTimeoutInMicroSeconds *= 1000000;
					liveURL += "?timeout=" + to_string(listenTimeoutInMicroSeconds);
				}
			}
			else
			{
				_logger->error(__FILEREF__ + "listen/timeout not managed yet for the current protocol"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", liveURL: " + liveURL
				);
			}

			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(liveURL);
		}
		else if (streamSourceType == "IP_PULL" || streamSourceType == "TV")
		{
			ffmpegArgumentList.push_back("-i");
			ffmpegArgumentList.push_back(liveURL);
		}
		else if (streamSourceType == "CaptureLive")
		{
			// video
			{
				// -f v4l2 -framerate 25 -video_size 640x480 -i /dev/video0
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("v4l2");

				ffmpegArgumentList.push_back("-thread_queue_size");
				ffmpegArgumentList.push_back("4096");

				if (captureLive_videoInputFormat != "")
				{
					ffmpegArgumentList.push_back("-input_format");
					ffmpegArgumentList.push_back(captureLive_videoInputFormat);
				}

				if (captureLive_frameRate != -1)
				{
					ffmpegArgumentList.push_back("-framerate");
					ffmpegArgumentList.push_back(to_string(captureLive_frameRate));
				}

				if (captureLive_width != -1 && captureLive_height != -1)
				{
					ffmpegArgumentList.push_back("-video_size");
					ffmpegArgumentList.push_back(
						to_string(captureLive_width) + "x" + to_string(captureLive_height));
				}

				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(string("/dev/video")
					+ to_string(captureLive_videoDeviceNumber));
			}

			// audio
			{
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("alsa");

				ffmpegArgumentList.push_back("-thread_queue_size");
				ffmpegArgumentList.push_back("2048");

				if (captureLive_channelsNumber != -1)
				{
					ffmpegArgumentList.push_back("-ac");
					ffmpegArgumentList.push_back(to_string(captureLive_channelsNumber));
				}

				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(string("hw:") + to_string(captureLive_audioDeviceNumber));
			}
		}

		// to detect a frame we have to:
		// 1. add -r 1 -loop 1 -i <picture path name of the frame to be detected>
		// 2. add: -filter_complex "[0:v][1:v]blend=difference:shortest=1,blackframe=99:32[f]" -map "[f]" -f null -
		if (framesToBeDetectedRoot != Json::nullValue && framesToBeDetectedRoot.size() > 0)
		{
			for(int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size();
				pictureIndex++)
			{
				Json::Value frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

				if (JSONUtils::isMetadataPresent(frameToBeDetectedRoot, "picturePathName"))
				{
					string picturePathName = frameToBeDetectedRoot.get("picturePathName", "").
						asString();

					ffmpegArgumentList.push_back("-r");
					ffmpegArgumentList.push_back("1");

					ffmpegArgumentList.push_back("-loop");
					ffmpegArgumentList.push_back("1");

					ffmpegArgumentList.push_back("-i");
					ffmpegArgumentList.push_back(picturePathName);
				}
			}
		}

		int streamingDurationIndex;
		{
			ffmpegArgumentList.push_back("-t");
			streamingDurationIndex = ffmpegArgumentList.size();
			ffmpegArgumentList.push_back(to_string(streamingDuration));
		}

		// this is to get all video tracks
		ffmpegArgumentList.push_back("-map");
		ffmpegArgumentList.push_back("0:v");

		ffmpegArgumentList.push_back("-c:v");
		ffmpegArgumentList.push_back("copy");

		// this is to get all audio tracks
		ffmpegArgumentList.push_back("-map");
		ffmpegArgumentList.push_back("0:a");

		ffmpegArgumentList.push_back("-c:a");
		ffmpegArgumentList.push_back("copy");

		if (segmenterType == "streamSegmenter")
		{
			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("segment");
			ffmpegArgumentList.push_back("-segment_list");
			ffmpegArgumentList.push_back(segmentListPathName);
			ffmpegArgumentList.push_back("-segment_time");
			ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
			ffmpegArgumentList.push_back("-segment_atclocktime");
			ffmpegArgumentList.push_back("1");
			ffmpegArgumentList.push_back("-strftime");
			ffmpegArgumentList.push_back("1");
			ffmpegArgumentList.push_back(segmentListPath + "/" + recordedFileNameTemplate);
		}
		else // if (segmenterType == "hlsSegmenter")
		{
			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("append_list");
			ffmpegArgumentList.push_back("-hls_time");
			ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
			ffmpegArgumentList.push_back("-hls_list_size");
			ffmpegArgumentList.push_back("10");

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

			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("program_date_time");

			ffmpegArgumentList.push_back("-strftime");
			ffmpegArgumentList.push_back("1");
			ffmpegArgumentList.push_back("-hls_segment_filename");
			ffmpegArgumentList.push_back(segmentListPath + "/" + recordedFileNameTemplate);

			// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
			// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
			// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
			// 2020-07-19: commented, if it is needed just test it
			// ffmpegArgumentList.push_back("-hls_start_number_source");
			// ffmpegArgumentList.push_back("datetime");

			// 2020-07-19: commented, if it is needed just test it
			// ffmpegArgumentList.push_back("-start_number");
			// ffmpegArgumentList.push_back(to_string(10));

			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("hls");
			ffmpegArgumentList.push_back(segmentListPathName);
		}

		for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
		{
			Json::Value outputRoot = outputsRoot[outputIndex];

			string outputType = outputRoot.get("outputType", "").asString();

			Json::Value filtersRoot = Json::nullValue;
			if (JSONUtils::isMetadataPresent(outputRoot, "filters"))
				filtersRoot = outputRoot["filters"];

			Json::Value encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];

			string encodingProfileContentType =
				outputRoot.get("encodingProfileContentType", "Video").asString();
			bool isVideo = encodingProfileContentType == "Video" ? true : false;
			string otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();

			int videoTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, "videoTrackIndexToBeUsed", -1);
			int audioTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, "audioTrackIndexToBeUsed", -1);

			if (outputType == "HLS" || outputType == "DASH")
			{
				// this is to get all video tracks
				ffmpegArgumentList.push_back("-map");
				if (videoTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:v");
				else
					ffmpegArgumentList.push_back(string("0:v:") + to_string(videoTrackIndexToBeUsed));

				// this is to get all audio tracks
				ffmpegArgumentList.push_back("-map");
				if (audioTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:a");
				else
					ffmpegArgumentList.push_back(string("0:a:") + to_string(videoTrackIndexToBeUsed));

				string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").
					asString();
				string manifestFileName = outputRoot.get("manifestFileName", "").asString();
				int playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);
				int localSegmentDurationInSeconds =
					JSONUtils::asInt(outputRoot, "segmentDurationInSeconds", 10);

				// filter to be managed with the others
				string ffmpegVideoResolutionParameter;

				vector<string> ffmpegEncodingProfileArgumentList;
				if (encodingProfileDetailsRoot != Json::nullValue)
				{
					try
					{
						string httpStreamingFileFormat;    
						string ffmpegHttpStreamingParameter = "";

						string ffmpegFileFormatParameter = "";

						string ffmpegVideoCodecParameter = "";
						string ffmpegVideoProfileParameter = "";
						int videoBitRateInKbps = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoOtherParameters = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegVideoFrameRateParameter = "";
						string ffmpegVideoKeyFramesRateParameter = "";
						bool twoPasses;
						vector<tuple<string, int, int, int, string, string, string>>
							videoBitRatesInfo;

						string ffmpegAudioCodecParameter = "";
						string ffmpegAudioBitRateParameter = "";
						string ffmpegAudioOtherParameters = "";
						string ffmpegAudioChannelsParameter = "";
						string ffmpegAudioSampleRateParameter = "";
						vector<string> audioBitRatesInfo;


						FFMpegEncodingParameters::settingFfmpegParameters(
							_logger,
							encodingProfileDetailsRoot,
							isVideo,

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
							ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter)
							= videoBitRateInfo;

						ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

						if (twoPasses)
						{
							twoPasses = false;

							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", twoPasses: " + to_string(twoPasses)
							;
							_logger->warn(errorMessage);
						}

						FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter,
							ffmpegEncodingProfileArgumentList);
						// FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 	ffmpegEncodingProfileArgumentList);
						ffmpegEncodingProfileArgumentList.push_back("-threads");
						ffmpegEncodingProfileArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter,
							ffmpegEncodingProfileArgumentList);
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__
							+ "encodingProfileParameter retrieving failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						throw e;
					}
				}

				tuple<string, string, string> allFilters = addFilters(
					filtersRoot, ffmpegVideoResolutionParameter,
					"", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;

				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
				}
				else
				{
					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					else if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}

					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					else if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}
				}

				{
					string manifestFilePathName = manifestDirectoryPath + "/" + manifestFileName;

					_logger->info(__FILEREF__ + "Checking manifestDirectoryPath directory"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
					);

					// directory is created by EncoderVideoAudioProxy
					//	using MMSStorage::getStagingAssetPathName
					// I saw just once that the directory was not created and
					//	the liveencoder remains in the loop
					// where:
					//	1. the encoder returns an error because of the missing directory
					//	2. EncoderVideoAudioProxy calls again the encoder
					// So, for this reason, the below check is done
					if (!FileIO::directoryExisting(manifestDirectoryPath))
					{
						_logger->info(__FILEREF__ + "Create directory"
							+ ", manifestDirectoryPath: " + manifestDirectoryPath
						);
						bool noErrorIfExists = true;
						bool recursive = true;
						FileIO::createDirectory(manifestDirectoryPath,
							S_IRUSR | S_IWUSR | S_IXUSR |
							S_IRGRP | S_IXGRP |
							S_IROTH | S_IXOTH, noErrorIfExists, recursive);
					}

					if (externalEncoder)
						addToIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

					if (outputType == "HLS")
					{
						ffmpegArgumentList.push_back("-hls_flags");
						ffmpegArgumentList.push_back("append_list");
						ffmpegArgumentList.push_back("-hls_time");
						ffmpegArgumentList.push_back(to_string(localSegmentDurationInSeconds));
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


						// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
						// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
						// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
						// 2020-07-19: commented, if it is needed just test it
						// ffmpegArgumentList.push_back("-hls_start_number_source");
						// ffmpegArgumentList.push_back("datetime");

						// 2020-07-19: commented, if it is needed just test it
						// ffmpegArgumentList.push_back("-start_number");
						// ffmpegArgumentList.push_back(to_string(10));
					}
					else if (outputType == "DASH")
					{
						ffmpegArgumentList.push_back("-seg_duration");
						ffmpegArgumentList.push_back(to_string(localSegmentDurationInSeconds));
						ffmpegArgumentList.push_back("-window_size");
						ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

						// it is important to specify -init_seg_name because those files
						// will not be removed in EncoderVideoAudioProxy.cpp
						ffmpegArgumentList.push_back("-init_seg_name");
						ffmpegArgumentList.push_back("init-stream$RepresentationID$.$ext$");

						// the only difference with the ffmpeg default is that default is $Number%05d$
						// We had to change it to $Number%01d$ because otherwise the generated file containing
						// 00001 00002 ... but the videojs player generates file name like 1 2 ...
						// and the streaming was not working
						ffmpegArgumentList.push_back("-media_seg_name");
						ffmpegArgumentList.push_back("chunk-stream$RepresentationID$-$Number%01d$.$ext$");
					}

					FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegArgumentList);

					ffmpegArgumentList.push_back(manifestFilePathName);
				}
			}
			else if (outputType == "RTMP_Stream" || outputType == "AWS_CHANNEL")
			{
				// 2022-09-01: scenario: mando un m3u8 multi tracce ricevuto da HWM (serie C)
				//	verso un rtmp della CDN77, mi fallisce perchè un flv/rtmp non puo' essere
				//	multi traccia.
				//	Quindi mi assicuro di mandare una sola traccia (la prima)
				ffmpegArgumentList.push_back("-map");
				if (videoTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:v:0");
				else
					ffmpegArgumentList.push_back(string("0:v:") + to_string(videoTrackIndexToBeUsed));
				ffmpegArgumentList.push_back("-map");
				if (audioTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:a:0");
				else
					ffmpegArgumentList.push_back(string("0:a:") + to_string(videoTrackIndexToBeUsed));

				string rtmpUrl = outputRoot.get("rtmpUrl", "").asString();
				string rtmpStreamName = outputRoot.get("rtmpStreamName", "").asString();
				string rtmpUserName = outputRoot.get("rtmpUserName", "").asString();
				string rtmpPassword = outputRoot.get("rtmpPassword", "").asString();
				if (rtmpUrl == "")
				{
					string errorMessage = __FILEREF__ + "rtmpUrl cannot be empty"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", rtmpUrl: " + rtmpUrl
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				if (rtmpStreamName != "")
					rtmpUrl += ("/" + rtmpStreamName);
				if (rtmpUserName != "" && rtmpPassword != "")
				{
					// rtmp://.....
					rtmpUrl.insert(7, (rtmpUserName + ":" + rtmpPassword + "@"));
				}

				// filter to be managed with the others
				string ffmpegVideoResolutionParameter;

				vector<string> ffmpegEncodingProfileArgumentList;
				if (encodingProfileDetailsRoot != Json::nullValue)
				{
					try
					{
						string httpStreamingFileFormat;    
						string ffmpegHttpStreamingParameter = "";

						string ffmpegFileFormatParameter = "";

						string ffmpegVideoCodecParameter = "";
						string ffmpegVideoProfileParameter = "";
						// string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoOtherParameters = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegVideoFrameRateParameter = "";
						string ffmpegVideoKeyFramesRateParameter = "";
						bool twoPasses;
						vector<tuple<string, int, int, int, string, string, string>>
							videoBitRatesInfo;

						string ffmpegAudioCodecParameter = "";
						string ffmpegAudioBitRateParameter = "";
						string ffmpegAudioOtherParameters = "";
						string ffmpegAudioChannelsParameter = "";
						string ffmpegAudioSampleRateParameter = "";
						vector<string> audioBitRatesInfo;

	
						FFMpegEncodingParameters::settingFfmpegParameters(
							_logger,
							encodingProfileDetailsRoot,
							isVideo,

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

						/*
						if (httpStreamingFileFormat != "")
						{
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have an httpStreaming encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
						else */ if (twoPasses)
						{
							/*
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
							*/
							twoPasses = false;

							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false"
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
						// FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 	ffmpegEncodingProfileArgumentList);
						ffmpegEncodingProfileArgumentList.push_back("-threads");
						ffmpegEncodingProfileArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						throw e;
					}
				}

				tuple<string, string, string> allFilters = addFilters(
					filtersRoot, ffmpegVideoResolutionParameter,
					"", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;


				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
				}
				else
				{
					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					else if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}

					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					else if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}
				}

				FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegArgumentList);

				ffmpegArgumentList.push_back("-bsf:a");
				ffmpegArgumentList.push_back("aac_adtstoasc");
				// 2020-08-13: commented bacause -c:v copy is already present
				// ffmpegArgumentList.push_back("-vcodec");
				// ffmpegArgumentList.push_back("copy");

				// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("flv");
				ffmpegArgumentList.push_back(rtmpUrl);
			}
			else if (outputType == "UDP_Stream")
			{
				// this is to get all video tracks
				ffmpegArgumentList.push_back("-map");
				if (videoTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:v");
				else
					ffmpegArgumentList.push_back(string("0:v:") + to_string(videoTrackIndexToBeUsed));

				// this is to get all audio tracks
				ffmpegArgumentList.push_back("-map");
				if (audioTrackIndexToBeUsed == -1)
					ffmpegArgumentList.push_back("0:a");
				else
					ffmpegArgumentList.push_back(string("0:a:") + to_string(videoTrackIndexToBeUsed));

				string udpUrl = outputRoot.get("udpUrl", "").asString();

				if (udpUrl == "")
				{
					string errorMessage = __FILEREF__ + "udpUrl cannot be empty"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", udpUrl: " + udpUrl
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// filter to be managed with the others
				string ffmpegVideoResolutionParameter;

				vector<string> ffmpegEncodingProfileArgumentList;
				if (encodingProfileDetailsRoot != Json::nullValue)
				{
					try
					{
						string httpStreamingFileFormat;    
						string ffmpegHttpStreamingParameter = "";

						string ffmpegFileFormatParameter = "";

						string ffmpegVideoCodecParameter = "";
						string ffmpegVideoProfileParameter = "";
						// string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoOtherParameters = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegVideoFrameRateParameter = "";
						string ffmpegVideoKeyFramesRateParameter = "";
						bool twoPasses;
						vector<tuple<string, int, int, int, string, string, string>>
							videoBitRatesInfo;

						string ffmpegAudioCodecParameter = "";
						string ffmpegAudioBitRateParameter = "";
						string ffmpegAudioOtherParameters = "";
						string ffmpegAudioChannelsParameter = "";
						string ffmpegAudioSampleRateParameter = "";
						vector<string> audioBitRatesInfo;

	
						FFMpegEncodingParameters::settingFfmpegParameters(
							_logger,
							encodingProfileDetailsRoot,
							isVideo,

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

						/*
						if (httpStreamingFileFormat != "")
						{
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have an httpStreaming encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
						else */ if (twoPasses)
						{
							/*
							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
							*/
							twoPasses = false;

							string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false"
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
						// FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 	ffmpegEncodingProfileArgumentList);
						ffmpegEncodingProfileArgumentList.push_back("-threads");
						ffmpegEncodingProfileArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						throw e;
					}
				}

				tuple<string, string, string> allFilters = addFilters(
					filtersRoot, ffmpegVideoResolutionParameter,
					"", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;


				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						FFMpegEncodingParameters::addToArguments(parameter, ffmpegArgumentList);

					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
				}
				else
				{
					if (videoFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:v");
						ffmpegArgumentList.push_back(videoFilters);
					}
					else if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}

					if (audioFilters != "")
					{
						ffmpegArgumentList.push_back("-filter:a");
						ffmpegArgumentList.push_back(audioFilters);
					}
					else if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}
				}

				FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegArgumentList);

				// ffmpegArgumentList.push_back("-bsf:a");
				// ffmpegArgumentList.push_back("aac_adtstoasc");
				// 2020-08-13: commented bacause -c:v copy is already present
				// ffmpegArgumentList.push_back("-vcodec");
				// ffmpegArgumentList.push_back("copy");

				// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("mpegts");
				ffmpegArgumentList.push_back(udpUrl);
			}
			else if (outputType == "NONE")
			{
				tuple<string, string, string> allFilters = addFilters(
					filtersRoot, "", "", -1);

				string videoFilters;
				string audioFilters;
				string complexFilters;
				tie(videoFilters, audioFilters, complexFilters) = allFilters;


				if (videoFilters != "")
				{
					ffmpegArgumentList.push_back("-filter:v");
					ffmpegArgumentList.push_back(videoFilters);
				}

				if (audioFilters != "")
				{
					ffmpegArgumentList.push_back("-filter:a");
					ffmpegArgumentList.push_back(audioFilters);
				}

				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("null");
				ffmpegArgumentList.push_back("-");
			}
			else
			{
				string errorMessage = __FILEREF__ + "liveRecording. Wrong output type"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", outputType: " + outputType;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// 2. add: -filter_complex "[0:v][1:v]blend=difference:shortest=1,blackframe=99:32[f]" -map "[f]" -f null -
		if (framesToBeDetectedRoot != Json::nullValue && framesToBeDetectedRoot.size() > 0)
		{
			for(int pictureIndex = 0; pictureIndex < framesToBeDetectedRoot.size();
				pictureIndex++)
			{
				Json::Value frameToBeDetectedRoot = framesToBeDetectedRoot[pictureIndex];

				if (JSONUtils::isMetadataPresent(frameToBeDetectedRoot, "picturePathName"))
				{
					bool videoFrameToBeCropped = JSONUtils::asBool(frameToBeDetectedRoot,
						"videoFrameToBeCropped", false);

					ffmpegArgumentList.push_back("-filter_complex");

					int amount = JSONUtils::asInt(frameToBeDetectedRoot, "amount", 99);
					int threshold = JSONUtils::asInt(frameToBeDetectedRoot, "threshold", 32);

					string filter;

					if (videoFrameToBeCropped)
					{
						int width = JSONUtils::asInt(frameToBeDetectedRoot, "width", -1);
						int height = JSONUtils::asInt(frameToBeDetectedRoot, "height", -1);
						int videoCrop_X = JSONUtils::asInt(frameToBeDetectedRoot, "videoCrop_X", -1);
						int videoCrop_Y = JSONUtils::asInt(frameToBeDetectedRoot, "videoCrop_Y", -1);

						filter =
							"[0:v]crop=w=" + to_string(width) + ":h=" + to_string(height)
							+ ":x=" + to_string(videoCrop_X) + ":y=" + to_string(videoCrop_Y)
							+ "[CROPPED];"
							+ "[CROPPED][" + to_string(pictureIndex + 1) + ":v]"
							+ "blend=difference:shortest=1,blackframe=amount=" + to_string(amount) + ":threshold=" + to_string(threshold)
							+ "[differenceOut_" + to_string(pictureIndex + 1) + "]"
						;
					}
					else
					{
						filter =
							"[0:v][" + to_string(pictureIndex + 1) + ":v]"
							+ "blend=difference:shortest=1,blackframe=amount=" + to_string(amount) + ":threshold=" + to_string(threshold)
							+ "[differenceOut_" + to_string(pictureIndex + 1) + "]"
						;
					}
					ffmpegArgumentList.push_back(filter);

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back("[differenceOut_"
						+ to_string(pictureIndex + 1) + "]");

					ffmpegArgumentList.push_back("-f");
					ffmpegArgumentList.push_back("null");

					ffmpegArgumentList.push_back("-");
				}
			}
		}

		bool sigQuitReceived = true;
		while(sigQuitReceived)
		{
			sigQuitReceived = false;

			if (!ffmpegArgumentList.empty())
				copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
					ostream_iterator<string>(ffmpegArgumentListStream, " "));

			_logger->info(__FILEREF__ + "liveRecorder: Executing ffmpeg command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
			);

			bool redirectionStdOutput = true;
			bool redirectionStdError = true;

			startFfmpegCommand = chrono::system_clock::now();

			ProcessUtility::forkAndExec (
				_ffmpegPath + "/ffmpeg",
				ffmpegArgumentList,
				_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
				pChildPid, &iReturnedStatus);

			endFfmpegCommand = chrono::system_clock::now();

			int64_t realDuration = chrono::duration_cast<chrono::seconds>(
				endFfmpegCommand - startFfmpegCommand).count();

			if (iReturnedStatus != 0)
			{
				string lastPartOfFfmpegOutputFile = getLastPartOfFile(
					_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				// Exiting normally, received signal 3.
				if (lastPartOfFfmpegOutputFile.find("signal 3") != string::npos)
				{
					sigQuitReceived = true;

					string errorMessage = __FILEREF__
						+ "liveRecorder: ffmpeg execution command failed because received SIGQUIT and is called again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", iReturnedStatus: " + to_string(iReturnedStatus)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
						+ ", difference between real and expected duration: "
							+ to_string(realDuration - streamingDuration)
					;
					_logger->error(errorMessage);

					// in case of IP_PUSH the monitor thread, in case the client does not
					// reconnect istantaneously, kills the process.
					// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
					// we will set again the pRecordingStart variable
					{
						if (chrono::system_clock::from_time_t(utcRecordingPeriodStart) <
							chrono::system_clock::now())
							*pRecordingStart = chrono::system_clock::now() +
								chrono::seconds(localPushListenTimeout);
						else
							*pRecordingStart = chrono::system_clock::from_time_t(
								utcRecordingPeriodStart) +
								chrono::seconds(localPushListenTimeout);
					}

					{
						chrono::system_clock::time_point now = chrono::system_clock::now();
						utcNow = chrono::system_clock::to_time_t(now);

						if (utcNow < utcRecordingPeriodEnd)
						{
							time_t localStreamingDuration = utcRecordingPeriodEnd - utcNow;
							ffmpegArgumentList[streamingDurationIndex] = to_string(localStreamingDuration);

							_logger->info(__FILEREF__
								+ "liveRecorder: ffmpeg execution command failed because received SIGQUIT, recalculate streaming duration"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", iReturnedStatus: " + to_string(iReturnedStatus)
								+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
								+ ", localStreamingDuration: " + to_string(localStreamingDuration)
							);
						}
						else
						{
							// exit from loop even if SIGQUIT because time period expired
							sigQuitReceived = false;

							_logger->info(__FILEREF__
								+ "liveRecorder: ffmpeg execution command should be called again because received SIGQUIT but utcRecordingPeriod expired"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", iReturnedStatus: " + to_string(iReturnedStatus)
								+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
								+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
							);
						}

						continue;
					}
				}

				string errorMessage = __FILEREF__ + "liveRecorder: ffmpeg: ffmpeg execution command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", iReturnedStatus: " + to_string(iReturnedStatus)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					+ ", difference between real and expected duration: "
						+ to_string(realDuration - streamingDuration)
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "liveRecorder: command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				;
				throw runtime_error(errorMessage);
			}
		}

        _logger->info(__FILEREF__ + "liveRecorder: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );

		for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
		{
			Json::Value outputRoot = outputsRoot[outputIndex];

			string outputType = outputRoot.get("outputType", "").asString();

			if (outputType == "HLS" || outputType == "DASH")
			{
				string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").asString();

				if (externalEncoder)
					removeFromIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

				if (manifestDirectoryPath != "")
				{
					if (FileIO::directoryExisting(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							Boolean_t bRemoveRecursively = true;
							FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
    	}

		// if (segmenterType == "streamSegmenter" || segmenterType == "hlsSegmenter")
		{
			if (segmentListPath != "" && FileIO::directoryExisting(segmentListPath))
			{
				try
				{
					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", segmentListPath: " + segmentListPath
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(segmentListPath, bRemoveRecursively);
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", segmentListPath: " + segmentListPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
				catch(exception e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", segmentListPath: " + segmentListPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
			}
		}

		/*
		if (endFfmpegCommand - startFfmpegCommand < chrono::seconds(utcRecordingPeriodEnd - utcNow - 60))
		{
			char		sEndFfmpegCommand [64];

			time_t	utcEndFfmpegCommand = chrono::system_clock::to_time_t(endFfmpegCommand);
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
				+ ".liveRecorder.log.debug"
				;

			_logger->info(__FILEREF__ + "Coping"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
				);
			FileIO::copyFile(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);    

			throw runtime_error("liveRecording exit before unexpectly");
		}
		*/
	}
    catch(runtime_error e)
    {
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(
			_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg: ffmpeg execution command failed because killed by the user"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		else
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg execution command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		_logger->error(errorMessage);

		// copy ffmpeg log file
		/*
		{
			char		sEndFfmpegCommand [64];

			time_t	utcEndFfmpegCommand = chrono::system_clock::to_time_t(
				chrono::system_clock::now());
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
				+ ".liveRecorder.log.debug"
			;

			_logger->info(__FILEREF__ + "Coping"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
				);
			FileIO::copyFile(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);    
		}
		*/

		bool exceptionInCaseOfError = false;
		/*
		_logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
		*/

		_logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", segmentListPathName: " + segmentListPathName);
		FileIO::remove(segmentListPathName, exceptionInCaseOfError);

		for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
		{
			Json::Value outputRoot = outputsRoot[outputIndex];

			string outputType = outputRoot.get("outputType", "").asString();

			if (outputType == "HLS" || outputType == "DASH")
			{
				string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").asString();

				if (externalEncoder)
					removeFromIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

				if (manifestDirectoryPath != "")
				{
					if (FileIO::directoryExisting(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							Boolean_t bRemoveRecursively = true;
							FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
    	}

		// if (segmenterType == "streamSegmenter" || segmenterType == "hlsSegmenter")
		{
			if (segmentListPath != "" && FileIO::directoryExisting(segmentListPath))
			{
				try
				{
					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", segmentListPath: " + segmentListPath
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(segmentListPath, bRemoveRecursively);
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", segmentListPath: " + segmentListPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
				catch(exception e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", segmentListPath: " + segmentListPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
			}
		}

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
			throw FFMpegURLForbidden();
		else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
			throw FFMpegURLNotFound();
		else
			throw e;
    }

	/*
    _logger->info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
	*/
}

void FFMpeg::liveProxy2(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	bool externalEncoder,

	mutex* inputsRootMutex,
	Json::Value* inputsRoot,

	Json::Value outputsRoot,

	pid_t* pChildPid,
	chrono::system_clock::time_point* pProxyStart
)
{
	_currentApiName = "liveProxy";

	_logger->info(__FILEREF__ + "Received " + _currentApiName
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", inputsRoot->size: " + to_string(inputsRoot->size())
	);

	setStatus(
		ingestionJobKey,
		encodingJobKey
		/*
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	// Creating multi outputs: https://trac.ffmpeg.org/wiki/Creating%20multiple%20outputs
	if (inputsRoot->size() == 0)
	{
		string errorMessage = __FILEREF__ + "liveProxy. No input parameters"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", inputsRoot->size: " + to_string(inputsRoot->size())
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	if (outputsRoot.size() == 0)
	{
		string errorMessage = __FILEREF__ + "liveProxy. No output parameters"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", outputsRoot.size: " + to_string(outputsRoot.size())
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	// _logger->info(__FILEREF__ + "Calculating timedInput"
	// 	+ ", ingestionJobKey: " + to_string(ingestionJobKey)
	// 	+ ", encodingJobKey: " + to_string(encodingJobKey)
	// 	+ ", inputsRoot->size: " + to_string(inputsRoot->size())
	// );
	bool timedInput = true;
	{
		lock_guard<mutex> locker(*inputsRootMutex);

		for (int inputIndex = 0; inputIndex < inputsRoot->size(); inputIndex++)
		{
			Json::Value inputRoot = (*inputsRoot)[inputIndex];

			bool timePeriod = false;
			string field = "timePeriod";
			if (JSONUtils::isMetadataPresent(inputRoot, field))
				timePeriod = JSONUtils::asBool(inputRoot, field, false);

			int64_t utcProxyPeriodStart = -1;
			field = "utcScheduleStart";
			if (JSONUtils::isMetadataPresent(inputRoot, field))
				utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// else
			// {
			// 	field = "utcProxyPeriodStart";
			// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
			// 		utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			int64_t utcProxyPeriodEnd = -1;
			field = "utcScheduleEnd";
			if (JSONUtils::isMetadataPresent(inputRoot, field))
				utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// else
			// {
			// 	field = "utcProxyPeriodEnd";
			// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
			// 		utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			if (!timePeriod || utcProxyPeriodStart == -1 || utcProxyPeriodEnd == -1)
			{
				timedInput = false;

				break;
			}
		}
	}
	_logger->info(__FILEREF__ + "Calculated timedInput"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", inputsRoot->size: " + to_string(inputsRoot->size())
		+ ", timedInput: " + to_string(timedInput)
	);

	if (timedInput)
	{
		int64_t utcFirstProxyPeriodStart = -1;
		int64_t utcLastProxyPeriodEnd = -1;
		{
			lock_guard<mutex> locker(*inputsRootMutex);

			for (int inputIndex = 0; inputIndex < inputsRoot->size(); inputIndex++)
			{
				Json::Value inputRoot = (*inputsRoot)[inputIndex];

				string field = "utcScheduleStart";
				int64_t utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
				// if (utcProxyPeriodStart == -1)
				// {
				// 	field = "utcProxyPeriodStart";
				// 	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
				// }
				if (utcFirstProxyPeriodStart == -1)
					utcFirstProxyPeriodStart = utcProxyPeriodStart;

				field = "utcScheduleEnd";
				utcLastProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
				// if (utcLastProxyPeriodEnd == -1)
				// {
				// 	field = "utcProxyPeriodEnd";
				// 	utcLastProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
				// }
			}
		}

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		if (utcNow < utcFirstProxyPeriodStart)
		{
			while (utcNow < utcFirstProxyPeriodStart)
			{
				time_t sleepTime = utcFirstProxyPeriodStart - utcNow;

				_logger->info(__FILEREF__ + "LiveProxy timing. "
					+ "Too early to start the LiveProxy, just sleep "
						+ to_string(sleepTime) + " seconds"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcFirstProxyPeriodStart: " + to_string(utcFirstProxyPeriodStart)
				);

				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}
			}
		}
		else if (utcLastProxyPeriodEnd < utcNow)
        {
			time_t tooLateTime = utcNow - utcLastProxyPeriodEnd;

            string errorMessage = __FILEREF__ + "LiveProxy timing. "
				+ "Too late to start the LiveProxy"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcLastProxyPeriodEnd: " + to_string(utcLastProxyPeriodEnd)
                    + ", tooLateTime: " + to_string(tooLateTime)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
	}

	// max repeating is 1 because:
	//	- we have to return to the engine because the engine has to register the failure
	//	- if we increase 'max repeating':
	//		- transcoder does not return to engine even in case of failure (max repeating is > 1)
	//		- engine calls getEncodingStatus and get a 'success' (transcoding is repeating and
	//			the failure is not raised to the engine). So failures number engine variable is set to 0
	//		- transcoder, after repeating, raise the failure to engine but the engine,
	//			as mentioned before, already reset failures number to 0
	//	The result is that engine never reach max number of failures and encoding request,
	//	even if it is failing, never exit from the engine loop (EncoderVideoAudioProxy.cpp)

	//	In case ffmpeg fails after at least XXX minutes, this is not considered a failure
	//	and it will be executed again. This is very important because it makes sure ffmpeg
	//	is not failing continuously without working at all.
	//	Here follows some scenarios where it is important to execute again ffmpeg and not returning to
	//	EncoderVideoAudioProxy:
	// case 1:
	// 2022-10-20. scenario (ffmpeg is a server):
	//	- (1) streamSourceType is IP_PUSH
	//	- (2) the client just disconnected because of a client issue
	//	- (3) ffmpeg exit too early
	// In this case ffmpeg has to return to listen as soon as possible for a new connection.
	// In case we return an exception it will pass about 10-15 seconds before ffmpeg returns
	// to be executed and listen again for a new connection.
	// To make ffmpeg to listen as soon as possible, we will not return an exception
	// da almeno XXX secondi (4)
	// case 2:
	// 2022-10-26. scenario (ffmpeg is a client):
	//	Nel caso in cui devono essere "ripetuti" multiple inputs, vedi commento 2022-10-27.
	//  In questo scenario quando ffmpeg termina la prima ripetizione, deve essere eseguito
	//	nuovamente per la successiva ripetizione.
	int maxTimesRepeatingSameInput = 1;
	int currentNumberOfRepeatingSameInput = 0;
	int sleepInSecondsInCaseOfRepeating = 5;
	int currentInputIndex = -1;
	Json::Value currentInputRoot;
	while ((currentInputIndex = getNextLiveProxyInput(ingestionJobKey, encodingJobKey,
		inputsRoot,
		inputsRootMutex, currentInputIndex, timedInput, &currentInputRoot)) != -1)
	{
		vector<string> ffmpegInputArgumentList;
		long streamingDurationInSeconds = -1;
		string otherOutputOptionsBecauseOfMaxWidth;
		string endlessPlaylistListPathName;
		int pushListenTimeout;
		int64_t utcProxyPeriodStart;
		try
		{
			_logger->info(__FILEREF__ + "liveProxyInput..."
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", inputsRoot->size: " + to_string(inputsRoot->size())
				+ ", timedInput: " + to_string(timedInput)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
			);
			tuple<long, string, string, int, int64_t> inputDetails = liveProxyInput(
				ingestionJobKey, encodingJobKey, externalEncoder,
				currentInputRoot, ffmpegInputArgumentList);
			tie(streamingDurationInSeconds, otherOutputOptionsBecauseOfMaxWidth,
				endlessPlaylistListPathName, pushListenTimeout, utcProxyPeriodStart) = inputDetails;

			{
				ostringstream ffmpegInputArgumentListStream;
				if (!ffmpegInputArgumentList.empty())
					copy(ffmpegInputArgumentList.begin(), ffmpegInputArgumentList.end(),
						ostream_iterator<string>(ffmpegInputArgumentListStream, " "));
				_logger->info(__FILEREF__ + "liveProxy: ffmpegInputArgumentList"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ffmpegInputArgumentList: " + ffmpegInputArgumentListStream.str()
				);
			}
		}
		catch(runtime_error e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong input parameters"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", exception: " + e.what()
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong input parameters"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", exception: " + e.what()
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegOutputArgumentList;
		try
		{
			_logger->info(__FILEREF__ + "liveProxyOutput..."
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", outputsRoot.size: " + to_string(outputsRoot.size())
			);
			liveProxyOutput(ingestionJobKey, encodingJobKey, externalEncoder,
				otherOutputOptionsBecauseOfMaxWidth,
				currentInputRoot,
				streamingDurationInSeconds,
				outputsRoot, ffmpegOutputArgumentList);

			{
				ostringstream ffmpegOutputArgumentListStream;
				if (!ffmpegOutputArgumentList.empty())
					copy(ffmpegOutputArgumentList.begin(), ffmpegOutputArgumentList.end(),
						ostream_iterator<string>(ffmpegOutputArgumentListStream, " "));
				_logger->info(__FILEREF__ + "liveProxy: ffmpegOutputArgumentList"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ffmpegOutputArgumentList: " + ffmpegOutputArgumentListStream.str()
				);
			}
		}
		catch(runtime_error e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong output parameters"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", exception: " + e.what()
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong output parameters"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", exception: " + e.what()
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		ostringstream ffmpegArgumentListStream;
		int iReturnedStatus = 0;
		chrono::system_clock::time_point startFfmpegCommand;
		chrono::system_clock::time_point endFfmpegCommand;

		try
		{
			{
				char	sUtcTimestamp [64];
				tm		tmUtcTimestamp;
				time_t	utcTimestamp = chrono::system_clock::to_time_t(
					chrono::system_clock::now());

				localtime_r (&utcTimestamp, &tmUtcTimestamp);
				sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
					tmUtcTimestamp.tm_year + 1900,
					tmUtcTimestamp.tm_mon + 1,
					tmUtcTimestamp.tm_mday,
					tmUtcTimestamp.tm_hour,
					tmUtcTimestamp.tm_min,
					tmUtcTimestamp.tm_sec);

				_outputFfmpegPathFileName =
					_ffmpegTempDir + "/"
					+ to_string(ingestionJobKey)
					+ "_"
					+ to_string(encodingJobKey)
					+ "_"
					+ sUtcTimestamp
					+ ".liveProxy." + to_string(currentInputIndex) + ".log";
			}

			vector<string> ffmpegArgumentList;

			ffmpegArgumentList.push_back("ffmpeg");
			for (string parameter: ffmpegInputArgumentList)
				ffmpegArgumentList.push_back(parameter);
			for (string parameter: ffmpegOutputArgumentList)
				ffmpegArgumentList.push_back(parameter);

			time_t utcNow;
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

			if (!ffmpegArgumentList.empty())
				copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
					ostream_iterator<string>(ffmpegArgumentListStream, " "));

			_logger->info(__FILEREF__ + "liveProxy: Executing ffmpeg command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
			);

			startFfmpegCommand = chrono::system_clock::now();

			bool redirectionStdOutput = true;
			bool redirectionStdError = true;

			ProcessUtility::forkAndExec (
				_ffmpegPath + "/ffmpeg",
				ffmpegArgumentList,
				_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
				pChildPid, &iReturnedStatus);

			endFfmpegCommand = chrono::system_clock::now();

			if (iReturnedStatus != 0)
			{
				string errorMessage = __FILEREF__ + "liveProxy: Executed ffmpeg command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", currentInputIndex: " + to_string(currentInputIndex)
					+ ", currentNumberOfRepeatingSameInput: "
						+ to_string(currentNumberOfRepeatingSameInput)
					+ ", iReturnedStatus: " + to_string(iReturnedStatus)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "liveProxy: command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", currentInputIndex: " + to_string(currentInputIndex)
					+ ", currentNumberOfRepeatingSameInput: "
						+ to_string(currentNumberOfRepeatingSameInput)
				;
				throw runtime_error(errorMessage);
			}

			_logger->info(__FILEREF__ + "liveProxy: Executed ffmpeg command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					endFfmpegCommand - startFfmpegCommand).count()) + "@"
			);

			if (endlessPlaylistListPathName != ""
				&& FileIO::fileExisting(endlessPlaylistListPathName))
			{
				if (externalEncoder)
				{
					ifstream ifConfigurationFile (endlessPlaylistListPathName);
					if (ifConfigurationFile)
					{
						string configuration;
						string prefixFile = "file '";
						while(getline(ifConfigurationFile, configuration))
						{
							if (configuration.size() >= prefixFile.size()
								&& 0 == configuration.compare(0, prefixFile.size(),
									prefixFile))
							{
								string mediaFileName = configuration.substr(
									prefixFile.size(),
									configuration.size() - prefixFile.size() - 1);

								_logger->info(__FILEREF__ + "Remove"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
									+ ", mediaPathName: "
										+ _ffmpegEndlessRecursivePlaylistDir + "/"
										+ mediaFileName);
								bool exceptionInCaseOfError = false;
								FileIO::remove(_ffmpegEndlessRecursivePlaylistDir + "/"
									+ mediaFileName, exceptionInCaseOfError);    
							}
						}

						ifConfigurationFile.close();
					}
				}

				_logger->info(__FILEREF__ + "Remove"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", endlessPlaylistListPathName: " + endlessPlaylistListPathName);
				bool exceptionInCaseOfError = false;
				FileIO::remove(endlessPlaylistListPathName, exceptionInCaseOfError);    
				endlessPlaylistListPathName = "";
			}

			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = outputRoot.get("outputType", "").asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "")
						.asString();

					if (externalEncoder)
						removeFromIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

					if (manifestDirectoryPath != "")
					{
						if (FileIO::directoryExisting(manifestDirectoryPath))
						{
							try
							{
								_logger->info(__FILEREF__ + "removeDirectory"
									+ ", manifestDirectoryPath: " + manifestDirectoryPath
								);
								Boolean_t bRemoveRecursively = true;
								FileIO::removeDirectory(manifestDirectoryPath,
									bRemoveRecursively);
							}
							catch(runtime_error e)
							{
								string errorMessage = __FILEREF__ + "remove directory failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
									+ ", currentInputIndex: " + to_string(currentInputIndex)
									+ ", currentNumberOfRepeatingSameInput: "
										+ to_string(currentNumberOfRepeatingSameInput)
									+ ", manifestDirectoryPath: " + manifestDirectoryPath
									+ ", e.what(): " + e.what()
								;
								_logger->error(errorMessage);

								// throw e;
							}
							catch(exception e)
							{
								string errorMessage = __FILEREF__ + "remove directory failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
									+ ", currentInputIndex: " + to_string(currentInputIndex)
									+ ", currentNumberOfRepeatingSameInput: "
										+ to_string(currentNumberOfRepeatingSameInput)
									+ ", manifestDirectoryPath: " + manifestDirectoryPath
									+ ", e.what(): " + e.what()
								;
								_logger->error(errorMessage);

								// throw e;
							}
						}
					}
				}

				if (JSONUtils::isMetadataPresent(outputRoot, "drawTextDetails"))
				{
					string textTemporaryFileName;
					{
						textTemporaryFileName =
							_ffmpegTempDir + "/"
							+ to_string(ingestionJobKey)
							+ "_"
							+ to_string(encodingJobKey)
							+ "_"
							+ to_string(outputIndex)
							+ ".overlayText";
					}

					if (FileIO::fileExisting(textTemporaryFileName))
					{
						_logger->info(__FILEREF__ + "Remove"
							+ ", textTemporaryFileName: " + textTemporaryFileName);
						bool exceptionInCaseOfError = false;
						FileIO::remove(textTemporaryFileName, exceptionInCaseOfError);
					}
				}
			}

			if (streamingDurationInSeconds != -1 &&
				endFfmpegCommand - startFfmpegCommand < chrono::seconds(streamingDurationInSeconds - 60))
			{
				throw runtime_error(
					string("liveProxy exit before unexpectly, tried ")
					+ to_string(maxTimesRepeatingSameInput) + " times"
				);
			}
			else
			{
				// we finished one input and, may be, go to the next input

				currentNumberOfRepeatingSameInput = 0;
			}
		}
		catch(runtime_error e)
		{
			bool stoppedBySigQuit = false;

			string lastPartOfFfmpegOutputFile = getLastPartOfFile(
				_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
			string errorMessage;
			if (iReturnedStatus == 9)	// 9 means: SIGKILL
			{
				errorMessage = __FILEREF__
					+ "ffmpeg: ffmpeg execution command failed because killed by the user"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", currentInputIndex: " + to_string(currentInputIndex)
					+ ", currentNumberOfRepeatingSameInput: "
						+ to_string(currentNumberOfRepeatingSameInput)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
					+ ", e.what(): " + e.what()
				;
			}
			else
			{
				// signal: 3 is what the LiveProxy playlist is changed and
				//		we need to use the new playlist
				// e.what(): Child has exit abnormally because of an uncaught signal. Terminating signal: 3
				if (string(e.what()).find("signal: 3") != string::npos)
				{
					stoppedBySigQuit = true;

					errorMessage = __FILEREF__
						+ "ffmpeg execution stopped by SIGQUIT (3): ffmpeg command failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", currentInputIndex: " + to_string(currentInputIndex)
						+ ", currentNumberOfRepeatingSameInput: "
							+ to_string(currentNumberOfRepeatingSameInput)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
						+ ", e.what(): " + e.what()
					;
				}
				else
				{
					errorMessage = __FILEREF__ + "ffmpeg: ffmpeg execution command failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", currentInputIndex: " + to_string(currentInputIndex)
						+ ", currentNumberOfRepeatingSameInput: "
							+ to_string(currentNumberOfRepeatingSameInput)
						+ ", ffmpegCommandDuration (secs): @"
							+ to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startFfmpegCommand).count()) + "@"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
						+ ", e.what(): " + e.what()
					;
				}
			}
			_logger->error(errorMessage);

			/*
			_logger->info(__FILEREF__ + "Remove"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
			bool exceptionInCaseOfError = false;
			FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
			*/

			if (endlessPlaylistListPathName != ""
				&& FileIO::fileExisting(endlessPlaylistListPathName))
			{
				if (externalEncoder)
				{
					ifstream ifConfigurationFile (endlessPlaylistListPathName);
					if (ifConfigurationFile)
					{
						string configuration;
						string prefixFile = "file '";
						while(getline(ifConfigurationFile, configuration))
						{
							if (configuration.size() >= prefixFile.size()
								&& 0 == configuration.compare(0, prefixFile.size(),
									prefixFile))
							{
								string mediaFileName = configuration.substr(
									prefixFile.size(),
									configuration.size() - prefixFile.size() - 1);

								_logger->info(__FILEREF__ + "Remove"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
									+ ", mediaPathName: "
										+ _ffmpegEndlessRecursivePlaylistDir + "/"
										+ mediaFileName);
								bool exceptionInCaseOfError = false;
								FileIO::remove(_ffmpegEndlessRecursivePlaylistDir + "/"
									+ mediaFileName, exceptionInCaseOfError);    
							}
						}

						ifConfigurationFile.close();
					}
				}

				_logger->info(__FILEREF__ + "Remove"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", endlessPlaylistListPathName: " + endlessPlaylistListPathName);
				bool exceptionInCaseOfError = false;
				FileIO::remove(endlessPlaylistListPathName, exceptionInCaseOfError);    
				endlessPlaylistListPathName = "";
			}

			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = outputRoot.get("outputType", "").asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "")
						.asString();

					if (externalEncoder)
						removeFromIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

					if (manifestDirectoryPath != "")
					{
						if (FileIO::directoryExisting(manifestDirectoryPath))
						{
							try
							{
								_logger->info(__FILEREF__ + "removeDirectory"
									+ ", manifestDirectoryPath: " + manifestDirectoryPath
								);
								Boolean_t bRemoveRecursively = true;
								FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
							}
							catch(runtime_error e)
							{
								string errorMessage = __FILEREF__ + "remove directory failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
									+ ", currentInputIndex: " + to_string(currentInputIndex)
									+ ", currentNumberOfRepeatingSameInput: "
										+ to_string(currentNumberOfRepeatingSameInput)
									+ ", manifestDirectoryPath: " + manifestDirectoryPath
									+ ", e.what(): " + e.what()
								;
								_logger->error(errorMessage);

								// throw e;
							}
							catch(exception e)
							{
								string errorMessage = __FILEREF__ + "remove directory failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
									+ ", currentInputIndex: " + to_string(currentInputIndex)
									+ ", currentNumberOfRepeatingSameInput: "
										+ to_string(currentNumberOfRepeatingSameInput)
									+ ", manifestDirectoryPath: " + manifestDirectoryPath
									+ ", e.what(): " + e.what()
								;
								_logger->error(errorMessage);

								// throw e;
							}
						}
					}
				}

				if (JSONUtils::isMetadataPresent(outputRoot, "drawTextDetails"))
				{
					string textTemporaryFileName;
					{
						textTemporaryFileName =
							_ffmpegTempDir + "/"
							+ to_string(ingestionJobKey)
							+ "_"
							+ to_string(encodingJobKey)
							+ "_"
							+ to_string(outputIndex)
							+ ".overlayText";
					}

					if (FileIO::fileExisting(textTemporaryFileName))
					{
						_logger->info(__FILEREF__ + "Remove"
							+ ", textTemporaryFileName: " + textTemporaryFileName);
						bool exceptionInCaseOfError = false;
						FileIO::remove(textTemporaryFileName, exceptionInCaseOfError);
					}
				}
			}

			// next code will decide to throw an exception or not (we are in an error scenario) 

			if (iReturnedStatus == 9)	// 9 means: SIGKILL
				throw FFMpegEncodingKilledByUser();
			else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
				throw FFMpegURLForbidden();
			else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
				throw FFMpegURLNotFound();
			else if (!stoppedBySigQuit)
			{
				// see the comment before 'while'
				if (
					// terminato troppo presto
					(streamingDurationInSeconds != -1 &&
						endFfmpegCommand - startFfmpegCommand <
							chrono::seconds(streamingDurationInSeconds - 60)
					)
					// per almeno XXX minuti ha strimmato correttamente
					&& endFfmpegCommand - startFfmpegCommand > chrono::seconds(5 * 60)
				)
				{
					_logger->info(__FILEREF__ + "Command has to be executed again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ffmpegCommandDuration (secs): @"
							+ to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startFfmpegCommand).count()) + "@"
						+ ", currentNumberOfRepeatingSameInput: "
							+ to_string(currentNumberOfRepeatingSameInput)
						+ ", currentInputIndex: " + to_string(currentInputIndex)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);

					// in case of IP_PUSH the monitor thread, in case the client does not
					// reconnect istantaneously, kills the process.
					// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
					// we will set again the liveProxy->_proxyStart variable
					{
						// pushListenTimeout in case it is not PUSH, it will be -1
						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
								chrono::system_clock::now())
								*pProxyStart = chrono::system_clock::now() +
									chrono::seconds(pushListenTimeout);
							else
								*pProxyStart = chrono::system_clock::from_time_t(
									utcProxyPeriodStart) +
									chrono::seconds(pushListenTimeout);
						}
						else
							*pProxyStart = chrono::system_clock::now() +
								chrono::seconds(pushListenTimeout);
					}
				}
				else
				{
					currentNumberOfRepeatingSameInput++;
					if (currentNumberOfRepeatingSameInput >= maxTimesRepeatingSameInput)
					{
						_logger->info(__FILEREF__
							+ "Command is NOT executed anymore, reached max number of repeating"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", currentNumberOfRepeatingSameInput: "
								+ to_string(currentNumberOfRepeatingSameInput)
							+ ", maxTimesRepeatingSameInput: "
								+ to_string(maxTimesRepeatingSameInput)
							+ ", sleepInSecondsInCaseOfRepeating: "
								+ to_string(sleepInSecondsInCaseOfRepeating)
							+ ", currentInputIndex: " + to_string(currentInputIndex)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
						throw e;
					}
					else
					{
						_logger->info(__FILEREF__ + "Command is executed again"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", currentNumberOfRepeatingSameInput: "
								+ to_string(currentNumberOfRepeatingSameInput)
							+ ", sleepInSecondsInCaseOfRepeating: "
								+ to_string(sleepInSecondsInCaseOfRepeating)
							+ ", currentInputIndex: " + to_string(currentInputIndex)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);

						// in case of IP_PUSH the monitor thread, in case the client does not
						// reconnect istantaneously, kills the process.
						// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
						// we will set again the liveProxy->_proxyStart variable
						{
							if (utcProxyPeriodStart != -1)
							{
								if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
									chrono::system_clock::now())
									*pProxyStart = chrono::system_clock::now() +
										chrono::seconds(pushListenTimeout);
								else
									*pProxyStart = chrono::system_clock::from_time_t(
										utcProxyPeriodStart) +
										chrono::seconds(pushListenTimeout);
							}
							else
								*pProxyStart = chrono::system_clock::now() +
									chrono::seconds(pushListenTimeout);
						}

						currentInputIndex--;
						this_thread::sleep_for(chrono::seconds(sleepInSecondsInCaseOfRepeating));
					}
				}
			}
			else // if (stoppedBySigQuit)
			{
				// in case of IP_PUSH the monitor thread, in case the client does not
				// reconnect istantaneously, kills the process.
				// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
				// we will set again the liveProxy->_proxyStart variable
				{
					if (utcProxyPeriodStart != -1)
					{
						if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
							chrono::system_clock::now())
							*pProxyStart = chrono::system_clock::now() +
								chrono::seconds(pushListenTimeout);
						else
							*pProxyStart = chrono::system_clock::from_time_t(
								utcProxyPeriodStart) +
								chrono::seconds(pushListenTimeout);
					}
					else
						*pProxyStart = chrono::system_clock::now() +
							chrono::seconds(pushListenTimeout);
				}

				// 2022-10-21: this is the scenario where the LiveProxy playlist is changed (signal: 3)
				//	and we need to use the new playlist
				//	This is the ffmpeg 'client side'.
				//	The above condition (!stoppedBySigQuit) is the scenario server side.
				//	This is what happens:
				//		time A: a new playlist is received by MMS and a SigQuit (signal: 3) is sent
				//			to the ffmpeg client side
				//		time A + few milliseconds: the ffmpeg client side starts again
				//			with the new 'input' (1)
				//		time A + few seconds: The server ffmpeg recognizes the client disconnect and exit
				//		time A + few seconds + few milliseconds: The ffmpeg server side starts again (2)
				//
				//	The problem is that The ffmpeg server starts too late (2). The ffmpeg client (1)
				//	already failed because the ffmpeg server was not listening yet.
				//	So ffmpeg client exit from this method, reach the engine and returns after about 15 seconds.
				//	In this scenario the player already disconnected and has to retry again the URL to start again.
				//
				//	To avoid this problem, we add here (ffmpeg client) a delay to wait ffmpeg server to starts
				//	Based on my statistics I think 2 seconds should be enought
				int sleepInSecondsToBeSureServerIsRunning = 2;
				this_thread::sleep_for(chrono::seconds(sleepInSecondsToBeSureServerIsRunning));
			}
		}

		/*
		_logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", currentInputIndex: " + to_string(currentInputIndex)
			+ ", currentNumberOfRepeatingSameInput: "
				+ to_string(currentNumberOfRepeatingSameInput)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		bool exceptionInCaseOfError = false;
		FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
		*/
	}
}

// return the index of the selected input
int FFMpeg::getNextLiveProxyInput(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	Json::Value* inputsRoot,	// IN: list of inputs
	mutex* inputsRootMutex,		// IN: mutex
	int currentInputIndex,		// IN: current index on the inputs
	bool timedInput,			// IN: specify if the input is "timed". If "timed", next input is calculated based on the current time, otherwise it is retrieved simply the next
	Json::Value* newInputRoot	// OUT: refer the input to be run
)
{
	lock_guard<mutex> locker(*inputsRootMutex);

	int newInputIndex = -1;
	*newInputRoot = Json::nullValue;

	if (timedInput)
	{
		int64_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

		for (int inputIndex = 0; inputIndex < inputsRoot->size(); inputIndex++)
		{
			Json::Value inputRoot = (*inputsRoot)[inputIndex];

			string field = "utcScheduleStart";
			int64_t utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// if (utcProxyPeriodStart == -1)
			// {
			// 	field = "utcProxyPeriodStart";
			// 	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			field = "utcScheduleEnd";
			int64_t utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// if (utcProxyPeriodEnd == -1)
			// {
			// 	field = "utcProxyPeriodEnd";
			// 	utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			_logger->info(__FILEREF__ + "getNextLiveProxyInput"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", timedInput: " + to_string(timedInput)
				+ ", inputIndex: " + to_string(inputIndex)
				+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
				+ ", utcNow: " + to_string(utcNow)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
			);

			if (utcProxyPeriodStart <= utcNow && utcNow < utcProxyPeriodEnd)
			{
				*newInputRoot = (*inputsRoot)[inputIndex];
				newInputIndex = inputIndex;

				break;
			}
		}
	}
	else
	{
		newInputIndex = currentInputIndex + 1;

		if (newInputIndex < inputsRoot->size())
			*newInputRoot = (*inputsRoot)[newInputIndex];
		else
			newInputIndex = -1;
	}

	_logger->info(__FILEREF__ + "getNextLiveProxyInput"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", currentInputIndex: " + to_string(currentInputIndex)
		+ ", timedInput: " + to_string(timedInput)
		+ ", newInputIndex: " + to_string(newInputIndex)
	);

	return newInputIndex;
}

tuple<long, string, string, int, int64_t> FFMpeg::liveProxyInput(
	int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder,
	Json::Value inputRoot, vector<string>& ffmpegInputArgumentList)
{
	long streamingDurationInSeconds = -1;
	string otherOutputOptionsBecauseOfMaxWidth;
	string endlessPlaylistListPathName;
	int pushListenTimeout = -1;
	int64_t utcProxyPeriodStart = -1;


	// "inputRoot": {
	//	"timePeriod": false, "utcScheduleEnd": -1, "utcScheduleStart": -1 
	//	...
	// }
	bool timePeriod = false;
	string field = "timePeriod";
	if (JSONUtils::isMetadataPresent(inputRoot, field))
		timePeriod = JSONUtils::asBool(inputRoot, field, false);

	// int64_t utcProxyPeriodStart = -1;
	field = "utcScheduleStart";
	if (JSONUtils::isMetadataPresent(inputRoot, field))
		utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
	// else
	// {
	// 	field = "utcProxyPeriodStart";
	// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
	// 		utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
	// }

	int64_t utcProxyPeriodEnd = -1;
	field = "utcScheduleEnd";
	if (JSONUtils::isMetadataPresent(inputRoot, field))
		utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
	// else
	// {
	// 	field = "utcProxyPeriodEnd";
	// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
	// 		utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
	// }

	//	"streamInput": { "captureAudioDeviceNumber": -1, "captureChannelsNumber": -1, "captureFrameRate": -1, "captureHeight": -1, "captureVideoDeviceNumber": -1, "captureVideoInputFormat": "", "captureWidth": -1, "confKey": 2464, "configurationLabel": "Italia-nazionali-Diretta canale satellitare della Camera dei deputati", "streamSourceType": "IP_PULL", "pushListenTimeout": -1, "tvAudioItalianPid": -1, "tvFrequency": -1, "tvModulation": "", "tvServiceId": -1, "tvSymbolRate": -1, "tvVideoPid": -1, "url": "https://www.youtube.com/watch?v=Cnjs83yowUM", "maxWidth": -1, "userAgent": "", "otherInputOptions": "" },
	if (JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
	{
		field = "streamInput";
		Json::Value streamInputRoot = inputRoot[field];

		field = "streamSourceType";
		if (!JSONUtils::isMetadataPresent(streamInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string streamSourceType = streamInputRoot.get(field, "").asString();

		int maxWidth = -1;
		field = "maxWidth";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			maxWidth = JSONUtils::asInt(streamInputRoot, field, -1);

		string url;
		field = "url";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			url = streamInputRoot.get(field, "").asString();

		string userAgent;
		field = "userAgent";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			userAgent = streamInputRoot.get(field, "").asString();

		// int pushListenTimeout = -1;
		field = "pushListenTimeout";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			pushListenTimeout = JSONUtils::asInt(streamInputRoot, field, -1);

		string otherInputOptions;
		field = "otherInputOptions";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			otherInputOptions = streamInputRoot.get(field, "").asString();

		string captureLive_videoInputFormat;
		field = "captureVideoInputFormat";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			captureLive_videoInputFormat = streamInputRoot.get(field, "").asString();

		int captureLive_frameRate = -1;
		field = "captureFrameRate";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			captureLive_frameRate = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_width = -1;
		field = "captureWidth";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			captureLive_width = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_height = -1;
		field = "captureHeight";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			captureLive_height = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_videoDeviceNumber = -1;
		field = "captureVideoDeviceNumber";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			captureLive_videoDeviceNumber = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_channelsNumber = -1;
		field = "captureChannelsNumber";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			captureLive_channelsNumber = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_audioDeviceNumber = -1;
		field = "captureAudioDeviceNumber";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			captureLive_audioDeviceNumber = JSONUtils::asInt(streamInputRoot, field, -1);

		_logger->info(__FILEREF__ + "liveProxy: setting dynamic -map option"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", timePeriod: " + to_string(timePeriod)
			+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
			+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
			+ ", streamSourceType: " + streamSourceType
		);

		if (streamSourceType == "IP_PULL" && maxWidth != -1)
		{
			try
			{
				_logger->info(__FILEREF__ + "liveProxy: setting dynamic -map option"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", maxWidth: " + to_string(maxWidth)
				);

				vector<tuple<int, string, string, string, string, int, int>>	videoTracks;
				vector<tuple<int, string, string, string, int, bool>>			audioTracks;

				getLiveStreamingInfo(
					url,
					userAgent,
					ingestionJobKey,
					encodingJobKey,
					videoTracks,
					audioTracks
				);

				int currentVideoWidth = -1;
				string selectedVideoStreamId;
				string selectedAudioStreamId;
				for(tuple<int, string, string, string, string, int, int> videoTrack:
					videoTracks)
				{
					int videoProgramId;
					string videoStreamId;
					string videoStreamDescription;
					string videoCodec;
					string videoYUV;
					int videoWidth;
					int videoHeight;

					tie(videoProgramId, videoStreamId, videoStreamDescription,                  
						videoCodec, videoYUV, videoWidth, videoHeight) = videoTrack;

					if (videoStreamId != ""
						&& videoWidth != -1 && videoWidth <= maxWidth
						&& (currentVideoWidth == -1 || videoWidth > currentVideoWidth)
					)
					{
						// look an audio belonging to the same Program
						for (tuple<int, string, string, string, int, bool> audioTrack: audioTracks)
						{
							int audioProgramId;
							string audioStreamId;
							string audioStreamDescription;
							string audioCodec;
							int audioSamplingRate;
							bool audioStereo;

							tie(audioProgramId, audioStreamId, audioStreamDescription,
								audioCodec, audioSamplingRate, audioStereo) = audioTrack;

							if (audioStreamDescription.find("eng") != string::npos
								|| audioStreamDescription.find("des") != string::npos
							)
							{
								_logger->info(__FILEREF__ + "liveProxy: audio track discarded"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
									+ ", audioStreamId: " + audioStreamId
									+ ", audioStreamDescription: " + audioStreamDescription
								);

								continue;
							}

							if (videoProgramId == audioProgramId
								&& audioStreamId != "")
							{
								selectedVideoStreamId = videoStreamId;
								selectedAudioStreamId = audioStreamId;

								currentVideoWidth = videoWidth;

								break;
							}
						}
					}
				}

				if (selectedVideoStreamId != "" && selectedAudioStreamId != "")
				{
					otherOutputOptionsBecauseOfMaxWidth =
						string(" -map ") + selectedVideoStreamId + " -map " + selectedAudioStreamId;
				}

				_logger->info(__FILEREF__ + "liveProxy: new other output options"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", maxWidth: " + to_string(maxWidth)
					+ ", otherOutputOptionsBecauseOfMaxWidth: " + otherOutputOptionsBecauseOfMaxWidth
				);
			}
			catch(runtime_error e)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: getLiveStreamingInfo or associate processing failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				// throw e;
			}
		}

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		{
			if (timePeriod)
			{
				streamingDurationInSeconds = utcProxyPeriodEnd - utcNow;

				_logger->info(__FILEREF__ + "LiveProxy timing. "
					+ "Streaming duration"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", utcNow: " + to_string(utcNow)
					+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
					+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
					+ ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
				);

				if (streamSourceType == "IP_PUSH" || streamSourceType == "TV")
				{
					if (pushListenTimeout > 0 && pushListenTimeout > streamingDurationInSeconds)
					{
						// 2021-02-02: sceanrio:
						//	streaming duration is 25 seconds
						//	timeout: 3600 seconds
						//	The result is that the process will finish after 3600 seconds, not after 25 seconds
						//	To avoid that, in this scenario, we will set the timeout equals to streamingDurationInSeconds
						_logger->info(__FILEREF__ + "LiveProxy timing. "
							+ "Listen timeout in seconds is reduced because max after 'streamingDurationInSeconds' the process has to finish"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", utcNow: " + to_string(utcNow)
							+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
							+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
							+ ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
							+ ", pushListenTimeout: " + to_string(pushListenTimeout)
						);

						pushListenTimeout = streamingDurationInSeconds;
					}
				}
			}

			// user agent is an HTTP header and can be used only in case of http request
			bool userAgentToBeUsed = false;
			if (streamSourceType == "IP_PULL" && userAgent != "")
			{
				string httpPrefix = "http";	// it includes also https
				if (url.size() >= httpPrefix.size()
					&& url.compare(0, httpPrefix.size(), httpPrefix) == 0)
				{
					userAgentToBeUsed = true;
				}
				else
				{
					_logger->warn(__FILEREF__ + "user agent cannot be used if not http"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", url: " + url
					);
				}
			}

			// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

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
			//	-nostdin: Disabling interaction on standard input, it is useful, for example, if ffmpeg is
			//		in the background process group
			ffmpegInputArgumentList.push_back("-nostdin");
			if (userAgentToBeUsed)
			{
				ffmpegInputArgumentList.push_back("-user_agent");
				ffmpegInputArgumentList.push_back(userAgent);
			}
			ffmpegInputArgumentList.push_back("-re");
			FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegInputArgumentList);
			if (streamSourceType == "IP_PUSH")
			{
				// listen/timeout depend on the protocol (https://ffmpeg.org/ffmpeg-protocols.html)
				if (
					url.find("http://") != string::npos
					|| url.find("rtmp://") != string::npos
				)
				{
					ffmpegInputArgumentList.push_back("-listen");
					ffmpegInputArgumentList.push_back("1");
					if (pushListenTimeout > 0)
					{
						// no timeout means it will listen infinitely
						ffmpegInputArgumentList.push_back("-timeout");
						ffmpegInputArgumentList.push_back(to_string(pushListenTimeout));
					}
				}
				else if (
					url.find("udp://") != string::npos
				)
				{
					if (pushListenTimeout > 0)
					{
						// About the timeout url parameter, ffmpeg docs says: This option is only relevant
						//	in read mode: if no data arrived in more than this time interval, raise error
						// This parameter accepts microseconds and we cannot provide a huge number
						// i.e. 1h in microseconds because it will not work (it will be the max number
						// of a 'long' type).
						// For this reason we have to set max 30 minutes
						//
						// Remark: this is just a read timeout, then we have below the -t parameter
						//	that will stop the ffmpeg command after a specified time.
						//  So, in case for example we have to run this command for 1h, we will have
						//  ?timeout=1800000000 (30 mins) and -t 3600
						//  ONLY in case it is not received any data for 30 mins, this command will exit
						//  after 30 mins (because of the ?timeout parameter) and the system will run
						// again the command again for the remaining 30 minutes:
						//  ?timeout=1800000000 (30 mins) and -t 180

						int maxPushTimeout = 180;	// 30 mins
						int64_t listenTimeoutInMicroSeconds;
						if (pushListenTimeout > maxPushTimeout)
							listenTimeoutInMicroSeconds = maxPushTimeout;
						else
							listenTimeoutInMicroSeconds = pushListenTimeout;
						listenTimeoutInMicroSeconds *= 1000000;

						if (url.find("?") == string::npos)
							url += ("?timeout=" + to_string(listenTimeoutInMicroSeconds));
						else
							url += ("&timeout=" + to_string(listenTimeoutInMicroSeconds));
					}

					// In case of udp:
					// overrun_nonfatal=1 prevents ffmpeg from exiting,
					//		it can recover in most circumstances.
					// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
					if (url.find("?") == string::npos)
						url += "?overrun_nonfatal=1&fifo_size=50000000";
					else
						url += "&overrun_nonfatal=1&fifo_size=50000000";
				}
				else
				{
					_logger->error(__FILEREF__ + "listen/timeout not managed yet for the current protocol"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", url: " + url
					);
				}

				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);
			}
			else if (streamSourceType == "IP_PULL")
			{
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);
			}
			else if (streamSourceType == "TV")
			{
				if (url.find("udp://") != string::npos)
				{
					// In case of udp:
					// overrun_nonfatal=1 prevents ffmpeg from exiting,
					//		it can recover in most circumstances.
					// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
					if (url.find("?") == string::npos)
						url += "?overrun_nonfatal=1&fifo_size=50000000";
					else
						url += "&overrun_nonfatal=1&fifo_size=50000000";
				}

				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);
			}
			else if (streamSourceType == "CaptureLive")
			{
				// video
				{
					// -f v4l2 -framerate 25 -video_size 640x480 -i /dev/video0
					ffmpegInputArgumentList.push_back("-f");
					ffmpegInputArgumentList.push_back("v4l2");

					ffmpegInputArgumentList.push_back("-thread_queue_size");
					ffmpegInputArgumentList.push_back("4096");

					if (captureLive_videoInputFormat != "")
					{
						ffmpegInputArgumentList.push_back("-input_format");
						ffmpegInputArgumentList.push_back(captureLive_videoInputFormat);
					}

					if (captureLive_frameRate != -1)
					{
						ffmpegInputArgumentList.push_back("-framerate");
						ffmpegInputArgumentList.push_back(to_string(captureLive_frameRate));
					}

					if (captureLive_width != -1 && captureLive_height != -1)
					{
						ffmpegInputArgumentList.push_back("-video_size");
						ffmpegInputArgumentList.push_back(
							to_string(captureLive_width) + "x" + to_string(captureLive_height));
					}

					ffmpegInputArgumentList.push_back("-i");
					ffmpegInputArgumentList.push_back(string("/dev/video")
						+ to_string(captureLive_videoDeviceNumber));
				}

				// audio
				{
					ffmpegInputArgumentList.push_back("-f");
					ffmpegInputArgumentList.push_back("alsa");

					ffmpegInputArgumentList.push_back("-thread_queue_size");
					ffmpegInputArgumentList.push_back("2048");

					if (captureLive_channelsNumber != -1)
					{
						ffmpegInputArgumentList.push_back("-ac");
						ffmpegInputArgumentList.push_back(to_string(captureLive_channelsNumber));
					}

					ffmpegInputArgumentList.push_back("-i");
					ffmpegInputArgumentList.push_back(string("hw:")
						+ to_string(captureLive_audioDeviceNumber));
				}
			}

			if (timePeriod)
			{
				ffmpegInputArgumentList.push_back("-t");
				ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));
			}
		}
	}
	//	"directURLInput": { "url": "" },
	else if (JSONUtils::isMetadataPresent(inputRoot, "directURLInput"))
	{
		field = "directURLInput";
		Json::Value streamInputRoot = inputRoot[field];

		string url;
		field = "url";
		if (JSONUtils::isMetadataPresent(streamInputRoot, field))
			url = streamInputRoot.get(field, "").asString();

		_logger->info(__FILEREF__ + "liveProxy: setting dynamic -map option"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", timePeriod: " + to_string(timePeriod)
			+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
			+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
			+ ", url: " + url
		);

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		{
			if (timePeriod)
			{
				streamingDurationInSeconds = utcProxyPeriodEnd - utcNow;

				_logger->info(__FILEREF__ + "LiveProxy timing. "
					+ "Streaming duration"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", utcNow: " + to_string(utcNow)
					+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
					+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
					+ ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
				);
			}

			// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

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
			//	-nostdin: Disabling interaction on standard input, it is useful, for example, if ffmpeg is
			//		in the background process group
			ffmpegInputArgumentList.push_back("-nostdin");
			ffmpegInputArgumentList.push_back("-re");
			{
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);
			}

			if (timePeriod)
			{
				ffmpegInputArgumentList.push_back("-t");
				ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));
			}
		}
	}
	//	"vodInput": { "vodContentType": "", "sources": [{"sourcePhysicalPathName": "..."}],
	//		"otherInputOptions": "" },
	else if (JSONUtils::isMetadataPresent(inputRoot, "vodInput"))
	{
		string field = "vodInput";
		Json::Value vodInputRoot = inputRoot[field];

		field = "vodContentType";
		if (!JSONUtils::isMetadataPresent(vodInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string vodContentType = vodInputRoot.get(field, "Video").asString();

		vector<string> sources;
		// int64_t durationOfInputsInMilliSeconds = 0;
		{
			field = "sources";
			if (!JSONUtils::isMetadataPresent(vodInputRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value sourcesRoot = vodInputRoot[field];

			for(int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
			{
				Json::Value sourceRoot = sourcesRoot[sourceIndex];

				if (externalEncoder)
					field = "sourcePhysicalDeliveryURL";
				else
					field = "sourcePhysicalPathName";
				if (!JSONUtils::isMetadataPresent(sourceRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sourcePhysicalReference = sourceRoot.get(field, "").asString();
				sources.push_back(sourcePhysicalReference);

				// field = "durationInMilliSeconds";
				// if (JSONUtils::isMetadataPresent(sourceRoot, field))
				// 	durationOfInputsInMilliSeconds += JSONUtils::asInt64(sourceRoot, field, 0);
			}
		}

		string otherInputOptions;
		field = "otherInputOptions";
		if (JSONUtils::isMetadataPresent(vodInputRoot, field))
			otherInputOptions = vodInputRoot.get(field, "").asString();


		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		{
			if (timePeriod)
			{
				streamingDurationInSeconds = utcProxyPeriodEnd - utcNow;

				_logger->info(__FILEREF__ + "VODProxy timing. "
					+ "Streaming duration"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", utcNow: " + to_string(utcNow)
					+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
					+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
					+ ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
				);
			}

			// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

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
			//	-nostdin: Disabling interaction on standard input, it is useful, for example, if ffmpeg is
			//		in the background process group
			ffmpegInputArgumentList.push_back("-nostdin");

			ffmpegInputArgumentList.push_back("-re");
			FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegInputArgumentList);

			if (vodContentType == "Image")
			{
				ffmpegInputArgumentList.push_back("-r");
				ffmpegInputArgumentList.push_back("25");

				ffmpegInputArgumentList.push_back("-loop");
				ffmpegInputArgumentList.push_back("1");
			}
			else
			{
				/*
					2022-10-27: -stream_loop works only in case of ONE input.
						In case of multiple VODs we will use the '-f concat' option implementing
						an endless recursive playlist
						see https://video.stackexchange.com/questions/18982/is-it-possible-to-create-an-endless-loop-using-concat
				*/
				if (sources.size() == 1)
				{
					ffmpegInputArgumentList.push_back("-stream_loop");
					ffmpegInputArgumentList.push_back("-1");
				}
			}

			if (sources.size() == 1)
			{
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(sources[0]);
			}
			else // if (sources.size() > 1)
			{
				// ffmpeg concat demuxer supports nested scripts with the header "ffconcat version 1.0".
				// Build the endless recursive playlist file like (i.e:
				//	ffconcat version 1.0
				//	file 'storage/MMSRepository/MMS_0003/1/000/004/016/2030954_97080_24.mp4'
				//	file 'storage/MMSRepository/MMS_0003/1/000/004/235/2143253_99028_24.mp4'
				//	...
				//	file 'XXX_YYY_endlessPlaylist.txt'

				string endlessPlaylistListFileName =
					to_string(ingestionJobKey) + "_" + to_string(encodingJobKey)
					+ "_endlessPlaylist.txt";
				endlessPlaylistListPathName = _ffmpegEndlessRecursivePlaylistDir
					+ "/" + endlessPlaylistListFileName;
				;
        
				ofstream playlistListFile(endlessPlaylistListPathName.c_str(), ofstream::trunc);
				playlistListFile << "ffconcat version 1.0" << endl;
				for(string sourcePhysicalReference: sources)
				{
					// sourcePhysicalReference will be:
					//	a URL in case of externalEncoder 
					//	a storage path name in case of a local encoder

					_logger->info(__FILEREF__ + "ffmpeg: adding physical path"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", sourcePhysicalReference: " + sourcePhysicalReference
					);

					if (externalEncoder)
					{
						bool isStreaming = false;

						string destBinaryPathName;
						string destBinaryFileName;
						{
							size_t fileNameIndex = sourcePhysicalReference.find_last_of("/");
							if (fileNameIndex == string::npos)
							{
								_logger->error(__FILEREF__ + "physical path has a wrong path"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", sourcePhysicalReference: " + sourcePhysicalReference
								);

								continue;
							}
							destBinaryFileName = sourcePhysicalReference.substr(fileNameIndex + 1);

							size_t extensionIndex = destBinaryFileName.find_last_of(".");
							if (extensionIndex != string::npos)
							{
								if (destBinaryFileName.substr(extensionIndex + 1) == "m3u8")
								{
									isStreaming = true;
									destBinaryFileName = destBinaryFileName.substr(0, extensionIndex) + ".mp4";
								}
							}

							destBinaryPathName = _ffmpegEndlessRecursivePlaylistDir
								+ "/" + destBinaryFileName;
						}

						// sourcePhysicalReference is like https://mms-delivery-path.catramms-cloud.com/token_mDEs0rZTXRyMkOCngnG87w==,1666987919/MMS_0000/1/000/229/507/1429406_231284_changeFileFormat.mp4
						if (isStreaming)
						{
							// regenerateTimestamps: see docs/TASK_01_Add_Content_JSON_Format.txt
							bool regenerateTimestamps = false;

							streamingToFile(
								ingestionJobKey,
								regenerateTimestamps,
								sourcePhysicalReference,
								destBinaryPathName);
						}
						else
						{
							MMSCURL::downloadFile(
								ingestionJobKey,
								sourcePhysicalReference,
								destBinaryPathName,
								_logger
							);
						}
						// playlist and dowloaded files will be removed by the calling FFMpeg::liveProxy2 method
						playlistListFile << "file '" << destBinaryFileName << "'" << endl;
					}
					else
					{
						size_t storageIndex = sourcePhysicalReference.find("/storage/");
						if (storageIndex == string::npos)
						{
							_logger->error(__FILEREF__ + "physical path has a wrong path"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", sourcePhysicalReference: " + sourcePhysicalReference
							);

							continue;
						}

						playlistListFile << "file '"
							<< sourcePhysicalReference.substr(storageIndex + 1)
							<< "'" << endl;
					}
				}
				playlistListFile << "file '" << endlessPlaylistListFileName << "'" << endl;
				playlistListFile.close();

				ffmpegInputArgumentList.push_back("-f");
				ffmpegInputArgumentList.push_back("concat");
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(endlessPlaylistListPathName);
			}

			if (timePeriod)
			{
				ffmpegInputArgumentList.push_back("-t");
				ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));
			}
		}
	}
	//	"countdownInput": { "mmsSourceVideoAssetPathName": "", "videoDurationInMilliSeconds": 123, "text": "", "textPosition_X_InPixel": "", "textPosition_Y_InPixel": "", "fontType": "", "fontSize": 22, "fontColor": "", "textPercentageOpacity": -1, "boxEnable": false, "boxColor": "", "boxPercentageOpacity": 20 },
	else if (JSONUtils::isMetadataPresent(inputRoot, "countdownInput"))
	{
		string field = "countdownInput";
		Json::Value countdownInputRoot = inputRoot[field];

		if (externalEncoder)
			field = "mmsSourceVideoAssetDeliveryURL";
		else
			field = "mmsSourceVideoAssetPathName";
		if (!JSONUtils::isMetadataPresent(countdownInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string mmsSourceVideoAssetPathName = countdownInputRoot.get(field, "").asString();

		field = "videoDurationInMilliSeconds";
		if (!JSONUtils::isMetadataPresent(countdownInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(countdownInputRoot, field, -1);

		if (!externalEncoder
			&& !FileIO::fileExisting(mmsSourceVideoAssetPathName)        
			&& !FileIO::directoryExisting(mmsSourceVideoAssetPathName)
		)
		{
			string errorMessage = string("Source video asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t utcCountDownEnd = utcProxyPeriodEnd;

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		if (utcCountDownEnd <= utcNow)
		{
			time_t tooLateTime = utcNow - utcCountDownEnd;

			string errorMessage = __FILEREF__ + "Countdown timing. "
				+ "Too late to start"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", utcNow: " + to_string(utcNow)
				+ ", utcCountDownEnd: " + to_string(utcCountDownEnd)
				+ ", tooLateTime: " + to_string(tooLateTime)
				;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		int streamLoopNumber;
		{
			streamingDurationInSeconds = utcCountDownEnd - utcNow;

			_logger->info(__FILEREF__ + "Countdown timing. "
				+ "Streaming duration"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", utcNow: " + to_string(utcNow)
				+ ", utcCountDownEnd: " + to_string(utcCountDownEnd)
				+ ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
				+ ", videoDurationInMilliSeconds: " + to_string(videoDurationInMilliSeconds)
			);

			float fVideoDurationInMilliSeconds = videoDurationInMilliSeconds;
			fVideoDurationInMilliSeconds /= 1000;

			streamLoopNumber = streamingDurationInSeconds
				/ fVideoDurationInMilliSeconds;
			streamLoopNumber += 2;
		}

		{
			// global options
			// input options
			ffmpegInputArgumentList.push_back("-re");
			ffmpegInputArgumentList.push_back("-stream_loop");
			ffmpegInputArgumentList.push_back(to_string(streamLoopNumber));
			ffmpegInputArgumentList.push_back("-i");
			ffmpegInputArgumentList.push_back(mmsSourceVideoAssetPathName);
			ffmpegInputArgumentList.push_back("-t");
			ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));

			// ffmpegInputArgumentList.push_back("-vf");
			// ffmpegInputArgumentList.push_back(ffmpegDrawTextFilter);
		}
	}
	else
	{
		string errorMessage = __FILEREF__ + "streamInput or vodInput or countdownInput is not present"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	return make_tuple(streamingDurationInSeconds, otherOutputOptionsBecauseOfMaxWidth,
		endlessPlaylistListPathName, pushListenTimeout, utcProxyPeriodStart);
}

void FFMpeg::liveProxyOutput(int64_t ingestionJobKey, int64_t encodingJobKey,
	bool externalEncoder,
	string otherOutputOptionsBecauseOfMaxWidth,
	Json::Value inputRoot,
	long streamingDurationInSeconds,
	Json::Value outputsRoot,
	vector<string>& ffmpegOutputArgumentList)
{
	// 2022-09-12: next if è usato solo nel caso di broadcastDrawTextDetails.
	//		Infatti, in genere i parametri del 'draw text' vengono inizializzati
	//		all'interno di outputRoot.
	//		Nel caso del Broadcast (Live Channel), outputRoot è comune a tutta la playlist,
	//		per cui non possiamo utilizzare outputRoot altrimenti avremmo il draw text
	//		anche per gli altri item della playlist quali LiveProxy, VODProxy, ...
	//		Per questo motivo:
	//			1. vngono aggiunti questi parametri in forma eccezionale per il Broadcast
	//			2. questi parametri saranno gestiti qui
	string ffmpegDrawTextFilter;
	if (JSONUtils::isMetadataPresent(inputRoot, "countdownInput"))
	{
		Json::Value countdownInputRoot = inputRoot["countdownInput"];

		if (JSONUtils::isMetadataPresent(countdownInputRoot, "broadcastDrawTextDetails"))
		{
			Json::Value broadcastDrawTextDetailsRoot = countdownInputRoot["broadcastDrawTextDetails"];

			string field = "text";
			if (!JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string text = broadcastDrawTextDetailsRoot.get(field, "").asString();

			int reloadAtFrameInterval = -1;
			field = "reloadAtFrameInterval";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				reloadAtFrameInterval = JSONUtils::asInt(broadcastDrawTextDetailsRoot, field, -1);

			string textPosition_X_InPixel = "";
			field = "textPosition_X_InPixel";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				textPosition_X_InPixel = broadcastDrawTextDetailsRoot.get(field, "").asString();

			string textPosition_Y_InPixel = "";
			field = "textPosition_Y_InPixel";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				textPosition_Y_InPixel = broadcastDrawTextDetailsRoot.get(field, "").asString();

			string fontType = "";
			field = "fontType";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				fontType = broadcastDrawTextDetailsRoot.get(field, "").asString();

			int fontSize = -1;
			field = "fontSize";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				fontSize = JSONUtils::asInt(broadcastDrawTextDetailsRoot, field, -1);

			string fontColor = "";
			field = "fontColor";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				fontColor = broadcastDrawTextDetailsRoot.get(field, "").asString();

			int textPercentageOpacity = -1;
			field = "textPercentageOpacity";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				textPercentageOpacity = JSONUtils::asInt(broadcastDrawTextDetailsRoot, field, -1);

			int shadowx = 0;
			field = "shadowx";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				shadowx = JSONUtils::asInt(broadcastDrawTextDetailsRoot, field, -1);

			int shadowy = 0;
			field = "shadowy";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				shadowy = JSONUtils::asInt(broadcastDrawTextDetailsRoot, field, -1);

			bool boxEnable = false;
			field = "boxEnable";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				boxEnable = JSONUtils::asBool(broadcastDrawTextDetailsRoot, field, false);

			string boxColor = "";
			field = "boxColor";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				boxColor = broadcastDrawTextDetailsRoot.get(field, "").asString();

			int boxPercentageOpacity = -1;
			field = "boxPercentageOpacity";
			if (JSONUtils::isMetadataPresent(broadcastDrawTextDetailsRoot, field))
				boxPercentageOpacity = JSONUtils::asInt(broadcastDrawTextDetailsRoot, field, -1);

			ffmpegDrawTextFilter = getDrawTextVideoFilterDescription(ingestionJobKey,
				text, "", reloadAtFrameInterval, textPosition_X_InPixel, textPosition_Y_InPixel,
				fontType, fontSize, fontColor, textPercentageOpacity, shadowx, shadowy,
				boxEnable, boxColor, boxPercentageOpacity,
				streamingDurationInSeconds);
		}
	}

	for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		Json::Value outputRoot = outputsRoot[outputIndex];

		string outputType = outputRoot.get("outputType", "").asString();

		Json::Value filtersRoot = Json::nullValue;
		if (JSONUtils::isMetadataPresent(outputRoot, "filters"))
			filtersRoot = outputRoot["filters"];

		Json::Value encodingProfileDetailsRoot = Json::nullValue;
		if (JSONUtils::isMetadataPresent(outputRoot, "encodingProfileDetails"))
			encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];

		string otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();

		int videoTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, "videoTrackIndexToBeUsed", -1);
		int audioTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, "audioTrackIndexToBeUsed", -1);

		string encodingProfileContentType = outputRoot.get("encodingProfileContentType", "Video")
			.asString();
		bool isVideo = encodingProfileContentType == "Video" ? true : false;

		if (ffmpegDrawTextFilter == "" && JSONUtils::isMetadataPresent(outputRoot, "drawTextDetails"))
		{
			string field = "drawTextDetails";
			Json::Value drawTextDetailsRoot = outputRoot[field];

			field = "text";
			if (!JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string text = drawTextDetailsRoot.get(field, "").asString();

			string textTemporaryFileName;
			{
				textTemporaryFileName =
					_ffmpegTempDir + "/"
					+ to_string(ingestionJobKey)
					+ "_"
					+ to_string(encodingJobKey)
					+ "_"
					+ to_string(outputIndex)
					+ ".overlayText";
				ofstream of(textTemporaryFileName, ofstream::trunc);
				of << text;
				of.flush();
			}

			int reloadAtFrameInterval = -1;
			field = "reloadAtFrameInterval";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				reloadAtFrameInterval = JSONUtils::asInt(drawTextDetailsRoot, field, -1);

			string textPosition_X_InPixel = "";
			field = "textPosition_X_InPixel";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				textPosition_X_InPixel = drawTextDetailsRoot.get(field, "").asString();

			string textPosition_Y_InPixel = "";
			field = "textPosition_Y_InPixel";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				textPosition_Y_InPixel = drawTextDetailsRoot.get(field, "").asString();

			string fontType = "";
			field = "fontType";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				fontType = drawTextDetailsRoot.get(field, "").asString();

			int fontSize = -1;
			field = "fontSize";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				fontSize = JSONUtils::asInt(drawTextDetailsRoot, field, -1);

			string fontColor = "";
			field = "fontColor";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				fontColor = drawTextDetailsRoot.get(field, "").asString();

			int textPercentageOpacity = -1;
			field = "textPercentageOpacity";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				textPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot, field, -1);

			int shadowx = 0;
			field = "shadowx";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				shadowx = JSONUtils::asInt(drawTextDetailsRoot, field, -1);

			int shadowy = 0;
			field = "shadowy";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				shadowy = JSONUtils::asInt(drawTextDetailsRoot, field, -1);

			bool boxEnable = false;
			field = "boxEnable";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				boxEnable = JSONUtils::asBool(drawTextDetailsRoot, field, false);

			string boxColor = "";
			field = "boxColor";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				boxColor = drawTextDetailsRoot.get(field, "").asString();

			int boxPercentageOpacity = -1;
			field = "boxPercentageOpacity";
			if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
				boxPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot, field, -1);

			ffmpegDrawTextFilter = getDrawTextVideoFilterDescription(ingestionJobKey,
				"", textTemporaryFileName, reloadAtFrameInterval,
				textPosition_X_InPixel, textPosition_Y_InPixel, fontType, fontSize,
				fontColor, textPercentageOpacity, shadowx, shadowy,
				boxEnable, boxColor, boxPercentageOpacity,
				streamingDurationInSeconds);
		}

		/*
		int fadeDuration = JSONUtils::asInt(outputRoot, "fadeDuration", -1);
		string ffmpegFadeFilter;
		if (fadeDuration > 0 && streamingDurationInSeconds >= fadeDuration)
		{
			// fade=type=in:duration=3,fade=type=out:duration=3:start_time=27
			ffmpegFadeFilter =
				string("fade=type=in:duration=") + to_string(fadeDuration)
				+ ",fade=type=out:duration=" + to_string(fadeDuration)
				+ ":start_time=" + to_string(streamingDurationInSeconds - fadeDuration);
		}
		*/

		string httpStreamingFileFormat;    
		string ffmpegHttpStreamingParameter = "";

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

		if (encodingProfileDetailsRoot != Json::nullValue)
		{
			try
			{
				FFMpegEncodingParameters::settingFfmpegParameters(
					_logger,
					encodingProfileDetailsRoot,
					isVideo,

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
					ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter)
					= videoBitRateInfo;

				ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

				/*
				if (httpStreamingFileFormat != "")
				{
					string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have an httpStreaming encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else */ if (twoPasses)
				{
					/*
					string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", twoPasses: " + to_string(twoPasses)
					;
					_logger->warn(errorMessage);
				}
			}
			catch(runtime_error e)
			{
				string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				throw e;
			}
		}

		// map output
		if (otherOutputOptions.find("-map") == string::npos
			&& otherOutputOptionsBecauseOfMaxWidth != "")
			FFMpegEncodingParameters::addToArguments(otherOutputOptions + otherOutputOptionsBecauseOfMaxWidth,
				ffmpegOutputArgumentList);
		else
			FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegOutputArgumentList);

		string ffmpegVideoFilter;

		if (encodingProfileDetailsRoot != Json::nullValue)
		{
			FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegOutputArgumentList);
			// ffmpegVideoResolutionParameter is -vf scale=w=1280:h=720
			// Since we cannot have more than one -vf (otherwise ffmpeg will use
			// only the last one), in case we have ffmpegDrawTextFilter,
			// we will append it here

			FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegOutputArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegOutputArgumentList);

			ffmpegOutputArgumentList.push_back("-threads");
			ffmpegOutputArgumentList.push_back("0");

			tuple<string, string, string> allFilters = addFilters(
				filtersRoot, ffmpegVideoResolutionParameter,
				ffmpegDrawTextFilter, streamingDurationInSeconds);

			string videoFilters;
			string audioFilters;
			string complexFilters;
			tie(videoFilters, audioFilters, complexFilters) = allFilters;


			if (videoFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter:v");
				ffmpegOutputArgumentList.push_back(videoFilters);
			}
			if (audioFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter:a");
				ffmpegOutputArgumentList.push_back(audioFilters);
			}
		}
		else
		{
			tuple<string, string, string> allFilters = addFilters(
				filtersRoot, ffmpegVideoResolutionParameter,
				ffmpegDrawTextFilter, streamingDurationInSeconds);

			string videoFilters;
			string audioFilters;
			string complexFilters;
			tie(videoFilters, audioFilters, complexFilters) = allFilters;


			if (videoFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter:v");
				ffmpegOutputArgumentList.push_back(videoFilters);
			}
			else if (otherOutputOptions.find("-filter:v") == string::npos)
			{
				// it is not possible to have -c:v copy and -filter:v toghether
				ffmpegOutputArgumentList.push_back("-c:v");
				ffmpegOutputArgumentList.push_back("copy");
			}

			if (audioFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter:a");
				ffmpegOutputArgumentList.push_back(audioFilters);
			}
			else if (otherOutputOptions.find("-filter:a") == string::npos)
			{
				// it is not possible to have -c:a copy and -filter:a toghether
				ffmpegOutputArgumentList.push_back("-c:a");
				ffmpegOutputArgumentList.push_back("copy");
			}
		}

		// output file
		if (outputType == "HLS" || outputType == "DASH")
		{
			string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "")
				.asString();
			string manifestFileName = outputRoot.get("manifestFileName", "").asString();
			int segmentDurationInSeconds = JSONUtils::asInt(outputRoot,
				"segmentDurationInSeconds", 10);
			int playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);

			string manifestFilePathName = manifestDirectoryPath + "/" + manifestFileName;

			_logger->info(__FILEREF__ + "Checking manifestDirectoryPath directory"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", manifestDirectoryPath: " + manifestDirectoryPath
			);

			// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
			// I saw just once that the directory was not created and the liveencoder remains in the loop
			// where:
			//	1. the encoder returns an error because of the missing directory
			//	2. EncoderVideoAudioProxy calls again the encoder
			// So, for this reason, the below check is done
			if (!FileIO::directoryExisting(manifestDirectoryPath))
			{
				_logger->warn(__FILEREF__ + "manifestDirectoryPath does not exist!!! It will be created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", manifestDirectoryPath: " + manifestDirectoryPath
				);

				_logger->info(__FILEREF__ + "Create directory"
					+ ", manifestDirectoryPath: " + manifestDirectoryPath
				);
				bool noErrorIfExists = true;
				bool recursive = true;
				FileIO::createDirectory(manifestDirectoryPath,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP |
					S_IROTH | S_IXOTH, noErrorIfExists, recursive);
			}

			if (externalEncoder)
				addToIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

			if (outputType == "HLS")
			{
				ffmpegOutputArgumentList.push_back("-hls_flags");
				ffmpegOutputArgumentList.push_back("append_list");
				ffmpegOutputArgumentList.push_back("-hls_time");
				ffmpegOutputArgumentList.push_back(to_string(segmentDurationInSeconds));
				ffmpegOutputArgumentList.push_back("-hls_list_size");
				ffmpegOutputArgumentList.push_back(to_string(playlistEntriesNumber));

				// Segment files removed from the playlist are deleted after a period of time
				// equal to the duration of the segment plus the duration of the playlist
				ffmpegOutputArgumentList.push_back("-hls_flags");
				ffmpegOutputArgumentList.push_back("delete_segments");

				// Set the number of unreferenced segments to keep on disk
				// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
				// to download segments which were recently referenced in the playlist.
				// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
				ffmpegOutputArgumentList.push_back("-hls_delete_threshold");
				ffmpegOutputArgumentList.push_back(to_string(1));


				// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
				// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
				// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
				// 2020-07-19: commented, if it is needed just test it
				// ffmpegArgumentList.push_back("-hls_start_number_source");
				// ffmpegArgumentList.push_back("datetime");

				// 2020-07-19: commented, if it is needed just test it
				// ffmpegArgumentList.push_back("-start_number");
				// ffmpegArgumentList.push_back(to_string(10));
			}
			else if (outputType == "DASH")
			{
				ffmpegOutputArgumentList.push_back("-seg_duration");
				ffmpegOutputArgumentList.push_back(to_string(segmentDurationInSeconds));
				ffmpegOutputArgumentList.push_back("-window_size");
				ffmpegOutputArgumentList.push_back(to_string(playlistEntriesNumber));

				// it is important to specify -init_seg_name because those files
				// will not be removed in EncoderVideoAudioProxy.cpp
				ffmpegOutputArgumentList.push_back("-init_seg_name");
				ffmpegOutputArgumentList.push_back("init-stream$RepresentationID$.$ext$");

				// the only difference with the ffmpeg default is that default is $Number%05d$
				// We had to change it to $Number%01d$ because otherwise the generated file containing
				// 00001 00002 ... but the videojs player generates file name like 1 2 ...
				// and the streaming was not working
				ffmpegOutputArgumentList.push_back("-media_seg_name");
				ffmpegOutputArgumentList.push_back("chunk-stream$RepresentationID$-$Number%01d$.$ext$");
			}
			ffmpegOutputArgumentList.push_back(manifestFilePathName);
		}
		else if (outputType == "RTMP_Stream" || outputType == "AWS_CHANNEL")
		{
			string rtmpUrl = outputRoot.get("rtmpUrl", "").asString();
			string rtmpStreamName = outputRoot.get("rtmpStreamName", "").asString();
			string rtmpUserName = outputRoot.get("rtmpUserName", "").asString();
			string rtmpPassword = outputRoot.get("rtmpPassword", "").asString();

			if (rtmpUrl == "")
			{
				string errorMessage = __FILEREF__ + "rtmpUrl cannot be empty"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", rtmpUrl: " + rtmpUrl
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (rtmpStreamName != "")
				rtmpUrl += ("/" + rtmpStreamName);
			if (rtmpUserName != "" && rtmpPassword != "")
			{
				// rtmp://.....
				rtmpUrl.insert(7, (rtmpUserName + ":" + rtmpPassword + "@"));
			}

			ffmpegOutputArgumentList.push_back("-bsf:a");
			ffmpegOutputArgumentList.push_back("aac_adtstoasc");
			// 2020-08-13: commented bacause -c:v copy is already present
			// ffmpegArgumentList.push_back("-vcodec");
			// ffmpegArgumentList.push_back("copy");

			// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
			ffmpegOutputArgumentList.push_back("-f");
			ffmpegOutputArgumentList.push_back("flv");
			ffmpegOutputArgumentList.push_back(rtmpUrl);
		}
		else if (outputType == "UDP_Stream")
		{
			string udpUrl = outputRoot.get("udpUrl", "").asString();

			if (udpUrl == "")
			{
				string errorMessage = __FILEREF__ + "udpUrl cannot be empty"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", udpUrl: " + udpUrl
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			ffmpegOutputArgumentList.push_back("-f");
			ffmpegOutputArgumentList.push_back("mpegts");
			ffmpegOutputArgumentList.push_back(udpUrl);
		}
		else
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong output type"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", outputType: " + outputType;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
}

/*
void FFMpeg::vodProxy(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,

	string vodContentType,
	string sourcePhysicalPathName,

	string otherInputOptions,

	bool timePeriod,
	time_t utcProxyPeriodStart,
	time_t utcProxyPeriodEnd,

	// array, each element is an output containing the following fields
	//  string outputType (it could be: HLS, DASH, RTMP_Stream)
	//  #in case of HLS or DASH
	//		string otherOutputOptions
	//		string audioVolumeChange
	//      Json::Value encodingProfileDetailsRoot,
	//      string encodingProfileContentType
	//      int segmentDurationInSeconds,
	//      int playlistEntriesNumber,
	//      string manifestDirectoryPath,
	//      string manifestFileName,
	//  #in case of RTMP_Stream
	//		string otherOutputOptions
	//		string audioVolumeChange
	//      Json::Value encodingProfileDetailsRoot,
	//      string encodingProfileContentType
	//      string rtmpUrl,
	//
	vector<tuple<string, string, string, Json::Value, string, string, int, int, bool, string, string>>& outputRoots,

	pid_t* pChildPid)
{
	_currentApiName = "vodProxy";

	_logger->info(__FILEREF__ + "Received " + _currentApiName
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", sourcePhysicalPathName: " + sourcePhysicalPathName
		+ ", otherInputOptions: " + otherInputOptions
		+ ", timePeriod: " + to_string(timePeriod)
		+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
		+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
	);

	setStatus(
		ingestionJobKey,
		encodingJobKey
	);


	time_t utcNow;

	if (timePeriod)
	{
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		if (utcNow < utcProxyPeriodStart)
		{
			while (utcNow < utcProxyPeriodStart)
			{
				time_t sleepTime = utcProxyPeriodStart - utcNow;

				_logger->info(__FILEREF__ + "VODProxy timing. "
						+ "Too early to start the VODProxy, just sleep "
					+ to_string(sleepTime) + " seconds"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
					);

				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}
			}
		}
		else if (utcProxyPeriodEnd <= utcNow)
        {
			time_t tooLateTime = utcNow - utcProxyPeriodEnd;

            string errorMessage = __FILEREF__ + "VODProxy timing. "
				+ "Too late to start the VODProxy"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
                    + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
                    + ", tooLateTime: " + to_string(tooLateTime)
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
		else
		{
			time_t delayTime = utcNow - utcProxyPeriodStart;

            string errorMessage = __FILEREF__ + "VODProxy timing. "
				+ "We are a bit late to start the VODProxy, let's start it"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
                    + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
                    + ", delayTime: " + to_string(delayTime)
            ;

            _logger->warn(errorMessage);
		}
	}
	else
	{
		chrono::system_clock::time_point now = chrono::system_clock::now();
		utcNow = chrono::system_clock::to_time_t(now);
	}

	// Creating multi outputs: https://trac.ffmpeg.org/wiki/Creating%20multiple%20outputs
	vector<string> ffmpegArgumentList;
	{
		time_t streamingDuration = 0;

		if (timePeriod)
		{
			streamingDuration = utcProxyPeriodEnd - utcNow;

			_logger->info(__FILEREF__ + "VODProxy timing. "
				+ "Streaming duration"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", utcNow: " + to_string(utcNow)
				+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
				+ ", streamingDuration: " + to_string(streamingDuration)
			);
		}

		// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

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
		//	-nostdin: Disabling interaction on standard input, it is useful, for example, if ffmpeg is
		//		in the background process group
		ffmpegArgumentList.push_back("-nostdin");

		ffmpegArgumentList.push_back("-re");
		FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegArgumentList);

		if (vodContentType == "Image")
		{
			ffmpegArgumentList.push_back("-r");
			ffmpegArgumentList.push_back("25");

			ffmpegArgumentList.push_back("-loop");
			ffmpegArgumentList.push_back("1");
		}
		else
		{
			ffmpegArgumentList.push_back("-stream_loop");
			ffmpegArgumentList.push_back("-1");
		}

		ffmpegArgumentList.push_back("-i");
		ffmpegArgumentList.push_back(sourcePhysicalPathName);

		if (timePeriod)
		{
			ffmpegArgumentList.push_back("-t");
			ffmpegArgumentList.push_back(to_string(streamingDuration));
		}
	}

	if (outputRoots.size() == 0)
	{
		string errorMessage = __FILEREF__ + "vodProxy. No output parameters"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", outputRoots.size: " + to_string(outputRoots.size())
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	for (tuple<string, string, string, Json::Value, string, string, int, int,
		bool, string, string> tOutputRoot: outputRoots)
	{
		string outputType;
		string otherOutputOptions;
		string audioVolumeChange;
		Json::Value encodingProfileDetailsRoot;
		string manifestDirectoryPath;
		string manifestFileName;
		int segmentDurationInSeconds;
		int playlistEntriesNumber;
		bool isVideo;
		string rtmpUrl;
		string udpUrl;

		tie(outputType, otherOutputOptions, audioVolumeChange,
			encodingProfileDetailsRoot, manifestDirectoryPath,       
			manifestFileName, segmentDurationInSeconds, playlistEntriesNumber, isVideo, rtmpUrl, udpUrl)
			= tOutputRoot;

		if (outputType == "HLS" || outputType == "DASH")
		{
			if (audioVolumeChange != "")
			{
				ffmpegArgumentList.push_back("-filter:a");
				ffmpegArgumentList.push_back(string("volume=") + audioVolumeChange);
			}

			vector<string> ffmpegEncodingProfileArgumentList;
			if (encodingProfileDetailsRoot != Json::nullValue)
			{
				try
				{
					string httpStreamingFileFormat;    
					string ffmpegHttpStreamingParameter = "";

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


					settingFfmpegParameters(
						encodingProfileDetailsRoot,
						isVideo,

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
						ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter)
						= videoBitRateInfo;

					ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

					if (twoPasses)
					{
						twoPasses = false;

						string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", twoPasses: " + to_string(twoPasses)
						;
						_logger->warn(errorMessage);
					}

					if (vodContentType == "Video" || vodContentType == "Image")
					{
						FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter,
							ffmpegEncodingProfileArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegVideoBitRateParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegVideoOtherParameters,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegVideoMaxRateParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegVideoBufSizeParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegVideoFrameRateParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegVideoKeyFramesRateParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments("-vf " + ffmpegVideoResolutionParameter,
							ffmpegEncodingProfileArgumentList);
					}
					ffmpegEncodingProfileArgumentList.push_back("-threads");
					ffmpegEncodingProfileArgumentList.push_back("0");
					if (vodContentType == "Video" || vodContentType == "Audio")
					{
						addToArguments(ffmpegAudioCodecParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegAudioBitRateParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegAudioOtherParameters,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegAudioChannelsParameter,
							ffmpegEncodingProfileArgumentList);
						addToArguments(ffmpegAudioSampleRateParameter,
							ffmpegEncodingProfileArgumentList);
					}
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					throw e;
				}
			}

			{
				string manifestFilePathName = manifestDirectoryPath + "/" + manifestFileName;

				_logger->info(__FILEREF__ + "Checking manifestDirectoryPath directory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", manifestDirectoryPath: " + manifestDirectoryPath
				);

				// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
				// I saw just once that the directory was not created and the liveencoder remains in the loop
				// where:
				//	1. the encoder returns an error because of the missing directory
				//	2. EncoderVideoAudioProxy calls again the encoder
				// So, for this reason, the below check is done
				if (!FileIO::directoryExisting(manifestDirectoryPath))
				{
					_logger->warn(__FILEREF__ + "manifestDirectoryPath does not exist!!! It will be created"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
					);

					_logger->info(__FILEREF__ + "Create directory"
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
					);
					bool noErrorIfExists = true;
					bool recursive = true;
					FileIO::createDirectory(manifestDirectoryPath,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP |
						S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}

				addToArguments(otherOutputOptions, ffmpegArgumentList);

				if (ffmpegEncodingProfileArgumentList.size() > 0)
				{
					for (string parameter: ffmpegEncodingProfileArgumentList)
						addToArguments(parameter, ffmpegArgumentList);
				}
				else
				{
					if (otherOutputOptions.find("-filter:v") == string::npos)
					{
						// it is not possible to have -c:v copy and -filter:v toghether
						ffmpegArgumentList.push_back("-c:v");
						ffmpegArgumentList.push_back("copy");
					}
					if (otherOutputOptions.find("-filter:a") == string::npos)
					{
						// it is not possible to have -c:a copy and -filter:a toghether
						ffmpegArgumentList.push_back("-c:a");
						ffmpegArgumentList.push_back("copy");
					}
				}
				if (outputType == "HLS")
				{
					ffmpegArgumentList.push_back("-hls_flags");
					ffmpegArgumentList.push_back("append_list");
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


					// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
					// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
					// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
					// 2020-07-19: commented, if it is needed just test it
					// ffmpegArgumentList.push_back("-hls_start_number_source");
					// ffmpegArgumentList.push_back("datetime");

					// 2020-07-19: commented, if it is needed just test it
					// ffmpegArgumentList.push_back("-start_number");
					// ffmpegArgumentList.push_back(to_string(10));
				}
				else if (outputType == "DASH")
				{
					ffmpegArgumentList.push_back("-seg_duration");
					ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
					ffmpegArgumentList.push_back("-window_size");
					ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));

					// it is important to specify -init_seg_name because those files
					// will not be removed in EncoderVideoAudioProxy.cpp
					ffmpegArgumentList.push_back("-init_seg_name");
					ffmpegArgumentList.push_back("init-stream$RepresentationID$.$ext$");

					// the only difference with the ffmpeg default is that default is $Number%05d$
					// We had to change it to $Number%01d$ because otherwise the generated file containing
					// 00001 00002 ... but the videojs player generates file name like 1 2 ...
					// and the streaming was not working
					ffmpegArgumentList.push_back("-media_seg_name");
					ffmpegArgumentList.push_back("chunk-stream$RepresentationID$-$Number%01d$.$ext$");
				}
				ffmpegArgumentList.push_back(manifestFilePathName);
			}
		}
		else if (outputType == "RTMP_Stream")
		{
			if (rtmpUrl == "")
			{
				string errorMessage = __FILEREF__ + "rtmpUrl cannot be empty"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", rtmpUrl: " + rtmpUrl
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (audioVolumeChange != "")
			{
				ffmpegArgumentList.push_back("-filter:a");
				ffmpegArgumentList.push_back(string("volume=") + audioVolumeChange);
			}

			vector<string> ffmpegEncodingProfileArgumentList;
			if (encodingProfileDetailsRoot != Json::nullValue)
			{
				try
				{
					string httpStreamingFileFormat;    
					string ffmpegHttpStreamingParameter = "";

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


					settingFfmpegParameters(
						encodingProfileDetailsRoot,
						isVideo,

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
						twoPasses = false;

						string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", twoPasses: " + to_string(twoPasses)
						;
						_logger->warn(errorMessage);
					}

					addToArguments(ffmpegVideoCodecParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoProfileParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoBitRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoOtherParameters, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoMaxRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoBufSizeParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoFrameRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments("-vf " + ffmpegVideoResolutionParameter,
						ffmpegEncodingProfileArgumentList);
					ffmpegEncodingProfileArgumentList.push_back("-threads");
					ffmpegEncodingProfileArgumentList.push_back("0");
					addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					throw e;
				}
			}

			addToArguments(otherOutputOptions, ffmpegArgumentList);

			if (ffmpegEncodingProfileArgumentList.size() > 0)
			{
				for (string parameter: ffmpegEncodingProfileArgumentList)
					addToArguments(parameter, ffmpegArgumentList);
			}
			else
			{
				if (otherOutputOptions.find("-filter:v") == string::npos)
				{
					// it is not possible to have -c:v copy and -filter:v toghether
					ffmpegArgumentList.push_back("-c:v");
					ffmpegArgumentList.push_back("copy");
				}
				if (otherOutputOptions.find("-filter:a") == string::npos)
				{
					// it is not possible to have -c:a copy and -filter:a toghether
					ffmpegArgumentList.push_back("-c:a");
					ffmpegArgumentList.push_back("copy");
				}
			}
			ffmpegArgumentList.push_back("-bsf:a");
			ffmpegArgumentList.push_back("aac_adtstoasc");
			// 2020-08-13: commented bacause -c:v copy is already present
			// ffmpegArgumentList.push_back("-vcodec");
			// ffmpegArgumentList.push_back("copy");

			// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("flv");
			ffmpegArgumentList.push_back(rtmpUrl);
		}
		else if (outputType == "UDP_Stream")
		{
			if (udpUrl == "")
			{
				string errorMessage = __FILEREF__ + "udpUrl cannot be empty"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", udpUrl: " + udpUrl
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (audioVolumeChange != "")
			{
				ffmpegArgumentList.push_back("-filter:a");
				ffmpegArgumentList.push_back(string("volume=") + audioVolumeChange);
			}

			vector<string> ffmpegEncodingProfileArgumentList;
			if (encodingProfileDetailsRoot != Json::nullValue)
			{
				try
				{
					string httpStreamingFileFormat;    
					string ffmpegHttpStreamingParameter = "";

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


					settingFfmpegParameters(
						encodingProfileDetailsRoot,
						isVideo,

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
						twoPasses = false;

						string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", twoPasses: " + to_string(twoPasses)
						;
						_logger->warn(errorMessage);
					}

					addToArguments(ffmpegVideoCodecParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoProfileParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoBitRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoOtherParameters, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoMaxRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoBufSizeParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoFrameRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments("-vf " + ffmpegVideoResolutionParameter,
						ffmpegEncodingProfileArgumentList);
					ffmpegEncodingProfileArgumentList.push_back("-threads");
					ffmpegEncodingProfileArgumentList.push_back("0");
					addToArguments(ffmpegAudioCodecParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioBitRateParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioOtherParameters, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioChannelsParameter, ffmpegEncodingProfileArgumentList);
					addToArguments(ffmpegAudioSampleRateParameter, ffmpegEncodingProfileArgumentList);
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					throw e;
				}
			}

			addToArguments(otherOutputOptions, ffmpegArgumentList);

			if (ffmpegEncodingProfileArgumentList.size() > 0)
			{
				for (string parameter: ffmpegEncodingProfileArgumentList)
					addToArguments(parameter, ffmpegArgumentList);
			}
			else
			{
				if (otherOutputOptions.find("-filter:v") == string::npos)
				{
					// it is not possible to have -c:v copy and -filter:v toghether
					ffmpegArgumentList.push_back("-c:v");
					ffmpegArgumentList.push_back("copy");
				}
				if (otherOutputOptions.find("-filter:a") == string::npos)
				{
					// it is not possible to have -c:a copy and -filter:a toghether
					ffmpegArgumentList.push_back("-c:a");
					ffmpegArgumentList.push_back("copy");
				}
			}
			// ffmpegArgumentList.push_back("-bsf:a");
			// ffmpegArgumentList.push_back("aac_adtstoasc");
			// 2020-08-13: commented bacause -c:v copy is already present
			// ffmpegArgumentList.push_back("-vcodec");
			// ffmpegArgumentList.push_back("copy");

			// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
			ffmpegArgumentList.push_back("-f");
			ffmpegArgumentList.push_back("mpegts");
			ffmpegArgumentList.push_back(udpUrl);
		}
		else
		{
			string errorMessage = __FILEREF__ + "vodProxy. Wrong output type"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", outputType: " + outputType;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;

    try
    {
		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey) + "_"
			+ to_string(encodingJobKey)
			+ ".vodProxy.log"
		;

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

		_logger->info(__FILEREF__ + "vodProxy: Executing ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
		);

		startFfmpegCommand = chrono::system_clock::now();

		bool redirectionStdOutput = true;
		bool redirectionStdError = true;

		ProcessUtility::forkAndExec (
			_ffmpegPath + "/ffmpeg",
			ffmpegArgumentList,
			_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
			pChildPid, &iReturnedStatus);
		if (iReturnedStatus != 0)
		{
			string errorMessage = __FILEREF__ + "vodProxy: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
           ;
           _logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "vodProxy: command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
           ;
			throw runtime_error(errorMessage);
		}
        
		endFfmpegCommand = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "vodProxy: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
			+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);

		for (tuple<string, string, string, Json::Value, string, string, int, int,
			bool, string, string> tOutputRoot: outputRoots)
		{
			string outputType;
			// string otherOutputOptions;
			// string audioVolumeChange;
			// Json::Value encodingProfileDetailsRoot;
			string manifestDirectoryPath;
			// string manifestFileName;
			// int segmentDurationInSeconds;
			// int playlistEntriesNumber;
			// bool isVideo;
			// string rtmpUrl;

			tie(outputType, ignore, ignore, ignore, manifestDirectoryPath,       
				ignore, ignore, ignore, ignore, ignore, ignore)
				= tOutputRoot;

			if (outputType == "HLS" || outputType == "DASH")
			{
				if (manifestDirectoryPath != "")
				{
					if (FileIO::directoryExisting(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							Boolean_t bRemoveRecursively = true;
							FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
    	}

		if (timePeriod && endFfmpegCommand - startFfmpegCommand <
			chrono::seconds(utcProxyPeriodEnd - utcNow - 60))
		{
			throw runtime_error("vodProxy exit before unexpectly");
		}
    }
    catch(runtime_error e)
    {
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(
			_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9)	// 9 means: SIGKILL
		{
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		}
		else
		{
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		}
        _logger->error(errorMessage);

		// copy ffmpeg log file
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
				+ ".vodProxy.log.debug"
			;

			_logger->info(__FILEREF__ + "Coping"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", debugOutputFfmpegPathFileName: " + debugOutputFfmpegPathFileName
				);
			FileIO::copyFile(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);    
		}

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

		for (tuple<string, string, string, Json::Value, string, string, int, int,
			bool, string, string> tOutputRoot: outputRoots)
		{
			string outputType;
			// string otherOutputOptions;
			// string audioVolumeChange;
			// Json::Value encodingProfileDetailsRoot;
			string manifestDirectoryPath;
			// string manifestFileName;
			// int segmentDurationInSeconds;
			// int playlistEntriesNumber;
			// bool isVideo;
			// string rtmpUrl;

			tie(outputType, ignore, ignore, ignore, manifestDirectoryPath,       
				ignore, ignore, ignore, ignore, ignore, ignore)
				= tOutputRoot;

			if (outputType == "HLS" || outputType == "DASH")
			{
				if (manifestDirectoryPath != "")
				{
					if (FileIO::directoryExisting(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							Boolean_t bRemoveRecursively = true;
							FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
    	}

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else
			throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}
*/


void FFMpeg::liveGrid(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	Json::Value encodingProfileDetailsRoot,
	string userAgent,
	Json::Value inputChannelsRoot,	// name,url
	int gridColumns,
	int gridWidth,	// i.e.: 1024
	int gridHeight, // i.e.: 578

	string outputType,	// HLS or SRT (DASH not implemented yet)

	// next are parameters for the hls output
	int segmentDurationInSeconds,
	int playlistEntriesNumber,
	string manifestDirectoryPath,
	string manifestFileName,

	// next are parameters for the srt output
	string srtURL,

	pid_t* pChildPid)
{
	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	string segmentListPath;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;
	time_t utcNow;

	_currentApiName = "liveGrid";

	setStatus(
		ingestionJobKey,
		encodingJobKey
		/*
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	string outputTypeLowerCase;
    try
    {
		_logger->info(__FILEREF__ + "Received " + _currentApiName
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		);

		outputTypeLowerCase.resize(outputType.size());
		transform(outputType.begin(), outputType.end(), outputTypeLowerCase.begin(),
				[](unsigned char c){return tolower(c); } );

		if (outputTypeLowerCase != "hls" && outputTypeLowerCase != "srt")
		{
			string errorMessage = __FILEREF__
				+ "liveProxy. Wrong output type (it has to be HLS or DASH)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", outputType: " + outputType;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
		// I saw just once that the directory was not created and the liveencoder remains in the loop
		// where:
		//	1. the encoder returns an error because of the missing directory
		//	2. EncoderVideoAudioProxy calls again the encoder
		// So, for this reason, the below check is done
		if (outputTypeLowerCase == "hls" && !FileIO::directoryExisting(manifestDirectoryPath))
		{
			_logger->warn(__FILEREF__ + "manifestDirectoryPath does not exist!!! It will be created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", manifestDirectoryPath: " + manifestDirectoryPath
					);

			_logger->info(__FILEREF__ + "Create directory"
                + ", manifestDirectoryPath: " + manifestDirectoryPath
            );
			bool noErrorIfExists = true;
			bool recursive = true;
			FileIO::createDirectory(manifestDirectoryPath,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH, noErrorIfExists, recursive);
		}

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey)
				+ "_"
				+ to_string(encodingJobKey)
				+ "_"
				+ sUtcTimestamp
				+ ".liveGrid.log";
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
				 " -map "[0r+1r]" -codec:v libx264 -b:v 800k -preset veryfast -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments -hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/low/test_%04d.ts -f hls /var/catramms/storage/MMSRepository-free/1/test/low/test.m3u8 \
				-map 0:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments -hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/tv1/test_%04d.ts -f hls /var/catramms/storage/MMSRepository-free/1/test/tv1/test.m3u8 \
				-map 1:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments -hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/tv2/test_%04d.ts -f hls /var/catramms/storage/MMSRepository-free/1/test/tv2/test.m3u8 \
				-map 2:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments -hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/tv3/test_%04d.ts -f hls /var/catramms/storage/MMSRepository-free/1/test/tv3/test.m3u8 \
				-map 3:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 -hls_delete_threshold 1 -hls_flags delete_segments -hls_start_number_source datetime -start_number 10 -hls_segment_filename /var/catramms/storage/MMSRepository-free/1/test/tv4/test_%04d.ts -f hls /var/catramms/storage/MMSRepository-free/1/test/tv4/test.m3u8

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
			Json::Value inputChannelRoot = inputChannelsRoot[inputChannelIndex];
			string inputChannelURL = inputChannelRoot.get("inputChannelURL", "").asString();

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

				ffmpegFilterComplex += (
					"[" + to_string(inputChannelIndex) + ":v]"
					+ "scale=width=" + to_string(width) + ":height=" + to_string(height)
					+ "[" + to_string(inputChannelIndex) + "v];"
					);
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
				for(int gridColumnIndex = 0; gridColumnIndex < columnsIntoTheRow; gridColumnIndex++)
					ffmpegFilterComplex += ("[" + to_string(inputChannelIndex++) + "v]");

				ffmpegFilterComplex += (
					"hstack=inputs=" + to_string(columnsIntoTheRow) + ":shortest=1"
					);

				if (gridRows == 1 && gridRowIndex == 0)
				{
					// in case there is just one row, vstack has NOT to be added 
					ffmpegFilterComplex += (
						"[outVideo]"
					);
				}
				else
				{
					ffmpegFilterComplex += (
						"[" + to_string(gridRowIndex) + "r];"
					);
				}
			}

			if (gridRows > 1)
			{
				// [0r][1r]               vstack=inputs=2:shortest=1              [outVideo]
				for (int gridRowIndex = 0, inputChannelIndex = 0; gridRowIndex < gridRows; gridRowIndex++)
					ffmpegFilterComplex += ("[" + to_string(gridRowIndex) + "r]");
				ffmpegFilterComplex += (
					"vstack=inputs=" + to_string(gridRows) + ":shortest=1[outVideo]"
				);
			}

			ffmpegArgumentList.push_back("-filter_complex");
			ffmpegArgumentList.push_back(ffmpegFilterComplex);
		}

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
					/*
					 * non so come si deve gestire nel caso di multi audio con DASH
					*/
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
						/*
						 * non so come si deve gestire nel caso di multi audio con DASH
						 */
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

		// We will create:
		//  - one m3u8 for each track (video and audio)
		//  - one main m3u8 having a group for AUDIO
		if (outputTypeLowerCase == "hls")
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

			{
				for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsNumber; inputChannelIndex++)
				{
					string audioTrackDirectoryName = to_string(inputChannelIndex) + "_audio";

					string audioPathName = manifestDirectoryPath + "/"
						+ audioTrackDirectoryName;

					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory (if needed)"
						+ ", audioPathName: " + audioPathName
					);
					FileIO::createDirectory(audioPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}

				{
					string videoTrackDirectoryName = "0_video";
					string videoPathName = manifestDirectoryPath + "/" + videoTrackDirectoryName;

					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory (if needed)"
						+ ", videoPathName: " + videoPathName
					);
					FileIO::createDirectory(videoPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}
			}

			// create main manifest file
			{
				string mainManifestPathName = manifestDirectoryPath + "/"
					+ manifestFileName;

				string mainManifest;

				mainManifest = string("#EXTM3U") + "\n";

				for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsNumber; inputChannelIndex++)
				{
					string audioTrackDirectoryName = to_string(inputChannelIndex) + "_audio";

					Json::Value inputChannelRoot = inputChannelsRoot[inputChannelIndex];
					string inputChannelName = inputChannelRoot.get("inputConfigurationLabel", "").asString();

					string audioManifestLine = "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",LANGUAGE=\""
						+ inputChannelName + "\",NAME=\"" + inputChannelName + "\",AUTOSELECT=YES, DEFAULT=YES,URI=\""
						+ audioTrackDirectoryName + "/" + manifestFileName + "\"";

					mainManifest += (audioManifestLine + "\n");
				}

				string videoManifestLine = "#EXT-X-STREAM-INF:PROGRAM-ID=1";
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
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

		_logger->info(__FILEREF__ + "liveGrid: Executing ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
		);

		startFfmpegCommand = chrono::system_clock::now();

		bool redirectionStdOutput = true;
		bool redirectionStdError = true;

		ProcessUtility::forkAndExec (
			_ffmpegPath + "/ffmpeg",
			ffmpegArgumentList,
			_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
			pChildPid, &iReturnedStatus);
		if (iReturnedStatus != 0)
		{
			string errorMessage = __FILEREF__ + "liveGrid: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
           ;
           _logger->error(errorMessage);

			// to hide the ffmpeg staff
			errorMessage = __FILEREF__ + "liveGrid: command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
			;
			throw runtime_error(errorMessage);
		}
        
		endFfmpegCommand = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "liveGrid: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
			+ ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
		);
    }
    catch(runtime_error e)
    {
		string lastPartOfFfmpegOutputFile = getLastPartOfFile(
			_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		string errorMessage;
		if (iReturnedStatus == 9)	// 9 means: SIGKILL
		{
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		}
		else
		{
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;

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
				FileIO::copyFile(_outputFfmpegPathFileName, debugOutputFfmpegPathFileName);    
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
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
		*/

		if (outputTypeLowerCase == "hls" && manifestDirectoryPath != "")
    	{
			try
			{
				_logger->info(__FILEREF__ + "Remove directory"
					+ ", manifestDirectoryPath: " + manifestDirectoryPath);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
			}
			catch(runtime_error e)
			{
				string errorMessage = __FILEREF__ + "remove directory failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				// throw e;
			}
			catch(exception e)
			{
				string errorMessage = __FILEREF__ + "remove directory failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				// throw e;
			}
		}

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
			throw FFMpegURLForbidden();
		else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
			throw FFMpegURLNotFound();
		else
			throw e;
    }

	/*
    _logger->info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
	bool exceptionInCaseOfError = false;
	FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
	*/
}

// destinationPathName will end with the new file format
void FFMpeg::changeFileFormat(
	int64_t ingestionJobKey,
	int64_t physicalPathKey,
	string sourcePhysicalPath,
	vector<tuple<int64_t, int, int64_t, int, int, string, string, long,
		string>>& sourceVideoTracks,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& sourceAudioTracks,

	string destinationPathName,
	string outputFileFormat)
{
	string ffmpegExecuteCommand;

	_currentApiName = "changeFileFormat";

	setStatus(
		ingestionJobKey
		/*
		encodingJobKey
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

    try
    {
		if (!FileIO::fileExisting(sourcePhysicalPath)        
			&& !FileIO::directoryExisting(sourcePhysicalPath)
		)
		{
			string errorMessage = string("Source asset path name not existing")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				// + ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", sourcePhysicalPath: " + sourcePhysicalPath
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey)
				+ "_"
				+ to_string(physicalPathKey)
				+ "_"
				+ sUtcTimestamp
				+ ".changeFileFormat.log";
		}

		/*
		if (outputFileFormat == "m3u8-tar.gz" || outputFileFormat == "m3u8-streaming")
		{
			ffmpegExecuteCommand = 
				_ffmpegPath + "/ffmpeg "
				+ "-i " + sourcePhysicalPath + " "
			;
			for(tuple<int64_t, int, int64_t, int, int, string, string, long,
				string> videoTrack: videoTracks)
			{
				int64_t videoTrackKey;
				int trackIndex;
				int64_t durationInMilliSeconds;
				int width;
				int height;
				string avgFrameRate;
				string codecName;
				long bitRate;
				string profile;

				tie(videoTrackKey, trackIndex, durationInMilliSeconds, width, height,
					avgFrameRate, codecName, bitRate, profile) = videoTrack;

				ffmpegExecuteCommand +=
					"0:v:" + to_string(trackIndex) -c:v copy 
					  -hls_time 10 -hls_playlist_type vod  -hls_segment_filename beach/360p_%03d.ts beach/360p.m3u8 \

			}
				+ "-map 0:v -c:v copy -map 0:a -c:a copy "
				//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
				+ "-q 0 "
				+ destinationPathName + " "
				+ "> " + _outputFfmpegPathFileName + " "
				+ "2>&1"
			;
		}
		else
		*/
		{
			ffmpegExecuteCommand = 
				_ffmpegPath + "/ffmpeg "
				+ "-i " + sourcePhysicalPath + " "
				// -map 0:v and -map 0:a is to get all video-audio tracks
				+ "-map 0:v -c:v copy -map 0:a -c:a copy "
				//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
				+ "-q 0 "
				+ destinationPathName + " "
				+ "> " + _outputFfmpegPathFileName + " "
				+ "2>&1"
			;
		}

		#ifdef __APPLE__
			ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=")
					+ getenv("DYLD_LIBRARY_PATH") + "; ");
		#endif

        _logger->info(__FILEREF__ + "changeFileFormat: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "changeFileFormat: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            _logger->error(errorMessage);

			// to hide the ffmpeg staff
            errorMessage = __FILEREF__ + "changeFileFormat: command failed"
            ;
            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "changeContainer: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        _logger->info(__FILEREF__ + "Remove"
            + ", destinationPathName: " + destinationPathName);
        FileIO::remove(destinationPathName, exceptionInCaseOfError);

        throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

void FFMpeg::streamingToFile(
	int64_t ingestionJobKey,
	bool regenerateTimestamps,
	string sourceReferenceURL,
	string destinationPathName)
{
	string ffmpegExecuteCommand;

	_currentApiName = "streamingToFile";

	setStatus(
		ingestionJobKey
		/*
		encodingJobKey
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

    try
    {
		{
			char	sUtcTimestamp [64];
			tm		tmUtcTimestamp;
			time_t	utcTimestamp = chrono::system_clock::to_time_t(
				chrono::system_clock::now());

			localtime_r (&utcTimestamp, &tmUtcTimestamp);
			sprintf (sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d",
				tmUtcTimestamp.tm_year + 1900,
				tmUtcTimestamp.tm_mon + 1,
				tmUtcTimestamp.tm_mday,
				tmUtcTimestamp.tm_hour,
				tmUtcTimestamp.tm_min,
				tmUtcTimestamp.tm_sec);

			_outputFfmpegPathFileName =
				_ffmpegTempDir + "/"
				+ to_string(ingestionJobKey)
				+ "_"
				+ sUtcTimestamp
				+ ".streamingToFile.log";
		}

		ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg ";
		if (regenerateTimestamps)
			ffmpegExecuteCommand += "-fflags +genpts ";
// 2022-06-06: cosa succede se sourceReferenceURL rappresenta un live?
//		- destinationPathName diventerà enorme!!!
//	Per questo motivo ho inserito un timeout di X hours
		int maxStreamingHours = 5;
		ffmpegExecuteCommand +=
			string("-y -i \"" + sourceReferenceURL + "\" ")
			+ "-t " + to_string(maxStreamingHours * 3600) + " "
			// -map 0:v and -map 0:a is to get all video-audio tracks
            + "-map 0:v -c:v copy -map 0:a -c:a copy "
			//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
			+ "-q 0 "
			+ destinationPathName + " "
			+ "> " + _outputFfmpegPathFileName + " "
			+ "2>&1"
		;

		#ifdef __APPLE__
			ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=")
					+ getenv("DYLD_LIBRARY_PATH") + "; ");
		#endif

        _logger->info(__FILEREF__ + "streamingToFile: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "streamingToFile: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            _logger->error(errorMessage);

			// 2021-12-13: sometimes we have one track creating problems and the command fails
			// in this case it is enought to avoid to copy all the tracks, leave ffmpeg
			// to choice the track and it works
			{
				ffmpegExecuteCommand = _ffmpegPath + "/ffmpeg ";
				if (regenerateTimestamps)
					ffmpegExecuteCommand += "-fflags +genpts ";
				ffmpegExecuteCommand +=
					string("-y -i \"" + sourceReferenceURL + "\" ")
					+ "-c:v copy -c:a copy "
					//  -q: 0 is best Quality, 2 is normal, 9 is strongest compression
					+ "-q 0 "
					+ destinationPathName + " "
					+ "> " + _outputFfmpegPathFileName + " "
					+ "2>&1"
				;

				_logger->info(__FILEREF__ + "streamingToFile: Executing ffmpeg command"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
				);

				chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
				if (executeCommandStatus != 0)
				{
					string errorMessage = __FILEREF__ + "streamingToFile: ffmpeg command failed"
						+ ", executeCommandStatus: " + to_string(executeCommandStatus)
						+ ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
					;
					_logger->error(errorMessage);

					// to hide the ffmpeg staff
					errorMessage = __FILEREF__ + "streamingToFile: command failed"
					;
					throw runtime_error(errorMessage);
				}
			}
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "streamingToFile: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        _logger->info(__FILEREF__ + "Remove"
            + ", destinationPathName: " + destinationPathName);
        FileIO::remove(destinationPathName, exceptionInCaseOfError);

        throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

int FFMpeg::getEncodingProgress()
{
    int encodingPercentage = 0;

    try
    {
		if (
				_currentApiName == "liveProxyByHTTPStreaming"
				|| _currentApiName == "liveProxyByCDN"
				|| _currentApiName == "liveGridByHTTPStreaming"
				|| _currentApiName == "liveGridByCDN"
				)
		{
			// it's a live

			return -1;
		}

        if (!FileIO::isFileExisting(_outputFfmpegPathFileName.c_str()))
        {
            _logger->info(__FILEREF__ + "ffmpeg: Encoding progress not available"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

        string ffmpegEncodingStatus;
        try
        {
			int lastCharsToBeReadToGetInfo = 10000;
            
            ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "ffmpeg: Failure reading the encoding progress file"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

        {
            // frame= 2315 fps= 98 q=27.0 q=28.0 size=    6144kB time=00:01:32.35 bitrate= 545.0kbits/s speed=3.93x    
            
            smatch m;   // typedef std:match_result<string>

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
                string duration = m[1].str();   // 00:01:47.87

                stringstream ss(duration);
                string hours;
                string minutes;
                string seconds;
                string roughMicroSeconds;    // microseconds???
                char delim = ':';

                getline(ss, hours, delim); 
                getline(ss, minutes, delim); 

                delim = '.';
                getline(ss, seconds, delim); 
                getline(ss, roughMicroSeconds, delim); 

                int iHours = atoi(hours.c_str());
                int iMinutes = atoi(minutes.c_str());
                int iSeconds = atoi(seconds.c_str());
                int iRoughMicroSeconds = atoi(roughMicroSeconds.c_str());

                double encodingSeconds = (iHours * 3600) + (iMinutes * 60) + (iSeconds) + (iRoughMicroSeconds / 100);
                double currentTimeInMilliSeconds = (encodingSeconds * 1000) + (_currentlyAtSecondPass ? _currentDurationInMilliSeconds : 0);
                //  encodingSeconds : _encodingItem->videoOrAudioDurationInMilliSeconds = x : 100
                
                encodingPercentage = 100 * currentTimeInMilliSeconds / (_currentDurationInMilliSeconds * (_twoPasses ? 2 : 1));

				if (encodingPercentage > 100 || encodingPercentage < 0)
				{
					_logger->error(__FILEREF__ + "Encoding progress too big"
						+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
						+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
						+ ", duration: " + duration
						+ ", encodingSeconds: " + to_string(encodingSeconds)
						+ ", _twoPasses: " + to_string(_twoPasses)
						+ ", _currentlyAtSecondPass: " + to_string(_currentlyAtSecondPass)
						+ ", currentTimeInMilliSeconds: " + to_string(currentTimeInMilliSeconds)
						+ ", _currentDurationInMilliSeconds: " + to_string(_currentDurationInMilliSeconds)
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
						+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
					);

					encodingPercentage		= 0;
				}
				else
				{
					_logger->info(__FILEREF__ + "Encoding progress"
						+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
						+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
						+ ", duration: " + duration
						+ ", encodingSeconds: " + to_string(encodingSeconds)
						+ ", _twoPasses: " + to_string(_twoPasses)
						+ ", _currentlyAtSecondPass: " + to_string(_currentlyAtSecondPass)
						+ ", currentTimeInMilliSeconds: " + to_string(currentTimeInMilliSeconds)
						+ ", _currentDurationInMilliSeconds: " + to_string(_currentDurationInMilliSeconds)
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
						+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
					);
				}
            }
        }
    }
    catch(FFMpegEncodingStatusNotAvailable e)
    {
        _logger->warn(__FILEREF__ + "ffmpeg: getEncodingProgress failed"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        throw FFMpegEncodingStatusNotAvailable();
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: getEncodingProgress failed"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
        );

        throw e;
    }
    
    return encodingPercentage;
}

bool FFMpeg::nonMonotonousDTSInOutputLog()
{
    try
    {
		if (_currentApiName != "liveProxyByCDN")
		{
			// actually we need this check just for liveProxyByCDN

			return false;
		}

        if (!FileIO::isFileExisting(_outputFfmpegPathFileName.c_str()))
        {
            _logger->warn(__FILEREF__ + "ffmpeg: Encoding status not available"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

        string ffmpegEncodingStatus;
        try
        {
			int lastCharsToBeReadToGetInfo = 10000;
            
            ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "ffmpeg: Failure reading the encoding status file"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

		string lowerCaseFfmpegEncodingStatus;
		lowerCaseFfmpegEncodingStatus.resize(ffmpegEncodingStatus.size());
		transform(ffmpegEncodingStatus.begin(), ffmpegEncodingStatus.end(), lowerCaseFfmpegEncodingStatus.begin(), [](unsigned char c){return tolower(c); } );

		// [flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
		if (lowerCaseFfmpegEncodingStatus.find("non-monotonous dts in output stream") != string::npos
				&& lowerCaseFfmpegEncodingStatus.find("incorrect timestamps") != string::npos)
			return true;
		else
			return false;
    }
    catch(FFMpegEncodingStatusNotAvailable e)
    {
        _logger->warn(__FILEREF__ + "ffmpeg: nonMonotonousDTSInOutputLog failed"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        throw FFMpegEncodingStatusNotAvailable();
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: nonMonotonousDTSInOutputLog failed"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
        );

        throw e;
    }
}

bool FFMpeg::forbiddenErrorInOutputLog()
{
    try
    {
		if (_currentApiName != "liveProxyByCDN")
		{
			// actually we need this check just for liveProxyByCDN

			return false;
		}

        if (!FileIO::isFileExisting(_outputFfmpegPathFileName.c_str()))
        {
            _logger->warn(__FILEREF__ + "ffmpeg: Encoding status not available"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

        string ffmpegEncodingStatus;
        try
        {
			int lastCharsToBeReadToGetInfo = 10000;
            
            ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "ffmpeg: Failure reading the encoding status file"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

		string lowerCaseFfmpegEncodingStatus;
		lowerCaseFfmpegEncodingStatus.resize(ffmpegEncodingStatus.size());
		transform(ffmpegEncodingStatus.begin(), ffmpegEncodingStatus.end(),
			lowerCaseFfmpegEncodingStatus.begin(),
			[](unsigned char c)
			{
				return tolower(c);
			}
		);

		// [https @ 0x555a8e428a00] HTTP error 403 Forbidden
		if (lowerCaseFfmpegEncodingStatus.find("http error 403 forbidden") != string::npos)
			return true;
		else
			return false;
    }
    catch(FFMpegEncodingStatusNotAvailable e)
    {
        _logger->warn(__FILEREF__ + "ffmpeg: forbiddenErrorInOutputLog failed"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        throw FFMpegEncodingStatusNotAvailable();
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: forbiddenErrorInOutputLog failed"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
        );

        throw e;
    }
}

bool FFMpeg::isFrameIncreasing(int maxMilliSecondsToWait)
{

	bool frameIncreasing = true;

	chrono::system_clock::time_point startCheck = chrono::system_clock::now();
    try
    {
		long minutesSinceBeginningPassed =
			chrono::duration_cast<chrono::minutes>(startCheck - _startFFMpegMethod).count();
		if (minutesSinceBeginningPassed <= _startCheckingFrameInfoInMinutes)
        {
            _logger->info(__FILEREF__ + "isFrameIncreasing: too early to check frame increasing"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
				+ ", minutesSinceBeginningPassed: " + to_string(minutesSinceBeginningPassed)
				+ ", _startCheckingFrameInfoInMinutes: " + to_string(_startCheckingFrameInfoInMinutes)
				+ ", isFrameIncreasing elapsed (millisecs): " + to_string(
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now()
					- startCheck).count())
            );

			return frameIncreasing;
		}

        if (!FileIO::isFileExisting(_outputFfmpegPathFileName.c_str()))
        {
            _logger->info(__FILEREF__ + "isFrameIncreasing: Encoding status not available"
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
				+ ", minutesSinceBeginningPassed: " + to_string(minutesSinceBeginningPassed)
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

		int lastCharsToBeReadToGetInfo = 10000;

		long firstFramesValue;
		{
			string ffmpegEncodingStatus;
			try
			{
				ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeReadToGetInfo);
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "isFrameIncreasing: Failure reading the encoding status file"
					+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
					+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
					+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
					+ ", minutesSinceBeginningPassed: " + to_string(minutesSinceBeginningPassed)
				);

				throw FFMpegEncodingStatusNotAvailable();
			}

			try
			{
				firstFramesValue = getFrameByOutputLog(ffmpegEncodingStatus);
			}
			catch(FFMpegFrameInfoNotAvailable e)
			{
				frameIncreasing = false;

				_logger->error(__FILEREF__ + "isFrameIncreasing: frame monitoring. Frame info not available (1)"
					+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
					+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
					+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
					+ ", minutesSinceBeginningPassed: " + to_string(minutesSinceBeginningPassed)
					+ ", frameIncreasing: " + to_string(frameIncreasing)
					+ ", isFrameIncreasing elapsed (millisecs): " + to_string(
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now()
						- startCheck).count())
				);

				return frameIncreasing;
				// throw FFMpegEncodingStatusNotAvailable();
			}
		}

		int millisecondsToWaitAmongChecks = 300;
		int numberOfChecksDone = 0;
		long secondFramesValue = firstFramesValue;

		while(chrono::duration_cast<chrono::milliseconds>(
			chrono::system_clock::now() - startCheck).count() < maxMilliSecondsToWait)
		{
			this_thread::sleep_for(chrono::milliseconds(millisecondsToWaitAmongChecks));
			numberOfChecksDone++;

			{
				string ffmpegEncodingStatus;
				try
				{
					ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName,
						lastCharsToBeReadToGetInfo);
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__
						+ "isFrameIncreasing: Failure reading the encoding status file"
						+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
						+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
						+ ", _currentStagingEncodedAssetPathName: "
							+ _currentStagingEncodedAssetPathName
						+ ", minutesSinceBeginningPassed: "
							+ to_string(minutesSinceBeginningPassed)
					);

					throw FFMpegEncodingStatusNotAvailable();
				}

				try
				{
					secondFramesValue = getFrameByOutputLog(ffmpegEncodingStatus);
				}
				catch(FFMpegFrameInfoNotAvailable e)
				{
					frameIncreasing = false;

					_logger->error(__FILEREF__
						+ "isFrameIncreasing. Frame info not available (2)"
						+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
						+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
						+ ", _currentStagingEncodedAssetPathName: "
							+ _currentStagingEncodedAssetPathName
						+ ", minutesSinceBeginningPassed: "
							+ to_string(minutesSinceBeginningPassed)
						+ ", frameIncreasing: " + to_string(frameIncreasing)
						+ ", isFrameIncreasing elapsed (millisecs): " + to_string(
							chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startCheck).count())
					);

					return frameIncreasing;
					// throw FFMpegEncodingStatusNotAvailable();
				}
			}

			if (firstFramesValue != secondFramesValue)
				break;
		}

		frameIncreasing = (firstFramesValue == secondFramesValue ? false : true);

        _logger->info(__FILEREF__ + "isFrameIncreasing"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            + ", firstFramesValue: " + to_string(firstFramesValue)
            + ", secondFramesValue: " + to_string(secondFramesValue)
			+ ", minutesSinceBeginningPassed: " + to_string(minutesSinceBeginningPassed)
            + ", frameIncreasing: " + to_string(frameIncreasing)
            + ", numberOfChecksDone: " + to_string(numberOfChecksDone)
			+ ", isFrameIncreasing elapsed (millisecs): " + to_string(
				chrono::duration_cast<chrono::milliseconds>(
				chrono::system_clock::now() - startCheck).count())
        );
    }
    catch(FFMpegEncodingStatusNotAvailable e)
    {
        _logger->info(__FILEREF__ + "isFrameIncreasing failed"
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			+ ", isFrameIncreasing elapsed (millisecs): " + to_string(
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now()
				- startCheck).count())
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "isFrameIncreasing failed" + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			+ ", isFrameIncreasing elapsed (millisecs): " + to_string(
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now()
				- startCheck).count())
        );

        throw e;
    }

    return frameIncreasing;
}

long FFMpeg::getFrameByOutputLog(string ffmpegEncodingStatus)
{
	// frame= 2315 fps= 98 q=27.0 q=28.0 size=    6144kB time=00:01:32.35 bitrate= 545.0kbits/s speed=3.93x    

	string frameToSearch = "frame=";
	size_t startFrameIndex = ffmpegEncodingStatus.rfind(frameToSearch);
	if (startFrameIndex == string::npos)
	{
		_logger->warn(__FILEREF__ + "ffmpeg: frame info was not found"
			+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
			+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
			+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
			+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			+ ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
		);

		throw FFMpegFrameInfoNotAvailable();
	}
	ffmpegEncodingStatus = ffmpegEncodingStatus.substr(startFrameIndex + frameToSearch.size());
	size_t endFrameIndex = ffmpegEncodingStatus.find(" fps=");
	if (endFrameIndex == string::npos)
	{
		_logger->error(__FILEREF__ + "ffmpeg: fps info was not found"
			+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
			+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
			+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
			+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
			+ ", ffmpegEncodingStatus: " + ffmpegEncodingStatus
		);

		throw FFMpegEncodingStatusNotAvailable();
	}
	ffmpegEncodingStatus = ffmpegEncodingStatus.substr(0, endFrameIndex);
	ffmpegEncodingStatus = StringUtils::trim(ffmpegEncodingStatus);

	return stol(ffmpegEncodingStatus);
}

string FFMpeg::getLastPartOfFile(
    string pathFileName, int lastCharsToBeRead)
{
    string lastPartOfFile = "";
    char* buffer = nullptr;

    auto logger = spdlog::get("mmsEngineService");

    try
    {
        ifstream ifPathFileName(pathFileName);
        if (ifPathFileName) 
        {
            int         charsToBeRead;
            
            // get length of file:
            ifPathFileName.seekg (0, ifPathFileName.end);
            int fileSize = ifPathFileName.tellg();
            if (fileSize >= lastCharsToBeRead)
            {
                ifPathFileName.seekg (fileSize - lastCharsToBeRead, ifPathFileName.beg);
                charsToBeRead = lastCharsToBeRead;
            }
            else
            {
                ifPathFileName.seekg (0, ifPathFileName.beg);
                charsToBeRead = fileSize;
            }

            buffer = new char [charsToBeRead];
            ifPathFileName.read (buffer, charsToBeRead);
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
    catch(exception e)
    {
        if (buffer != nullptr)
            delete [] buffer;

        logger->error("getLastPartOfFile failed");        
    }

    return lastPartOfFile;
}

pair<string, string> FFMpeg::retrieveStreamingYouTubeURL(
	int64_t ingestionJobKey, string youTubeURL)
{
	_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL"
		+ ", youTubeURL: " + youTubeURL
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
	);

	string detailsYouTubeProfilesPath;
	{
		detailsYouTubeProfilesPath =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "-youTubeProfiles.txt";
    
		string youTubeExecuteCommand =
			_pythonPathName + " " + _youTubeDlPath + "/youtube-dl "
			+ "--list-formats "
			+ youTubeURL + " "
			+ " > " + detailsYouTubeProfilesPath
			+ " 2>&1"
		;

		try
		{
			_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL: Executing youtube command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
			);

			chrono::system_clock::time_point startYouTubeCommand = chrono::system_clock::now();

			int executeCommandStatus = ProcessUtility::execute(youTubeExecuteCommand);
			if (executeCommandStatus != 0)
			{
				// it could be also that the live is not available
				// ERROR: f2vW_XyTW4o: YouTube said: This live stream recording is not available.

				string lastPartOfFfmpegOutputFile;
				if (FileIO::fileExisting(detailsYouTubeProfilesPath))
					lastPartOfFfmpegOutputFile = getLastPartOfFile(
						detailsYouTubeProfilesPath, _charsToBeReadFromFfmpegErrorOutput);
				else
					lastPartOfFfmpegOutputFile = string("file not found: ") + detailsYouTubeProfilesPath;

				string errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: youTube command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", executeCommandStatus: " + to_string(executeCommandStatus)
					+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
					+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				throw runtime_error(errorMessage);
			}
			else if (!FileIO::fileExisting(detailsYouTubeProfilesPath))
			{
				string errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: youTube command failed. no profiles file created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", executeCommandStatus: " + to_string(executeCommandStatus)
					+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: command failed. no profiles file created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				throw runtime_error(errorMessage);
			}

			chrono::system_clock::time_point endYouTubeCommand = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL: Executed youTube command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
				+ ", detailsYouTubeProfilesPath size: " + to_string(FileIO::getFileSizeInBytes(detailsYouTubeProfilesPath, false))
				+ ", @FFMPEG statistics@ - duration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endYouTubeCommand - startYouTubeCommand).count()) + "@"
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL, youTube command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", e.what(): " + e.what()
			;
			_logger->error(errorMessage);

			if (FileIO::fileExisting(detailsYouTubeProfilesPath))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath);
				bool exceptionInCaseOfError = false;
				FileIO::remove(detailsYouTubeProfilesPath, exceptionInCaseOfError);
			}

			throw e;
		}
		catch(exception e)
		{
			string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL, youTube command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", e.what(): " + e.what()
			;
			_logger->error(errorMessage);

			if (FileIO::fileExisting(detailsYouTubeProfilesPath))
			{
				_logger->info(__FILEREF__ + "Remove"
				+ ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath);
				bool exceptionInCaseOfError = false;
				FileIO::remove(detailsYouTubeProfilesPath, exceptionInCaseOfError);
			}

			throw e;
		}
	}

	int selectedFormatCode = -1;
	string extension;
    try
    {
        // txt output will be like:
        /*
[youtube] f2vW_XyTW4o: Downloading webpage
[youtube] f2vW_XyTW4o: Downloading m3u8 information
[youtube] f2vW_XyTW4o: Downloading MPD manifest
[info] Available formats for f2vW_XyTW4o:
format code  extension  resolution note
91           mp4        256x144    HLS  197k , avc1.42c00b, 30.0fps, mp4a.40.5@ 48k
92           mp4        426x240    HLS  338k , avc1.4d4015, 30.0fps, mp4a.40.5@ 48k
93           mp4        640x360    HLS  829k , avc1.4d401e, 30.0fps, mp4a.40.2@128k
94           mp4        854x480    HLS 1380k , avc1.4d401f, 30.0fps, mp4a.40.2@128k
95           mp4        1280x720   HLS 2593k , avc1.4d401f, 30.0fps, mp4a.40.2@256k (best)
        */

        ifstream detailsFile(detailsYouTubeProfilesPath);
		string line;
		bool formatCodeLabelFound = false;
		int lastFormatCode = -1;
		int bestFormatCode = -1;
		while(getline(detailsFile, line))
		{
			_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL, Details youTube profiles"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath
				+ ", formatCodeLabelFound: " + to_string(formatCodeLabelFound)
				+ ", lastFormatCode: " + to_string(lastFormatCode)
				+ ", bestFormatCode: " + to_string(bestFormatCode)
				+ ", line: " + line
			);

			if (formatCodeLabelFound)
			{
				int lastDigit = 0;
				while(lastDigit < line.length() && isdigit(line[lastDigit]))
					lastDigit++;
				if (lastDigit > 0)
				{
					string formatCode = line.substr(0, lastDigit);
					lastFormatCode = stoi(formatCode);

					if (line.find("(best)") != string::npos)
						bestFormatCode = lastFormatCode;

					int startExtensionIndex = lastDigit;
					while(startExtensionIndex < line.length()
						&& isspace(line[startExtensionIndex]))
						startExtensionIndex++;
					int endExtensionIndex = startExtensionIndex;
					while(endExtensionIndex < line.length()
						&& !isspace(line[endExtensionIndex]))
						endExtensionIndex++;

					extension = line.substr(startExtensionIndex,
						endExtensionIndex - startExtensionIndex);
				}
			}
			else if (line.find("format code") != string::npos)
				formatCodeLabelFound = true;
		}

		_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL, Details youTube profiles, final info"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath
			+ ", formatCodeLabelFound: " + to_string(formatCodeLabelFound)
			+ ", lastFormatCode: " + to_string(lastFormatCode)
			+ ", bestFormatCode: " + to_string(bestFormatCode)
			+ ", line: " + line
		);

		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath);
			bool exceptionInCaseOfError = false;
			FileIO::remove(detailsYouTubeProfilesPath, exceptionInCaseOfError);
		}

		if (bestFormatCode != -1)
			selectedFormatCode = bestFormatCode;
		else if (lastFormatCode != -1)
			selectedFormatCode = lastFormatCode;
		else
		{
			string errorMessage = __FILEREF__
				+ "retrieveStreamingYouTubeURL: no format code found"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
    catch(runtime_error e)
    {
        string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL: profile error processing or format code not found"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

		if (FileIO::fileExisting(detailsYouTubeProfilesPath))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath);
			bool exceptionInCaseOfError = false;
			FileIO::remove(detailsYouTubeProfilesPath, exceptionInCaseOfError);
		}

        throw e;
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL: profiles error processing"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

		if (FileIO::fileExisting(detailsYouTubeProfilesPath))
		{
			_logger->info(__FILEREF__ + "Remove"
            + ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath);
			bool exceptionInCaseOfError = false;
			FileIO::remove(detailsYouTubeProfilesPath, exceptionInCaseOfError);
		}

        throw e;
	}

	string streamingYouTubeURL;
	{
		string detailsYouTubeURLPath =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "-youTubeUrl.txt";

		string youTubeExecuteCommand =
			_pythonPathName + " " + _youTubeDlPath + "/youtube-dl "
			+ "-f " + to_string(selectedFormatCode) + " "
			+ "-g " + youTubeURL + " "
			+ " > " + detailsYouTubeURLPath
			+ " 2>&1"
		;

		try
		{
			_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL: Executing youtube command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
			);

			chrono::system_clock::time_point startYouTubeCommand = chrono::system_clock::now();

			int executeCommandStatus = ProcessUtility::execute(youTubeExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: youTube command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", executeCommandStatus: " + to_string(executeCommandStatus)
					+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				throw runtime_error(errorMessage);
			}
			else if (!FileIO::fileExisting(detailsYouTubeURLPath))
			{
				string errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: youTube command failed. no URL file created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", executeCommandStatus: " + to_string(executeCommandStatus)
					+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
				;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__
					+ "retrieveStreamingYouTubeURL: command failed. no URL file created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				throw runtime_error(errorMessage);
			}

			chrono::system_clock::time_point endYouTubeCommand = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL: Executed youTube command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
				+ ", @FFMPEG statistics@ - duration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endYouTubeCommand - startYouTubeCommand).count()) + "@"
			);

			{
				ifstream urlFile(detailsYouTubeURLPath);
				std::stringstream buffer;
				buffer << urlFile.rdbuf();

				streamingYouTubeURL = buffer.str();
				streamingYouTubeURL = StringUtils::trimNewLineToo(streamingYouTubeURL);

				_logger->info(__FILEREF__ + "retrieveStreamingYouTubeURL: Executed youTube command"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", youTubeExecuteCommand: " + youTubeExecuteCommand
					+ ", streamingYouTubeURL: " + streamingYouTubeURL
				);
			}

			{
				_logger->info(__FILEREF__ + "Remove"
				+ ", detailsYouTubeURLPath: " + detailsYouTubeURLPath);
				bool exceptionInCaseOfError = false;
				FileIO::remove(detailsYouTubeProfilesPath, exceptionInCaseOfError);
			}
		}
		catch(runtime_error e)
		{
			string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL, youTube command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", e.what(): " + e.what()
			;
			_logger->error(errorMessage);

			if (FileIO::fileExisting(detailsYouTubeURLPath))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", detailsYouTubeURLPath: " + detailsYouTubeURLPath);
				bool exceptionInCaseOfError = false;
				FileIO::remove(detailsYouTubeURLPath, exceptionInCaseOfError);
			}

			throw e;
		}
		catch(exception e)
		{
			string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL, youTube command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", e.what(): " + e.what()
			;
			_logger->error(errorMessage);

			if (FileIO::fileExisting(detailsYouTubeURLPath))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", detailsYouTubeURLPath: " + detailsYouTubeURLPath);
				bool exceptionInCaseOfError = false;
				FileIO::remove(detailsYouTubeProfilesPath, exceptionInCaseOfError);
			}

			throw e;
		}
	}

	return make_pair(streamingYouTubeURL, extension);
}

void FFMpeg::encodingVideoCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger)
{    
    if (codec != "libx264" 
            && codec != "libvpx"
            && codec != "rawvideo"
			)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: Video codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpeg::setStatus(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	int64_t durationInMilliSeconds,
	string mmsSourceAssetPathName,
	string stagingEncodedAssetPathName
)
{

    _currentIngestionJobKey             = ingestionJobKey;	// just for log
    _currentEncodingJobKey              = encodingJobKey;	// just for log
    _currentDurationInMilliSeconds      = durationInMilliSeconds;	// in case of some functionalities, it is important for getEncodingProgress
    _currentMMSSourceAssetPathName      = mmsSourceAssetPathName;	// just for log
    _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;	// just for log

	_startFFMpegMethod = chrono::system_clock::now();
}

tuple<string, string, string> FFMpeg::addFilters(
	Json::Value filtersRoot,
	string ffmpegVideoResolutionParameter,
	string ffmpegDrawTextFilter,
	int64_t streamingDurationInSeconds
)
{
	string videoFilters;
	string audioFilters;
	string complexFilters;


	if (ffmpegVideoResolutionParameter != "")
	{
		if (videoFilters != "")
			videoFilters += ",";
		videoFilters += ffmpegVideoResolutionParameter;
	}
	if (ffmpegDrawTextFilter != "")
	{
		if (videoFilters != "")
			videoFilters += ",";
		videoFilters += ffmpegDrawTextFilter;
	}

	if (filtersRoot != Json::nullValue)
	{
		if (JSONUtils::isMetadataPresent(filtersRoot, "video"))
		{
			for (int filterIndex = 0; filterIndex < filtersRoot["video"].size(); filterIndex++)
			{
				Json::Value filterRoot = filtersRoot["video"][filterIndex];

				string filter = getFilter(filterRoot, streamingDurationInSeconds);
				if (videoFilters != "")
					videoFilters += ",";
				videoFilters += filter;
			}
		}

		if (JSONUtils::isMetadataPresent(filtersRoot, "audio"))
		{
			for (int filterIndex = 0; filterIndex < filtersRoot["audio"].size(); filterIndex++)
			{
				Json::Value filterRoot = filtersRoot["audio"][filterIndex];

				string filter = getFilter(filterRoot, streamingDurationInSeconds);
				if (audioFilters != "")
					audioFilters += ",";
				audioFilters += filter;
			}
		}

		if (JSONUtils::isMetadataPresent(filtersRoot, "complex"))
		{
			for (int filterIndex = 0; filterIndex < filtersRoot["complex"].size(); filterIndex++)
			{
				Json::Value filterRoot = filtersRoot["complex"][filterIndex];

				string filter = getFilter(filterRoot, streamingDurationInSeconds);
				if (complexFilters != "")
					complexFilters += ",";
				complexFilters += filter;
			}
		}
	}

	return make_tuple(videoFilters, audioFilters, complexFilters);
}

string FFMpeg::getFilter(
	Json::Value filterRoot,
	int64_t streamingDurationInSeconds
)
{
	string filter;


	if (!JSONUtils::isMetadataPresent(filterRoot, "type"))
	{
		string errorMessage = string("filterRoot->type field does not exist");
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	string type = filterRoot.get("type", "").asString();

	if (type == "blackdetect")
	{
		double black_min_duration = JSONUtils::asDouble(filterRoot, "black_min_duration", 2);
		double pixel_black_th = JSONUtils::asDouble(filterRoot, "pixel_black_th", 0.0);

		filter = ("blackdetect=d=" + to_string(black_min_duration)
			+ ":pix_th=" + to_string(pixel_black_th));
	}
	else if (type == "blackframe")
	{
		int amount = JSONUtils::asInt(filterRoot, "amount", 98);
		int threshold = JSONUtils::asInt(filterRoot, "threshold", 32);

		filter = ("blackframe=amount=" + to_string(amount)
			+ ":threshold=" + to_string(threshold));
	}
	else if (type == "freezedetect")
	{
		int noiseInDb = JSONUtils::asInt(filterRoot, "noiseInDb", -60);
		int duration = JSONUtils::asInt(filterRoot, "duration", 2);

		filter = ("freezedetect=noise=" + to_string(noiseInDb)
			+ "dB:duration=" + to_string(duration));
	}
	else if (type == "fade")
	{
		int duration = JSONUtils::asInt(filterRoot, "duration", 4);

		if (streamingDurationInSeconds >= duration)
		{
			// fade=type=in:duration=3,fade=type=out:duration=3:start_time=27
			filter = ("fade=type=in:duration=" + to_string(duration)
				+ ",fade=type=out:duration=" + to_string(duration)
				+ ":start_time=" + to_string(streamingDurationInSeconds - duration)
			);
		}
		else
		{
			_logger->warn(__FILEREF__ + "fade filter, streaming duration to small"
				+ ", fadeDuration: " + to_string(duration)
				+ ", streamingDurationInSeconds: "
					+ to_string(streamingDurationInSeconds)
			);
		}
	}
	else if (type == "showinfo")
	{
		filter = ("showinfo");
	}
	else if (type == "metadata")
	{
		filter = ("metadata=mode=print");
	}
	else if (type == "silencedetect")
	{
		double noise = JSONUtils::asDouble(filterRoot, "noise", 0.0001);

		filter = ("silencedetect=noise=" + to_string(noise));
	}
	else if (type == "volume")
	{
		double factor = JSONUtils::asDouble(filterRoot, "factor", 5.0);

		filter = ("volume=" + to_string(factor));
	}
	else if (type == "ashowinfo")
	{
		filter = ("ashowinfo");
	}
	else if (type == "ametadata")
	{
		filter = ("ametadata=mode=print");
	}
	else
	{
		string errorMessage = string("filterRoot->type is unknown")
			+ ", type: " + type
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	return filter;
}

void FFMpeg::addToIncrontab(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string directoryToBeMonitored
)
{
	try
	{
		_logger->info(__FILEREF__ + "Received addToIncrontab"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", directoryToBeMonitored: " + directoryToBeMonitored
		);

		if (!FileIO::directoryExisting(_incrontabConfigurationDirectory))
		{
			_logger->info(__FILEREF__ + "addToIncrontab: create directory"
				+ ", _ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(encodingJobKey)
				+ ", _incrontabConfigurationDirectory: " + _incrontabConfigurationDirectory
			);

			bool noErrorIfExists = true;
			bool recursive = true;
			FileIO::createDirectory(
				_incrontabConfigurationDirectory,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IWUSR | S_IXGRP |
				S_IROTH | S_IWUSR | S_IXOTH,
				noErrorIfExists, recursive);
		}

		string incrontabConfigurationPathName =
			_incrontabConfigurationDirectory
			+ "/" + _incrontabConfigurationFileName
		;

		bool directoryAlreadyMonitored = false;
		{
			ifstream ifConfigurationFile (incrontabConfigurationPathName);
			if (ifConfigurationFile)
			{
				string configuration;
				while(getline(ifConfigurationFile, configuration))
				{
					string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

					if (configuration.size() >= directoryToBeMonitored.size()
						&& 0 == configuration.compare(0, directoryToBeMonitored.size(),
						directoryToBeMonitored))
					{
						directoryAlreadyMonitored = true;

						break;
					}
				}

				ifConfigurationFile.close();
			}
		}

		if (directoryAlreadyMonitored)
		{
			string errorMessage = __FILEREF__
				+ "addToIncrontab: directory is already into the incontab configuration file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", incrontabConfigurationPathName: " + incrontabConfigurationPathName
			;
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}
		else
		{
			ofstream ofConfigurationFile (incrontabConfigurationPathName, ofstream::app);
			if (!ofConfigurationFile)
			{
				string errorMessage = __FILEREF__
					+ "addToIncrontab: open incontab configuration file failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", incrontabConfigurationPathName: " + incrontabConfigurationPathName
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			string configuration = directoryToBeMonitored
				+ " IN_MODIFY,IN_CLOSE_WRITE,IN_CREATE,IN_DELETE,IN_MOVED_FROM,IN_MOVED_TO,IN_MOVE_SELF /opt/catramms/CatraMMS/scripts/incrontab.sh $% $@ $#";

			_logger->info(__FILEREF__ + "addToIncrontab: adding incontab configuration"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", configuration: " + configuration
			);

			ofConfigurationFile << configuration << endl;
			ofConfigurationFile.close();
		}

		{
			string incrontabExecuteCommand = _incrontabBinary + " "
				+ incrontabConfigurationPathName;

			_logger->info(__FILEREF__ + "addToIncrontab: Executing incontab command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", incrontabExecuteCommand: " + incrontabExecuteCommand
			);

			int executeCommandStatus = ProcessUtility::execute(incrontabExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = __FILEREF__
					+ "addToIncrontab: incrontab command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", executeCommandStatus: " + to_string(executeCommandStatus)
					+ ", incrontabExecuteCommand: " + incrontabExecuteCommand
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
        }
	}
	catch (...)
	{
		string errorMessage = __FILEREF__ + "addToIncrontab failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);
	}
}

void FFMpeg::removeFromIncrontab(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string directoryToBeMonitored
)
{
	try
	{
		_logger->info(__FILEREF__ + "Received removeFromIncrontab"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", directoryToBeMonitored: " + directoryToBeMonitored
		);

		string incrontabConfigurationPathName =
			_incrontabConfigurationDirectory
			+ "/" + _incrontabConfigurationFileName
		;

		bool foundMonitoryDirectory = false;
		vector<string> vConfiguration;
		{
			ifstream ifConfigurationFile (incrontabConfigurationPathName);
			if (!ifConfigurationFile)
			{
				string errorMessage = __FILEREF__
					+ "removeFromIncrontab: open incontab configuration file failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", incrontabConfigurationPathName: " + incrontabConfigurationPathName
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			string configuration;
			while(getline(ifConfigurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				if (configuration.size() >= directoryToBeMonitored.size()
					&& 0 == configuration.compare(0, directoryToBeMonitored.size(),
					directoryToBeMonitored))
				{
					_logger->info(__FILEREF__ + "removeFromIncrontab: removing incontab configuration"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", configuration: " + configuration
					);

					foundMonitoryDirectory = true;
				}
				else
				{
					vConfiguration.push_back(trimmedConfiguration);
				}
			}
		}

		if (!foundMonitoryDirectory)
		{
			string errorMessage = __FILEREF__
				+ "removeFromIncrontab: monitoring directory is not found into the incontab configuration file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", incrontabConfigurationPathName: " + incrontabConfigurationPathName
			;
			_logger->warn(errorMessage);
		}
		else
		{
			ofstream ofConfigurationFile (incrontabConfigurationPathName, ofstream::trunc);
			if (!ofConfigurationFile)
			{
				string errorMessage = __FILEREF__
					+ "removeFromIncrontab: open incontab configuration file failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", incrontabConfigurationPathName: " + incrontabConfigurationPathName
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			for (string configuration: vConfiguration)
				ofConfigurationFile << configuration << endl;
			ofConfigurationFile.close();
		}

		{
			string incrontabExecuteCommand = _incrontabBinary + " "
				+ incrontabConfigurationPathName;

			_logger->info(__FILEREF__ + "removeFromIncrontab: Executing incontab command"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", incrontabExecuteCommand: " + incrontabExecuteCommand
			);

			int executeCommandStatus = ProcessUtility::execute(incrontabExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage = __FILEREF__
					+ "removeFromIncrontab: incrontab command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", executeCommandStatus: " + to_string(executeCommandStatus)
					+ ", incrontabExecuteCommand: " + incrontabExecuteCommand
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
        }
	}
	catch (...)
	{
		string errorMessage = __FILEREF__ + "removeFromIncrontab failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);
	}
}

