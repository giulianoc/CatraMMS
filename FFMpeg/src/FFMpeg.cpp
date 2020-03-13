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
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/FileIO.h"
#include "FFMpeg.h"


FFMpeg::FFMpeg(Json::Value configuration,
        shared_ptr<spdlog::logger> logger) 
{
    _logger             = logger;

    _ffmpegPath = configuration["ffmpeg"].get("path", "").asString();
    _ffmpegTempDir = configuration["ffmpeg"].get("tempDir", "").asString();
    _ffmpegTtfFontDir = configuration["ffmpeg"].get("ttfFontDir", "").asString();

    _waitingNFSSync_attemptNumber = asInt(configuration["storage"],
		"waitingNFSSync_attemptNumber", 1);
	/*
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->waitingNFSSync_attemptNumber: "
		+ to_string(_waitingNFSSync_attemptNumber)
    );
	*/
    _waitingNFSSync_sleepTimeInSeconds = asInt(configuration["storage"],
		"waitingNFSSync_sleepTimeInSeconds", 3);
	/*
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->waitingNFSSync_sleepTimeInSeconds: "
		+ to_string(_waitingNFSSync_sleepTimeInSeconds)
    );
	*/

    _charsToBeReadFromFfmpegErrorOutput     = 1024;
    
    _twoPasses = false;
    _currentlyAtSecondPass = false;

    _currentDurationInMilliSeconds      = -1;
    _currentMMSSourceAssetPathName      = "";
    _currentStagingEncodedAssetPathName = "";
    _currentIngestionJobKey             = -1;
    _currentEncodingJobKey              = -1;
}

FFMpeg::~FFMpeg() 
{
    
}

void FFMpeg::encodeContent(
        string mmsSourceAssetPathName,
        int64_t durationInMilliSeconds,
        // string encodedFileName,
        string stagingEncodedAssetPathName,
        string encodingProfileDetails,
        bool isVideo,   // if false it means is audio
        int64_t physicalPathKey,
        string customerDirectoryName,
        string relativePath,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "encodeContent";

    try
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

        string ffmpegAudioCodecParameter = "";
        string ffmpegAudioBitRateParameter = "";
        string ffmpegAudioOtherParameters = "";
        string ffmpegAudioChannelsParameter = "";
        string ffmpegAudioSampleRateParameter = "";


        _currentDurationInMilliSeconds      = durationInMilliSeconds;
        _currentMMSSourceAssetPathName      = mmsSourceAssetPathName;
        _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        _currentIngestionJobKey             = ingestionJobKey;
        _currentEncodingJobKey              = encodingJobKey;
        
        _currentlyAtSecondPass = false;

        // we will set by default _twoPasses to false otherwise, since the ffmpeg class is reused
        // it could remain set to true from a previous call
        _twoPasses = false;
        
        settingFfmpegParameters(
            stagingEncodedAssetPathName,

            encodingProfileDetails,
            isVideo,

            httpStreamingFileFormat,
			ffmpegHttpStreamingParameter,

            ffmpegFileFormatParameter,

            ffmpegVideoCodecParameter,
            ffmpegVideoProfileParameter,
            ffmpegVideoResolutionParameter,
            ffmpegVideoBitRateParameter,
            ffmpegVideoOtherParameters,
            _twoPasses,
            ffmpegVideoMaxRateParameter,
            ffmpegVideoBufSizeParameter,
            ffmpegVideoFrameRateParameter,
            ffmpegVideoKeyFramesRateParameter,

            ffmpegAudioCodecParameter,
            ffmpegAudioBitRateParameter,
            ffmpegAudioOtherParameters,
            ffmpegAudioChannelsParameter,
            ffmpegAudioSampleRateParameter
        );

        _outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + ".ffmpegoutput";

        if (httpStreamingFileFormat != "")
        {
			// hls or dash

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
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoResolutionParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("1");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
				// It should be useless to add the audio parameters in phase 1 but,
				// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
				//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
				//  So, this is the reason, I'm adding phase 2 as well
				// + "-an "    // disable audio
				addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

				// 2020-01-20: I removed the hls file format parameter because it was not working
				//	and added -f mp4. At the end it has to generate just the log file
				//	to be used in the second step
				// addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);
				//
				// addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-f");
				ffmpegArgumentList.push_back("mp4");

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
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
							+ ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						;            
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (first step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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
                    removeHavingPrefixFileName(_ffmpegTempDir /* stagingEncodedAssetPath */, passlogFileName);

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				string segmentPathFileName;
				if (httpStreamingFileFormat == "hls")
					segmentPathFileName =
						stagingEncodedAssetPathName 
						+ "/"
						+ to_string(_currentIngestionJobKey)
						+ "_"
						+ to_string(_currentEncodingJobKey)
						+ "_%04d.ts"
					;

				string stagingManifestAssetPathName =
					stagingEncodedAssetPathName
					+ "/" + manifestFileName;

				ffmpegArgumentList.clear();

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoResolutionParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("2");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
				addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

				addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

				if (httpStreamingFileFormat == "hls")
				{
					ffmpegArgumentList.push_back("-hls_segment_filename");
					ffmpegArgumentList.push_back(segmentPathFileName);
				}

				addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back(stagingManifestAssetPathName);

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

						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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
                    removeHavingPrefixFileName(_ffmpegTempDir /* stagingEncodedAssetPath */, passlogFileName);

					_logger->info(__FILEREF__ + "Remove"
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
					FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
				}

				bool exceptionInCaseOfError = false;
				removeHavingPrefixFileName(_ffmpegTempDir /* stagingEncodedAssetPath */, passlogFileName);

				_logger->info(__FILEREF__ + "Remove"
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
				FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
			}
			else
            {
				string segmentPathFileName;
				if (httpStreamingFileFormat == "hls")
					segmentPathFileName =
						stagingEncodedAssetPathName 
						+ "/"
						+ to_string(_currentIngestionJobKey)
						+ "_"
						+ to_string(_currentEncodingJobKey)
						+ "_%04d.ts"
					;

				string stagingManifestAssetPathName =
					stagingEncodedAssetPathName
					+ "/" + manifestFileName;

				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoResolutionParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);

				addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);

				if (httpStreamingFileFormat == "hls")
				{
					ffmpegArgumentList.push_back("-hls_segment_filename");
					ffmpegArgumentList.push_back(segmentPathFileName);
				}

				addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back(stagingManifestAssetPathName);

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

						throw runtime_error(errorMessage);
					}

					chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

					_logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
						+ ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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

                throw runtime_error(errorMessage);
            }

            // changes to be done to the manifest, see EncoderThread.cpp
        }
        else
        {
			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;

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
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoResolutionParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("1");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
				// It should be useless to add the audio parameters in phase 1 but,
				// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
				//	it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
				//	So, this is the reason, I'm adding phase 2 as well
                // + "-an "    // disable audio
				addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
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
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                            + ", iReturnedStatus: " + to_string(iReturnedStatus)
							+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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
                    removeHavingPrefixFileName(_ffmpegTempDir /* stagingEncodedAssetPath */, passlogFileName);
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
				// output options
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoResolutionParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("2");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
				addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

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

                        throw runtime_error(errorMessage);
                    }
                    
                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command (second step)"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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
                    removeHavingPrefixFileName(_ffmpegTempDir /* stagingEncodedAssetPath */, passlogFileName);
                    _logger->info(__FILEREF__ + "Remove"
                        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

					if (iReturnedStatus == 9)	// 9 means: SIGKILL
						throw FFMpegEncodingKilledByUser();
					else
						throw e;
                }

                bool exceptionInCaseOfError = false;
                removeHavingPrefixFileName(_ffmpegTempDir /* stagingEncodedAssetPath */, passlogFileName);
                _logger->info(__FILEREF__ + "Remove"
                    + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }
            else
            {
				ffmpegArgumentList.clear();
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceAssetPathName);
				// output options
				addToArguments(ffmpegVideoCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoProfileParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegVideoResolutionParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				addToArguments(ffmpegAudioCodecParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioOtherParameters, ffmpegArgumentList);
				addToArguments(ffmpegAudioChannelsParameter, ffmpegArgumentList);
				addToArguments(ffmpegAudioSampleRateParameter, ffmpegArgumentList);
				addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);

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

                        throw runtime_error(errorMessage);
                    }

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();
                    _logger->info(__FILEREF__ + "encodeContent: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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

void FFMpeg::overlayImageOnVideo(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,
        string mmsSourceImageAssetPathName,
        string imagePosition_X_InPixel,
        string imagePosition_Y_InPixel,
        // string encodedFileName,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "overlayImageOnVideo";

    try
    {
        _currentDurationInMilliSeconds      = videoDurationInMilliSeconds;
        _currentMMSSourceAssetPathName      = mmsSourceVideoAssetPathName;
        _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        _currentIngestionJobKey             = ingestionJobKey;
        _currentEncodingJobKey              = encodingJobKey;
        

        _outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + ".ffmpegoutput";

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
		#ifdef __EXECUTE__
            string ffmpegExecuteCommand;
		#else
			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
		#endif
            {
			#ifdef __EXECUTE__
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
                string globalOptions = "-y ";
                string inputOptions = "";
                string outputOptions =
                        ffmpegFilterComplex + " "
                        ;
                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + globalOptions
                        + inputOptions
                        + "-i " + mmsSourceVideoAssetPathName + " "
                        + "-i " + mmsSourceImageAssetPathName + " "
                        + outputOptions
                        
                        + stagingEncodedAssetPathName + " "
                        + "> " + _outputFfmpegPathFileName 
                        + " 2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif
			#else
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceVideoAssetPathName);
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceImageAssetPathName);
				// output options
				addToArguments(ffmpegFilterComplex, ffmpegArgumentList);
				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
			#endif

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "overlayImageOnVideo: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                    );

                    int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "overlayImageOnVideo: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", executeCommandStatus: " + to_string(executeCommandStatus)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
				#else
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

                        throw runtime_error(errorMessage);
                    }
				#endif

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "overlayImageOnVideo: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#else
                    _logger->info(__FILEREF__ + "overlayImageOnVideo: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#endif
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				#ifdef __EXECUTE__
                    string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                            + ", e.what(): " + e.what()
                    ;
				#else
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
				#endif
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
			#ifdef __EXECUTE__
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                ;
			#else
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
			#endif

                _logger->error(errorMessage);

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
        string textPosition_X_InPixel,
        string textPosition_Y_InPixel,
        string fontType,
        int fontSize,
        string fontColor,
        int textPercentageOpacity,
        bool boxEnable,
        string boxColor,
        int boxPercentageOpacity,

        // string encodedFileName,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "overlayTextOnVideo";

    try
    {
        _currentDurationInMilliSeconds      = videoDurationInMilliSeconds;
        _currentMMSSourceAssetPathName      = mmsSourceVideoAssetPathName;
        _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        _currentIngestionJobKey             = ingestionJobKey;
        _currentEncodingJobKey              = encodingJobKey;
        

        _outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + ".ffmpegoutput";

        {
            string ffmpegTextPosition_X_InPixel = 
                    regex_replace(textPosition_X_InPixel, regex("video_width"), "w");
            ffmpegTextPosition_X_InPixel = 
                    regex_replace(ffmpegTextPosition_X_InPixel, regex("text_width"), "text_w");
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

            string ffmpegDrawTextFilter = string("drawtext=text=") + text;
            if (textPosition_X_InPixel != "")
                ffmpegDrawTextFilter += (":x=" + ffmpegTextPosition_X_InPixel);
            if (textPosition_Y_InPixel != "")
                ffmpegDrawTextFilter += (":y=" + ffmpegTextPosition_Y_InPixel);               
            if (fontType != "")
                ffmpegDrawTextFilter += (":fontfile=" + _ffmpegTtfFontDir + "/" + fontType);
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

		#ifdef __EXECUTE__
            string ffmpegExecuteCommand;
		#else
			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
		#endif
            {
			#ifdef __EXECUTE__
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
                string globalOptions = "-y ";
                string inputOptions = "";
                string outputOptions =
                        ffmpegDrawTextFilter + " "
                        ;
                
                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + globalOptions
                        + inputOptions
                        + "-i " + mmsSourceVideoAssetPathName + " "
                        + outputOptions
                        + stagingEncodedAssetPathName + " "
                        + "> " + _outputFfmpegPathFileName 
                        + " 2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif
			#else
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceVideoAssetPathName);
				// output options
				// addToArguments(ffmpegDrawTextFilter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-vf");
				ffmpegArgumentList.push_back(ffmpegDrawTextFilter);
				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
			#endif

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "overlayTextOnVideo: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                    );

                    int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "overlayTextOnVideo: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", executeCommandStatus: " + to_string(executeCommandStatus)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
				#else
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

                        throw runtime_error(errorMessage);
                    }
				#endif

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "overlayTextOnVideo: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#else
                    _logger->info(__FILEREF__ + "overlayTextOnVideo: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#endif
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				#ifdef __EXECUTE__
                    string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                            + ", e.what(): " + e.what()
                    ;
				#else
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
				#endif
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
			#ifdef __EXECUTE__
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                ;
			#else
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
			#endif

                _logger->error(errorMessage);

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

void FFMpeg::videoSpeed(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,

        string videoSpeedType,
        int videoSpeedSize,

        // string encodedFileName,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "videoSpeed";

    try
    {
        _currentDurationInMilliSeconds      = videoDurationInMilliSeconds;
        _currentMMSSourceAssetPathName      = mmsSourceVideoAssetPathName;
        _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        _currentIngestionJobKey             = ingestionJobKey;
        _currentEncodingJobKey              = encodingJobKey;
        

        _outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + ".ffmpegoutput";

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
		#ifdef __EXECUTE__
            string ffmpegExecuteCommand;
		#else
			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
		#endif
            {
			#ifdef __EXECUTE__
			#else
				ffmpegArgumentList.push_back("ffmpeg");
				// global options
				ffmpegArgumentList.push_back("-y");
				// input options
				ffmpegArgumentList.push_back("-i");
				ffmpegArgumentList.push_back(mmsSourceVideoAssetPathName);
				// output options
				addToArguments(complexFilter, ffmpegArgumentList);
				addToArguments(videoMap, ffmpegArgumentList);
				addToArguments(audioMap, ffmpegArgumentList);
				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
			#endif

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "videoSpeed: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                    );

                    int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "videoSpeed: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", executeCommandStatus: " + to_string(executeCommandStatus)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
				#else
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

                        throw runtime_error(errorMessage);
                    }
				#endif

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "videoSpeed: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#else
                    _logger->info(__FILEREF__ + "videoSpeed: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#endif
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				#ifdef __EXECUTE__
                    string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                            + ", e.what(): " + e.what()
                    ;
				#else
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
				#endif
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
			#ifdef __EXECUTE__
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                ;
			#else
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, encoded file size is 0"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
			#endif

                _logger->error(errorMessage);

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
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid)
{
	int iReturnedStatus = 0;

	_currentApiName = "pictureInPicture";

    try
    {
		if (mainVideoDurationInMilliSeconds < overlayVideoDurationInMilliSeconds)
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

        _currentDurationInMilliSeconds      = mainVideoDurationInMilliSeconds;
        _currentMMSSourceAssetPathName      = mmsMainVideoAssetPathName;
        _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        _currentIngestionJobKey             = ingestionJobKey;
        _currentEncodingJobKey              = encodingJobKey;
        

        _outputFfmpegPathFileName =
                _ffmpegTempDir + "/"
                + to_string(_currentIngestionJobKey)
                + "_"
                + to_string(_currentEncodingJobKey)
                + ".ffmpegoutput";

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
		#ifdef __EXECUTE__
            string ffmpegExecuteCommand;
		#else
			vector<string> ffmpegArgumentList;
			ostringstream ffmpegArgumentListStream;
		#endif
            {
			#ifdef __EXECUTE__
                // ffmpeg <global-options> <input-options> -i <input> <output-options> <output>
                string globalOptions = "-y ";
                string inputOptions = "";
                string outputOptions =
                        ffmpegFilterComplex + " "
                        ;
                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + globalOptions
                        + inputOptions;
				if (soundOfMain)
					ffmpegExecuteCommand +=
                        ("-i " + mmsMainVideoAssetPathName + " " + "-i " + mmsOverlayVideoAssetPathName + " ");
				else
					ffmpegExecuteCommand +=
                        ("-i " + mmsOverlayVideoAssetPathName + " " + "-i " + mmsMainVideoAssetPathName + " ");
				ffmpegExecuteCommand +=
					(outputOptions

					+ stagingEncodedAssetPathName + " "
					+ "> " + _outputFfmpegPathFileName 
					+ " 2>&1")
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif
			#else
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
				addToArguments(ffmpegFilterComplex, ffmpegArgumentList);
				ffmpegArgumentList.push_back(stagingEncodedAssetPathName);
			#endif

                try
                {
                    chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "pictureInPicture: Executing ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                    );

                    int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "pictureInPicture: ffmpeg command failed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", executeCommandStatus: " + to_string(executeCommandStatus)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
				#else
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

                        throw runtime_error(errorMessage);
                    }
				#endif

                    chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

				#ifdef __EXECUTE__
                    _logger->info(__FILEREF__ + "pictureInPicture: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#else
                    _logger->info(__FILEREF__ + "pictureInPicture: Executed ffmpeg command"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                        + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
                    );
				#endif
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
				#ifdef __EXECUTE__
                    string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                            + ", e.what(): " + e.what()
                    ;
				#else
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
				#endif
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
			#ifdef __EXECUTE__
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, pictureInPicture encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                ;
			#else
                string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, pictureInPicture encoded file size is 0"
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
                ;
			#endif

                _logger->error(errorMessage);

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

void FFMpeg::removeHavingPrefixFileName(string directoryName, string prefixFileName)
{
    try
    {
        FileIO::DirectoryEntryType_t detDirectoryEntryType;
        shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (directoryName + "/");

        bool scanDirectoryFinished = false;
        while (!scanDirectoryFinished)
        {
            string directoryEntry;
            try
            {
                string directoryEntry = FileIO::readDirectory (directory,
                    &detDirectoryEntryType);

                if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                    continue;

                if (directoryEntry.size() >= prefixFileName.size() && directoryEntry.compare(0, prefixFileName.size(), prefixFileName) == 0) 
                {
                    bool exceptionInCaseOfError = false;
                    string pathFileName = directoryName + "/" + directoryEntry;
                    _logger->info(__FILEREF__ + "Remove"
                        + ", pathFileName: " + pathFileName);
                    FileIO::remove(pathFileName, exceptionInCaseOfError);
                }
            }
            catch(DirectoryListFinished e)
            {
                scanDirectoryFinished = true;
            }
            catch(runtime_error e)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
            catch(exception e)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
        }

        FileIO::closeDirectory (directory);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "removeHavingPrefixFileName failed"
            + ", e.what(): " + e.what()
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "removeHavingPrefixFileName failed");
    }
}

int FFMpeg::getEncodingProgress()
{
    int encodingPercentage = 0;

    try
    {
		if (_currentApiName == "liveProxyByHTTPStreaming"
				|| _currentApiName == "liveProxyByCDN")
		{
			// it's a live

			return -1;
		}

        if (!FileIO::isFileExisting(_outputFfmpegPathFileName.c_str()))
        {
            _logger->info(__FILEREF__ + "ffmpeg: Encoding status not available"
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            );

            throw FFMpegEncodingStatusNotAvailable();
        }

        string ffmpegEncodingStatus;
        try
        {
            int lastCharsToBeRead = 512;
            
            ffmpegEncodingStatus = getLastPartOfFile(_outputFfmpegPathFileName, lastCharsToBeRead);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "ffmpeg: Failure reading the encoding status file"
                + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
                + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
                + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
                + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
                + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
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
					_logger->error(__FILEREF__ + "Encoding status too big"
						+ ", duration: " + duration
						+ ", encodingSeconds: " + to_string(encodingSeconds)
						+ ", _twoPasses: " + to_string(_twoPasses)
						+ ", _currentlyAtSecondPass: " + to_string(_currentlyAtSecondPass)
						+ ", currentTimeInMilliSeconds: " + to_string(currentTimeInMilliSeconds)
						+ ", _currentDurationInMilliSeconds: " + to_string(_currentDurationInMilliSeconds)
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
						+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
						+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
						+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
					);

					encodingPercentage		= 0;
				}
				else
				{
					_logger->info(__FILEREF__ + "Encoding status"
						+ ", duration: " + duration
						+ ", encodingSeconds: " + to_string(encodingSeconds)
						+ ", _twoPasses: " + to_string(_twoPasses)
						+ ", _currentlyAtSecondPass: " + to_string(_currentlyAtSecondPass)
						+ ", currentTimeInMilliSeconds: " + to_string(currentTimeInMilliSeconds)
						+ ", _currentDurationInMilliSeconds: " + to_string(_currentDurationInMilliSeconds)
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
						+ ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
						+ ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
						+ ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
					);
				}
            }
        }
    }
    catch(FFMpegEncodingStatusNotAvailable e)
    {
        _logger->info(__FILEREF__ + "ffmpeg: getEncodingProgress failed"
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
            + ", e.what(): " + e.what()
        );

        throw FFMpegEncodingStatusNotAvailable();
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg: getEncodingProgress failed"
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
        );

        throw e;
    }
    
    return encodingPercentage;
}

tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
	FFMpeg::getMediaInfo(string mmsAssetPathName)
{
	_currentApiName = "getMediaInfo";

	_logger->info(__FILEREF__ + "getMediaInfo"
			", mmsAssetPathName: " + mmsAssetPathName
			);

    size_t fileNameIndex = mmsAssetPathName.find_last_of("/");
    if (fileNameIndex == string::npos)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: No fileName find in the asset path name"
                + ", mmsAssetPathName: " + mmsAssetPathName;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    string sourceFileName = mmsAssetPathName.substr(fileNameIndex + 1);

    string      detailsPathFileName =
            _ffmpegTempDir + "/" + sourceFileName + ".json";
    
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
            + "-v quiet -print_format json -show_streams -show_format "
            + mmsAssetPathName + " "
            + "> " + detailsPathFileName 
            + " 2>&1"
            ;

    #ifdef __APPLE__
        ffprobeExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    try
    {
        _logger->info(__FILEREF__ + "getMediaInfo: Executing ffprobe command"
            + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

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
				if (FileIO::fileExisting(mmsAssetPathName))
				{
					string errorMessage = __FILEREF__ +
						"getMediaInfo: ffmpeg: ffprobe command failed"
						+ ", executeCommandStatus: " + to_string(executeCommandStatus)
						+ ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
					;

					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else
				{
					if (attemptIndex < _waitingNFSSync_attemptNumber)
					{
						attemptIndex++;

						string errorMessage = __FILEREF__
							+ "getMediaInfo: The file does not exist, waiting because of nfs delay"
							+ ", executeCommandStatus: " + to_string(executeCommandStatus)
							+ ", attemptIndex: " + to_string(attemptIndex)
							+ ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
						;

						_logger->warn(errorMessage);

						this_thread::sleep_for(
								chrono::seconds(_waitingNFSSync_sleepTimeInSeconds));
					}
					else
					{
						string errorMessage = __FILEREF__
							+ "getMediaInfo: ffmpeg: ffprobe command failed because the file does not exist"
							+ ", executeCommandStatus: " + to_string(executeCommandStatus)
							+ ", attemptIndex: " + to_string(attemptIndex)
							+ ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
			else
			{
				executeDone = true;
			}
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "getMediaInfo: Executed ffmpeg command"
            + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
            + ", statistics duration (secs): "
				+ to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
        );
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                detailsPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg: ffprobe command failed"
                + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);

        throw e;
    }

    int64_t durationInMilliSeconds = -1;
    long bitRate = -1;
    string videoCodecName;
    string videoProfile;
    int videoWidth = -1;
    int videoHeight = -1;
    string videoAvgFrameRate;
    long videoBitRate = -1;
    string audioCodecName;
    long audioSampleRate = -1;
    int audioChannels = -1;
    long audioBitRate = -1;
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
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", details: " + buffer.str()
        );

        string mediaDetails = buffer.str();
        // LF and CR create problems to the json parser...
        while (mediaDetails.back() == 10 || mediaDetails.back() == 13)
            mediaDetails.pop_back();

        Json::Value detailsRoot;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(mediaDetails.c_str(),
                    mediaDetails.c_str() + mediaDetails.size(), 
                    &detailsRoot, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: failed to parse the media details"
                        + ", mmsAssetPathName: " + mmsAssetPathName
                        + ", errors: " + errors
                        + ", mediaDetails: " + mediaDetails
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("ffmpeg: media json is not well format")
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", mediaDetails: " + mediaDetails
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
                
        string field = "streams";
        if (!isMetadataPresent(detailsRoot, field))
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        Json::Value streamsRoot = detailsRoot[field];
        bool videoFound = false;
        bool audioFound = false;
        for(int streamIndex = 0; streamIndex < streamsRoot.size(); streamIndex++) 
        {
            Json::Value streamRoot = streamsRoot[streamIndex];
            
            field = "codec_type";
            if (!isMetadataPresent(streamRoot, field))
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                        + ", mmsAssetPathName: " + mmsAssetPathName
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string codecType = streamRoot.get(field, "XXX").asString();
            
            if (codecType == "video" && !videoFound)
            {
                videoFound = true;

                field = "codec_name";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoCodecName = streamRoot.get(field, "XXX").asString();

                field = "profile";
                if (isMetadataPresent(streamRoot, field))
                    videoProfile = streamRoot.get(field, "XXX").asString();
                else
                {
                    /*
                    if (videoCodecName != "mjpeg")
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                                + ", mmsAssetPathName: " + mmsAssetPathName
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                     */
                }

                field = "width";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoWidth = asInt(streamRoot, field, 0);

                field = "height";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoHeight = asInt(streamRoot, field, 0);
                
                field = "avg_frame_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoAvgFrameRate = streamRoot.get(field, "XXX").asString();

                field = "bit_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    if (videoCodecName != "mjpeg")
                    {
                        // I didn't find bit_rate also in a ts file, let's set it as a warning
                        
                        string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                                + ", mmsAssetPathName: " + mmsAssetPathName
                                + ", Field: " + field;
                        _logger->warn(errorMessage);

                        // throw runtime_error(errorMessage);
                    }
                }
                else
                    videoBitRate = stol(streamRoot.get(field, "XXX").asString());
            }
            else if (codecType == "audio" && !audioFound)
            {
                audioFound = true;

                field = "codec_name";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                audioCodecName = streamRoot.get(field, "XXX").asString();

                field = "sample_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                audioSampleRate = stol(streamRoot.get(field, "XXX").asString());

                field = "channels";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                audioChannels = asInt(streamRoot, field, 0);
                
                field = "bit_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    // I didn't find bit_rate in a webm file, let's set it as a warning

                    string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->warn(errorMessage);

                    // throw runtime_error(errorMessage);
                }
				else
					audioBitRate = stol(streamRoot.get(field, "XXX").asString());
            }
        }

        field = "format";
        if (!isMetadataPresent(detailsRoot, field))
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        Json::Value formatRoot = detailsRoot[field];

        field = "duration";
        if (!isMetadataPresent(formatRoot, field))
        {
			// I didn't find it in a .avi file generated using OpenCV::VideoWriter
			// let's log it as a warning
            if (videoCodecName != "" && videoCodecName != "mjpeg")
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                    + ", mmsAssetPathName: " + mmsAssetPathName
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
        if (!isMetadataPresent(formatRoot, field))
        {
            if (videoCodecName != "" && videoCodecName != "mjpeg")
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }            
        }
        else
        {
            string bit_rate = formatRoot.get(field, "XXX").asString();
            bitRate = atoll(bit_rate.c_str());
        }

        _logger->info(__FILEREF__ + "Remove"
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);
    }
    catch(runtime_error e)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: error processing ffprobe output"
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);

        throw e;
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: error processing ffprobe output"
                + ", e.what(): " + e.what()
        ;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", detailsPathFileName: " + detailsPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);

        throw e;
    }

    /*
    if (durationInMilliSeconds == -1)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: durationInMilliSeconds was not able to be retrieved from media"
                + ", mmsAssetPathName: " + mmsAssetPathName
                + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds);
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    else if (width == -1 || height == -1)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: width/height were not able to be retrieved from media"
                + ", mmsAssetPathName: " + mmsAssetPathName
                + ", width: " + to_string(width)
                + ", height: " + to_string(height)
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
     */
    
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
    
    return make_tuple(durationInMilliSeconds, bitRate, 
            videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
            audioCodecName, audioSampleRate, audioChannels, audioBitRate
            );
}

vector<string> FFMpeg::generateFramesToIngest(
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string imageDirectory,
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

    _logger->info(__FILEREF__ + "generateFramesToIngest"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", encodingJobKey: " + to_string(encodingJobKey)
        + ", imageDirectory: " + imageDirectory
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
    
	int iReturnedStatus = 0;

    _currentDurationInMilliSeconds      = videoDurationInMilliSeconds;
    _currentMMSSourceAssetPathName      = mmsAssetPathName;
    _currentIngestionJobKey             = ingestionJobKey;
    _currentEncodingJobKey              = encodingJobKey;
        
    vector<string> generatedFramesFileNames;
    
    _outputFfmpegPathFileName =
            _ffmpegTempDir + "/"
            + to_string(_currentIngestionJobKey)
            + "_"
            + to_string(_currentEncodingJobKey)
            + ".generateFrame.log"
            ;
        
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
            videoFilterParameters = "-vf select='eq(pict_type,PICT_TYPE_I)' -vsync vfr ";
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
#ifdef __EXECUTE__
    string globalOptions = "-y ";
    string inputOptions = "";
    string outputOptions =
            "-ss " + to_string(startTimeInSeconds) + " "
            + (framesNumber != -1 ? ("-vframes " + to_string(framesNumber)) : "") + " "
            + videoFilterParameters
            + (mjpeg ? "-f mjpeg " : "")
            + "-an -s " + to_string(imageWidth) + "x" + to_string(imageHeight) + " "
            ;
    string ffmpegExecuteCommand = 
            _ffmpegPath + "/ffmpeg "
            + globalOptions
            + inputOptions
            + "-i " + mmsAssetPathName + " "
            + outputOptions
            + imageDirectory + "/" + localImageFileName + " "
            + "> " + _outputFfmpegPathFileName + " "
            + "2>&1"
            ;

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif
#else
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
	addToArguments(videoFilterParameters, ffmpegArgumentList);
	if (mjpeg)
	{
		ffmpegArgumentList.push_back("-f");
		ffmpegArgumentList.push_back("mjpeg");
	}
	ffmpegArgumentList.push_back("-an");
	ffmpegArgumentList.push_back("-s");
	ffmpegArgumentList.push_back(to_string(imageWidth) + "x" + to_string(imageHeight));
	ffmpegArgumentList.push_back(imageDirectory + "/" + localImageFileName);
#endif

    try
    {
        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

	#ifdef __EXECUTE__
        _logger->info(__FILEREF__ + "generateFramesToIngest: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "generateFramesToIngest: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
	#else
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

            throw runtime_error(errorMessage);
        }
	#endif

        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();
        
	#ifdef __EXECUTE__
        _logger->info(__FILEREF__ + "generateFramesToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
        );
	#else
        _logger->info(__FILEREF__ + "generateFramesToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
        );
	#endif
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
	#ifdef __EXECUTE__
        string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
        ;
	#else
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
	#endif
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
     
    if (mjpeg || framesNumber == 1)
        generatedFramesFileNames.push_back(localImageFileName);
    else
    {
        // get files from file system
    
        FileIO::DirectoryEntryType_t detDirectoryEntryType;
        shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (imageDirectory + "/");

        bool scanDirectoryFinished = false;
        while (!scanDirectoryFinished)
        {
            string directoryEntry;
            try
            {
                string directoryEntry = FileIO::readDirectory (directory,
                    &detDirectoryEntryType);
                
                if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                    continue;

                if (directoryEntry.size() >= imageBaseFileName.size() && 0 == directoryEntry.compare(0, imageBaseFileName.size(), imageBaseFileName))
                    generatedFramesFileNames.push_back(directoryEntry);
            }
            catch(DirectoryListFinished e)
            {
                scanDirectoryFinished = true;
            }
            catch(runtime_error e)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
            catch(exception e)
            {
                string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
        }

        FileIO::closeDirectory (directory);
    }
    
    /*
    bool inCaseOfLinkHasItToBeRead = false;
    unsigned long ulFileSize = FileIO::getFileSizeInBytes (
        localImagePathName, inCaseOfLinkHasItToBeRead);

    if (ulFileSize == 0)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed, image file size is 0"
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    } 
    */ 
    
    return generatedFramesFileNames;
}

void FFMpeg::generateConcatMediaToIngest(
        int64_t ingestionJobKey,
        vector<string>& sourcePhysicalPaths,
        string concatenatedMediaPathName)
{
	_currentApiName = "generateConcatMediaToIngest";

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
        
        concatListFile << "file '" << sourcePhysicalPath << "'" << endl;
    }
    concatListFile.close();

    _outputFfmpegPathFileName =
            _ffmpegTempDir + "/"
            + to_string(ingestionJobKey)
            + ".concat.log"
            ;

    // Then you can stream copy or re-encode your files
    // The -safe 0 above is not required if the paths are relative
    // ffmpeg -f concat -safe 0 -i mylist.txt -c copy output
	// 2019-10-10: added -fflags +genpts -async 1 for lipsync issue!!!
	// 2019-10-11: removed -fflags +genpts -async 1 because does not have inpact on lipsync issue!!!
    string ffmpegExecuteCommand = 
            _ffmpegPath + "/ffmpeg "
            + "-f concat -safe 0 -i " + concatenationListPathName + " "
            + "-c copy " + concatenatedMediaPathName + " "
            + "> " + _outputFfmpegPathFileName + " "
            + "2>&1"
            ;

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    try
    {
        _logger->info(__FILEREF__ + "generateConcatMediaToIngest: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "generateConcatMediaToIngest: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "generateConcatMediaToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", ffmpegCommandDuration (secs): "
				+ to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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

        bool exceptionInCaseOfError = false;
        _logger->info(__FILEREF__ + "Remove"
            + ", concatenationListPathName: " + concatenationListPathName);
        FileIO::remove(concatenationListPathName, exceptionInCaseOfError);
        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        throw e;
    }

    bool exceptionInCaseOfError = false;
    _logger->info(__FILEREF__ + "Remove"
        + ", concatenationListPathName: " + concatenationListPathName);
    FileIO::remove(concatenationListPathName, exceptionInCaseOfError);
    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

void FFMpeg::generateCutMediaToIngest(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
		bool keyFrameSeeking,
        double startTimeInSeconds,
        double endTimeInSeconds,
        int framesNumber,
        string cutMediaPathName)
{

	_currentApiName = "generateCutMediaToIngest";

    _outputFfmpegPathFileName =
            _ffmpegTempDir + "/"
            + to_string(ingestionJobKey)
            + ".cut.log"
            ;

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
	if (keyFrameSeeking)
	{
		ffmpegExecuteCommand = 
            _ffmpegPath + "/ffmpeg "
            + "-ss " + to_string(startTimeInSeconds) + " "
            + (framesNumber != -1 ? ("-vframes " + to_string(framesNumber) + " ") : ("-to " + to_string(endTimeInSeconds) + " "))
            + "-i " + sourcePhysicalPath + " "
			+ "-async 1 "
			// commented because aresample filtering requires encoding and here we are just streamcopy
            // + "-af \"aresample=async=1:min_hard_comp=0.100000:first_pts=0\" "
            + "-c copy " + cutMediaPathName + " "
            + "> " + _outputFfmpegPathFileName + " "
            + "2>&1"
            ;
	}
	else
		ffmpegExecuteCommand = 
            _ffmpegPath + "/ffmpeg "
            + "-i " + sourcePhysicalPath + " "
            + "-ss " + to_string(startTimeInSeconds) + " "
            + (framesNumber != -1 ? ("-vframes " + to_string(framesNumber) + " ") : ("-to " + to_string(endTimeInSeconds) + " "))
			+ "-async 1 "
			// commented because aresample filtering requires encoding and here we are just streamcopy
            // + "-af \"aresample=async=1:min_hard_comp=0.100000:first_pts=0\" "
            + "-c copy " + cutMediaPathName + " "
            + "> " + _outputFfmpegPathFileName + " "
            + "2>&1"
            ;

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    try
    {
        _logger->info(__FILEREF__ + "generateCutMediaToIngest: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "generateCutMediaToIngest: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "generateCutMediaToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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

void FFMpeg::generateSlideshowMediaToIngest(
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        vector<string>& sourcePhysicalPaths,
        double durationOfEachSlideInSeconds, 
        int outputFrameRate,
        string slideshowMediaPathName,
		pid_t* pChildPid)
{
	_currentApiName = "generateSlideshowMediaToIngest";

	int iReturnedStatus = 0;

    string slideshowListPathName =
        _ffmpegTempDir + "/"
        + to_string(ingestionJobKey)
        + ".slideshowList.txt"
        ;
        
    ofstream slideshowListFile(slideshowListPathName.c_str(), ofstream::trunc);
    string lastSourcePhysicalPath;
    for (string sourcePhysicalPath: sourcePhysicalPaths)
    {
        slideshowListFile << "file '" << sourcePhysicalPath << "'" << endl;
        slideshowListFile << "duration " << durationOfEachSlideInSeconds << endl;
        
        lastSourcePhysicalPath = sourcePhysicalPath;
    }
    slideshowListFile << "file '" << lastSourcePhysicalPath << "'" << endl;
    slideshowListFile.close();

    _outputFfmpegPathFileName =
            _ffmpegTempDir + "/"
            + to_string(ingestionJobKey)
            + ".slideshow.log"
            ;
    
    // Then you can stream copy or re-encode your files
    // The -safe 0 above is not required if the paths are relative
    // ffmpeg -f concat -safe 0 -i mylist.txt -c copy output

#ifdef __EXECUTE__
    string ffmpegExecuteCommand = 
            _ffmpegPath + "/ffmpeg "
            + "-f concat -safe 0 " 
            // + "-framerate 5/1 "
            + "-i " + slideshowListPathName + " "
            + "-c:v libx264 "
            + "-r " + to_string(outputFrameRate) + " "
            + "-vsync vfr "
            + "-pix_fmt yuv420p " + slideshowMediaPathName + " "
            + "> " + _outputFfmpegPathFileName + " "
            + "2>&1"
            ;

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif
#else
	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;

	ffmpegArgumentList.push_back("ffmpeg");
	ffmpegArgumentList.push_back("-f");
	ffmpegArgumentList.push_back("concat");
	ffmpegArgumentList.push_back("-safe");
	ffmpegArgumentList.push_back("0");
    // + "-framerate 5/1 "
	ffmpegArgumentList.push_back("-i");
	ffmpegArgumentList.push_back(slideshowListPathName);
	ffmpegArgumentList.push_back("-c:v");
	ffmpegArgumentList.push_back("libx264");
	ffmpegArgumentList.push_back("-r");
	ffmpegArgumentList.push_back(to_string(outputFrameRate));
	ffmpegArgumentList.push_back("-vsync");
	ffmpegArgumentList.push_back("vfr");
	ffmpegArgumentList.push_back("-pix_fmt");
	ffmpegArgumentList.push_back("yuv420p");
	ffmpegArgumentList.push_back(slideshowMediaPathName);
#endif

    try
    {
        chrono::system_clock::time_point startFfmpegCommand = chrono::system_clock::now();

	#ifdef __EXECUTE__
        _logger->info(__FILEREF__ + "generateSlideshowMediaToIngest: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );

        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "generateSlideshowMediaToIngest: ffmpeg command failed"
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
	#else
		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

        _logger->info(__FILEREF__ + "generateSlideshowMediaToIngest: Executing ffmpeg command"
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
			string errorMessage = __FILEREF__ + "generateSlideshowMediaToIngest: ffmpeg command failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            ;            
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
	#endif
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

	#ifdef __EXECUTE__
        _logger->info(__FILEREF__ + "generateSlideshowMediaToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
        );
	#else
        _logger->info(__FILEREF__ + "generateSlideshowMediaToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
        );
	#endif
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		#ifdef __EXECUTE__
			string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
			;
		#else
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
		#endif
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        _logger->info(__FILEREF__ + "Remove"
            + ", slideshowListPathName: " + slideshowListPathName);
        FileIO::remove(slideshowListPathName, exceptionInCaseOfError);

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else
			throw e;
    }

    _logger->info(__FILEREF__ + "Remove"
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
    
    _logger->info(__FILEREF__ + "Remove"
        + ", slideshowListPathName: " + slideshowListPathName);
    FileIO::remove(slideshowListPathName, exceptionInCaseOfError);
}

void FFMpeg::extractTrackMediaToIngest(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
        vector<pair<string,int>>& tracksToBeExtracted,
        string extractTrackMediaPathName)
{
	_currentApiName = "extractTrackMediaToIngest";

    _outputFfmpegPathFileName =
            _ffmpegTempDir + "/"
            + to_string(ingestionJobKey)
            + ".extractTrack.log"
            ;

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

            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "extractTrackMediaToIngest: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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
		string segmentListPathName,
		string recordedFileNamePrefix,
        string liveURL, string userAgent,
        time_t utcRecordingPeriodStart, 
        time_t utcRecordingPeriodEnd, 
        int segmentDurationInSeconds,
        string outputFileFormat,
		pid_t* pChildPid)
{
	_currentApiName = "liveRecorder";

#ifdef __EXECUTE__
	string ffmpegExecuteCommand;
#else
	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
#endif
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

		if (utcNow < utcRecordingPeriodStart)
		{
			// 2019-12-19: since the first chunk is removed, we will start a bit early
			// than utcRecordingPeriodStart (50% less than segmentDurationInSeconds)
			long secondsToStartEarly =
				segmentDurationInSeconds * 50 / 100;

			while (utcNow + secondsToStartEarly < utcRecordingPeriodStart)
			{
				time_t sleepTime = utcRecordingPeriodStart - (utcNow + secondsToStartEarly);

				_logger->info(__FILEREF__ + "LiveRecorder timing. "
						+ "Too early to start the LiveRecorder, just sleep "
					+ to_string(sleepTime) + " seconds"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcNow: " + to_string(utcNow)
                    + ", secondsToStartEarly: " + to_string(secondsToStartEarly)
                    + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
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
            string errorMessage = __FILEREF__ + "LiveRecorder timing. "
				+ "Too late to start the LiveRecorder"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
                    + ", utcNow: " + to_string(utcNow)
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
		else
		{
            string errorMessage = __FILEREF__ + "LiveRecorder timing. "
				+ "We are a bit late to start the LiveRecorder, let's start it"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", delay (secs): " + to_string(utcNow - utcRecordingPeriodStart)
                    + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
                    + ", utcNow: " + to_string(utcNow)
                    + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
            ;

            _logger->warn(errorMessage);
		}

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey) + "_"
			+ to_string(encodingJobKey)
			+ ".liveRecorder.log"
			;
    
		string recordedFileNameTemplate = recordedFileNamePrefix
			+ "_%Y-%m-%d_%H-%M-%S_%s." + outputFileFormat;

	{
		chrono::system_clock::time_point now = chrono::system_clock::now();
		utcNow = chrono::system_clock::to_time_t(now);
	}

	#ifdef __EXECUTE__
		ffmpegExecuteCommand = 
			_ffmpegPath + "/ffmpeg "
			+ (userAgent == "" ? "" : "-user_agent " + userAgent + " ")
			+ "-i " + liveURL + " "
			+ "-t " + to_string(utcRecordingPeriodEnd - utcNow) + " "
			+ "-c:v copy "
			+ "-c:a copy "
			+ "-f segment "
			+ "-segment_list " + segmentListPathName + " "
			+ "-segment_time " + to_string(segmentDurationInSeconds) + " "
			+ "-segment_atclocktime 1 "
			+ "-strftime 1 \"" + segmentListPath + "/" + recordedFileNameTemplate + "\" "
			+ "> " + _outputFfmpegPathFileName + " "
			+ "2>&1"
		;

		#ifdef __APPLE__
			ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=")
					+ getenv("DYLD_LIBRARY_PATH") + "; ");
		#endif

        _logger->info(__FILEREF__ + "liveRecorder: Executing ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );
	#else

		ffmpegArgumentList.push_back("ffmpeg");
		// addToArguments("-loglevel repeat+level+trace", ffmpegArgumentList);
		if (userAgent != "")
		{
			ffmpegArgumentList.push_back("-user_agent");
			ffmpegArgumentList.push_back(userAgent);
		}
		ffmpegArgumentList.push_back("-i");
		ffmpegArgumentList.push_back(liveURL);
		ffmpegArgumentList.push_back("-t");
		ffmpegArgumentList.push_back(to_string(utcRecordingPeriodEnd - utcNow));
		ffmpegArgumentList.push_back("-c:v");
		ffmpegArgumentList.push_back("copy");
		ffmpegArgumentList.push_back("-c:a");
		ffmpegArgumentList.push_back("copy");
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

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

        _logger->info(__FILEREF__ + "liveRecorder: Executing ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
        );
	#endif

        startFfmpegCommand = chrono::system_clock::now();

	#ifdef __EXECUTE__
        int executeCommandStatus = ProcessUtility::execute(ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "liveRecorder: ffmpeg command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", executeCommandStatus: " + to_string(executeCommandStatus)
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
	#else
		bool redirectionStdOutput = true;
		bool redirectionStdError = true;

		ProcessUtility::forkAndExec (
			_ffmpegPath + "/ffmpeg",
			ffmpegArgumentList,
			_outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError,
			pChildPid, &iReturnedStatus);
		if (iReturnedStatus != 0)
        {
			string errorMessage = __FILEREF__ + "liveRecorder: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
                + ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            ;            
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
	#endif
        
        endFfmpegCommand = chrono::system_clock::now();

	#ifdef __EXECUTE__
        _logger->info(__FILEREF__ + "liveRecorder: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
        );
	#else
        _logger->info(__FILEREF__ + "liveRecorder: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
        );
	#endif
    }
    catch(runtime_error e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
		#ifdef __EXECUTE__
			string errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                + ", e.what(): " + e.what()
			;
		#else
			string errorMessage;
			if (iReturnedStatus == 9)	// 9 means: SIGKILL
				errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed because killed by the user"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
					+ ", e.what(): " + e.what()
				;
			else
				errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
					+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
					+ ", e.what(): " + e.what()
				;
		#endif
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", segmentListPathName: " + segmentListPathName);
        FileIO::remove(segmentListPathName, exceptionInCaseOfError);

		if (segmentListPath != "")
    	{
        	// get files from file system
    
        	FileIO::DirectoryEntryType_t detDirectoryEntryType;
        	shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (segmentListPath + "/");

        	bool scanDirectoryFinished = false;
        	while (!scanDirectoryFinished)
        	{
            	string directoryEntry;
            	try
            	{
                	string directoryEntry = FileIO::readDirectory (directory,
                    	&detDirectoryEntryType);
                
                	if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                    	continue;

                	if (directoryEntry.size() >= recordedFileNamePrefix.size() && 0 == directoryEntry.compare(0, recordedFileNamePrefix.size(), recordedFileNamePrefix))
					{
						string recordedPathNameToBeRemoved = segmentListPath + "/" + directoryEntry;
        				_logger->info(__FILEREF__ + "Remove"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
            				+ ", recordedPathNameToBeRemoved: " + recordedPathNameToBeRemoved);
        				// FileIO::remove(recordedPathNameToBeRemoved, exceptionInCaseOfError);
					}
            	}
            	catch(DirectoryListFinished e)
            	{
                	scanDirectoryFinished = true;
            	}
            	catch(runtime_error e)
            	{
                	string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
                       	+ ", e.what(): " + e.what()
                	;
                	_logger->error(errorMessage);

                	// throw e;
            	}
            	catch(exception e)
            	{
                	string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
                	;
                	_logger->error(errorMessage);

                	// throw e;
            	}
        	}

        	FileIO::closeDirectory (directory);
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
	}

    _logger->info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

void FFMpeg::liveProxyByHTTPStreaming(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string liveURL, string userAgent,

	string outputType,	// HLS or DASH

	// next are parameters for the output
	int segmentDurationInSeconds,
	int playlistEntriesNumber,
	string manifestFilePathName,
	pid_t* pChildPid)
{
	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	string segmentListPath;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;
	time_t utcNow;

	_currentApiName = "liveProxyByHTTPStreaming";

	string manifestDirectoryPathName;
    try
    {
		string outputTypeLowerCase;
		outputTypeLowerCase.resize(outputType.size());
		transform(outputType.begin(), outputType.end(), outputTypeLowerCase.begin(),
				[](unsigned char c){return tolower(c); } );

		if (outputTypeLowerCase != "hls" && outputTypeLowerCase != "dash")
		{
			string errorMessage = __FILEREF__ + "liveProxyByHTTPStreaming. Wrong output type (it has to be HLS or DASH)"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", outputType: " + outputType;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t manifestFilePathIndex = manifestFilePathName.find_last_of("/");
		if (manifestFilePathIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "No manifestDirectoryPath find in the manifest file path name"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", manifestFilePathName: " + manifestFilePathName;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);

		// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
		// I saw just once that the directory was not created and the liveencoder remains in the loop
		// where:
		//	1. the encoder returns an error becaise of the missing directory
		//	2. EncoderVideoAudioProxy calls again the encoder
		// So, for this reason, the below check is done
		if (!FileIO::directoryExisting(manifestDirectoryPathName))
		{
			_logger->warn(__FILEREF__ + "manifestDirectoryPathName does not exist!!! It will be created"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
					);

			_logger->info(__FILEREF__ + "Create directory"
                + ", manifestDirectoryPathName: " + manifestDirectoryPathName
            );
			bool noErrorIfExists = true;
			bool recursive = true;
			FileIO::createDirectory(manifestDirectoryPathName,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP |
				S_IROTH | S_IXOTH, noErrorIfExists, recursive);
		}

		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey) + "_"
			+ to_string(encodingJobKey)
			+ ".liveProxy.log"
		;

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

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
		ffmpegArgumentList.push_back("-i");
		ffmpegArgumentList.push_back(liveURL);
		ffmpegArgumentList.push_back("-c:v");
		ffmpegArgumentList.push_back("copy");
		ffmpegArgumentList.push_back("-c:a");
		ffmpegArgumentList.push_back("copy");
		if (outputTypeLowerCase == "hls")
		{
			ffmpegArgumentList.push_back("-hls_flags");
			ffmpegArgumentList.push_back("append_list");
			ffmpegArgumentList.push_back("-hls_time");
			ffmpegArgumentList.push_back(to_string(segmentDurationInSeconds));
			ffmpegArgumentList.push_back("-hls_list_size");
			ffmpegArgumentList.push_back(to_string(playlistEntriesNumber));
		}
		else if (outputTypeLowerCase == "dash")
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

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

		_logger->info(__FILEREF__ + "liveProxy: Executing ffmpeg command"
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
			string errorMessage = __FILEREF__ + "liveProxy: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
           ;            
           _logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
        
		endFfmpegCommand = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "liveProxy: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
			+ ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		else
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

		if (manifestDirectoryPathName != "")
    	{
        	// get files from file system
    
        	FileIO::DirectoryEntryType_t detDirectoryEntryType;
        	shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (
					manifestDirectoryPathName + "/");

        	bool scanDirectoryFinished = false;
        	while (!scanDirectoryFinished)
        	{
            	string directoryEntry;
            	try
            	{
                	string directoryEntry = FileIO::readDirectory (directory,
                    	&detDirectoryEntryType);
                
                	if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                    	continue;

					{
						string segmentPathNameToBeRemoved =
							manifestDirectoryPathName + "/" + directoryEntry;
        				_logger->info(__FILEREF__ + "Remove"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
            				+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
        				FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
					}
            	}
            	catch(DirectoryListFinished e)
            	{
                	scanDirectoryFinished = true;
            	}
            	catch(runtime_error e)
            	{
                	string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
                       	+ ", e.what(): " + e.what()
                	;
                	_logger->error(errorMessage);

                	// throw e;
            	}
            	catch(exception e)
            	{
                	string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
                	;
                	_logger->error(errorMessage);

                	// throw e;
            	}
        	}

        	FileIO::closeDirectory (directory);
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

    _logger->info(__FILEREF__ + "Remove"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
    bool exceptionInCaseOfError = false;
    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);    
}

void FFMpeg::liveProxyByCDN(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string liveURL, string userAgent,
	string cdnURL,
	pid_t* pChildPid)
{
	vector<string> ffmpegArgumentList;
	ostringstream ffmpegArgumentListStream;
	int iReturnedStatus = 0;
	string segmentListPath;
	chrono::system_clock::time_point startFfmpegCommand;
	chrono::system_clock::time_point endFfmpegCommand;
	time_t utcNow;

	_currentApiName = "liveProxyByCDN";

    try
    {
		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey) + "_"
			+ to_string(encodingJobKey)
			+ ".liveProxy.log"
		;

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		// sample: ffmpeg -re -i http://80.211.238.33/restream/fiera.m3u8 -c copy -bsf:a aac_adtstoasc -vcodec copy -f flv rtmp://1.s.cdn77.eu:1936/static/LS-PRG-43330-22?password=hrpiTIFmsK3R

		ffmpegArgumentList.push_back("ffmpeg");
		// -re (input) Read input at native frame rate. By default ffmpeg attempts to read the input(s)
		//		as fast as possible. This option will slow down the reading of the input(s)
		//		to the native frame rate of the input(s). It is useful for real-time output
		//		(e.g. live streaming).
		ffmpegArgumentList.push_back("-nostdin");
		ffmpegArgumentList.push_back("-re");
		ffmpegArgumentList.push_back("-itsoffset");
		ffmpegArgumentList.push_back("-0.5");
		ffmpegArgumentList.push_back("-i");
		ffmpegArgumentList.push_back(liveURL);
		ffmpegArgumentList.push_back("-c:v");
		ffmpegArgumentList.push_back("copy");
		ffmpegArgumentList.push_back("-c:a");
		ffmpegArgumentList.push_back("copy");
		ffmpegArgumentList.push_back("-bsf:a");
		ffmpegArgumentList.push_back("aac_adtstoasc");
		ffmpegArgumentList.push_back("-vcodec");
		ffmpegArgumentList.push_back("copy");

		// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
		ffmpegArgumentList.push_back("-f");
		ffmpegArgumentList.push_back("flv");
		ffmpegArgumentList.push_back(cdnURL);

		if (!ffmpegArgumentList.empty())
			copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(),
				ostream_iterator<string>(ffmpegArgumentListStream, " "));

		_logger->info(__FILEREF__ + "liveProxy: Executing ffmpeg command"
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
			string errorMessage = __FILEREF__ + "liveProxy: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", iReturnedStatus: " + to_string(iReturnedStatus)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
           ;            
           _logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
        
		endFfmpegCommand = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "liveProxy: Executed ffmpeg command"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
			+ ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
		else
			errorMessage = __FILEREF__ + "ffmpeg: ffmpeg command failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
				+ ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
				+ ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
				+ ", e.what(): " + e.what()
			;
        _logger->error(errorMessage);

        _logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
        bool exceptionInCaseOfError = false;
        FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

		if (iReturnedStatus == 9)	// 9 means: SIGKILL
			throw FFMpegEncodingKilledByUser();
		else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
			throw FFMpegURLForbidden();
		else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
			throw FFMpegURLNotFound();
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

// destinationPathName will end with the new file format
void FFMpeg::changeFileFormat(
	int64_t ingestionJobKey,
	int64_t sourceKey,
	string sourcePhysicalPath,
	string destinationPathName)
{
	string ffmpegExecuteCommand;

	_currentApiName = "changeFileFormat";

    try
    {
		_outputFfmpegPathFileName =
			_ffmpegTempDir + "/"
			+ to_string(ingestionJobKey)
			+ "_" + to_string(sourceKey)
			+ ".changeFileFormat.log"
			;
    
		ffmpegExecuteCommand = 
			_ffmpegPath + "/ffmpeg "
			+ "-i " + sourcePhysicalPath + " "
			+ "-c:v copy "
			+ "-c:a copy "
			+ destinationPathName + " "
			+ "> " + _outputFfmpegPathFileName + " "
			+ "2>&1"
		;

		#ifdef __APPLE__
			ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=")
					+ getenv("DYLD_LIBRARY_PATH") + "; ");
		#endif

        _logger->info(__FILEREF__ + "changeFileFormat: Executing ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", sourceKey: " + to_string(sourceKey)
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

            throw runtime_error(errorMessage);
        }
        
        chrono::system_clock::time_point endFfmpegCommand = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "changeContainer: Executed ffmpeg command"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", sourceKey: " + to_string(sourceKey)
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            + ", ffmpegCommandDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count())
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

void FFMpeg::settingFfmpegParameters(
        string stagingEncodedAssetPathName,
        
        string encodingProfileDetails,
        bool isVideo,   // if false it means is audio
        
        string& httpStreamingFileFormat,
        string& ffmpegHttpStreamingParameter,

        string& ffmpegFileFormatParameter,

        string& ffmpegVideoCodecParameter,
        string& ffmpegVideoProfileParameter,
        string& ffmpegVideoResolutionParameter,
        string& ffmpegVideoBitRateParameter,
        string& ffmpegVideoOtherParameters,
        bool& twoPasses,
        string& ffmpegVideoMaxRateParameter,
        string& ffmpegVideoBufSizeParameter,
        string& ffmpegVideoFrameRateParameter,
        string& ffmpegVideoKeyFramesRateParameter,

        string& ffmpegAudioCodecParameter,
        string& ffmpegAudioBitRateParameter,
        string& ffmpegAudioOtherParameters,
        string& ffmpegAudioChannelsParameter,
        string& ffmpegAudioSampleRateParameter
)
{
    string field;
    Json::Value encodingProfileRoot;
    try
    {
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        string errors;

        bool parsingSuccessful = reader->parse(encodingProfileDetails.c_str(),
                encodingProfileDetails.c_str() + encodingProfileDetails.size(), 
                &encodingProfileRoot, &errors);
        delete reader;

        if (!parsingSuccessful)
        {
            string errorMessage = __FILEREF__ + "ffmpeg: failed to parse the encoder details"
                    + ", errors: " + errors
                    + ", encodingProfileDetails: " + encodingProfileDetails
                    ;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
    }
    catch(...)
    {
        throw runtime_error(string("ffmpeg: wrong encoding profile json format")
                + ", encodingProfileDetails: " + encodingProfileDetails
                );
    }

    // fileFormat
    string fileFormat;
	string fileFormatLowerCase;
    {
		field = "FileFormat";
		if (!isMetadataPresent(encodingProfileRoot, field))
		{
			string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
				+ ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        fileFormat = encodingProfileRoot.get(field, "XXX").asString();
		fileFormatLowerCase.resize(fileFormat.size());
		transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(),
			[](unsigned char c){return tolower(c); } );

        FFMpeg::encodingFileFormatValidation(fileFormat, _logger);

        if (fileFormatLowerCase == "hls")
        {
			httpStreamingFileFormat = "hls";

            ffmpegFileFormatParameter = 
				+ "-f hls "
			;

			long segmentDurationInSeconds = 10;

			field = "HLS";
			if (isMetadataPresent(encodingProfileRoot, field))
			{
				Json::Value hlsRoot = encodingProfileRoot[field]; 

				field = "SegmentDuration";
				if (isMetadataPresent(hlsRoot, field))
					segmentDurationInSeconds = asInt(hlsRoot, field, 10);
			}

            ffmpegHttpStreamingParameter = 
				"-hls_time " + to_string(segmentDurationInSeconds) + " ";

			// hls_list_size: set the maximum number of playlist entries. If set to 0 the list file
			//	will contain all the segments. Default value is 5.
            ffmpegHttpStreamingParameter += "-hls_list_size 0 ";
		}
		else if (fileFormatLowerCase == "dash")
        {
			httpStreamingFileFormat = "dash";

            ffmpegFileFormatParameter = 
				+ "-f dash "
			;

			long segmentDurationInSeconds = 10;

			field = "DASH";
			if (isMetadataPresent(encodingProfileRoot, field))
			{
				Json::Value dashRoot = encodingProfileRoot[field]; 

				field = "SegmentDuration";
				if (isMetadataPresent(dashRoot, field))
					segmentDurationInSeconds = asInt(dashRoot, field, 10);
			}

            ffmpegHttpStreamingParameter =
				"-seg_duration " + to_string(segmentDurationInSeconds) + " ";

			// hls_list_size: set the maximum number of playlist entries. If set to 0 the list file
			//	will contain all the segments. Default value is 5.
            // ffmpegHttpStreamingParameter += "-hls_list_size 0 ";

			// it is important to specify -init_seg_name because those files
			// will not be removed in EncoderVideoAudioProxy.cpp
            ffmpegHttpStreamingParameter +=
				"-init_seg_name init-stream$RepresentationID$.$ext$ ";

			// the only difference with the ffmpeg default is that default is $Number%05d$
			// We had to change it to $Number%01d$ because otherwise the generated file containing
			// 00001 00002 ... but the videojs player generates file name like 1 2 ...
			// and the streaming was not working
            ffmpegHttpStreamingParameter +=
				"-media_seg_name chunk-stream$RepresentationID$-$Number%01d$.$ext$ ";
		}
        else
        {
            httpStreamingFileFormat = "";

            ffmpegFileFormatParameter =
				" -f " + fileFormatLowerCase + " "
            ;
        }
    }

    if (isVideo)
    {
        field = "Video";
        if (!isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value videoRoot = encodingProfileRoot[field]; 

        // codec
        string codec;
        {
            field = "Codec";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            codec = videoRoot.get(field, "XXX").asString();

            FFMpeg::encodingVideoCodecValidation(codec, _logger);

            ffmpegVideoCodecParameter   =
                    "-codec:v " + codec + " "
            ;
        }

        // profile
        {
            field = "Profile";
            if (isMetadataPresent(videoRoot, field))
            {
                string profile = videoRoot.get(field, "XXX").asString();

                FFMpeg::encodingVideoProfileValidation(codec, profile, _logger);
                if (codec == "libx264")
                {
                    ffmpegVideoProfileParameter =
                            "-profile:v " + profile + " "
                    ;
                }
                else if (codec == "libvpx")
                {
                    ffmpegVideoProfileParameter =
                            "-quality " + profile + " "
                    ;
                }
                else
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: codec is wrong"
                            + ", codec: " + codec;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // resolution
        {
            field = "Width";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            int width = asInt(videoRoot, field, 0);
            if (width == -1 && codec == "libx264")
                width   = -2;     // h264 requires always a even width/height
        
            field = "Height";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            int height = asInt(videoRoot, field, 0);
            if (height == -1 && codec == "libx264")
                height   = -2;     // h264 requires always a even width/height

            ffmpegVideoResolutionParameter =
                    "-vf scale=" + to_string(width) + ":" + to_string(height) + " "
            ;
        }

        // bitRate
        {
            field = "KBitRate";
            if (isMetadataPresent(videoRoot, field))
            {
                int bitRate = asInt(videoRoot, field, 0);

                ffmpegVideoBitRateParameter =
                        "-b:v " + to_string(bitRate) + "k "
                ;
            }
        }

        // OtherOutputParameters
        {
            field = "OtherOutputParameters";
            if (isMetadataPresent(videoRoot, field))
            {
                string otherOutputParameters = videoRoot.get(field, "XXX").asString();

                ffmpegVideoOtherParameters =
                        otherOutputParameters + " "
                ;
            }
        }

        // twoPasses
        {
            field = "TwoPasses";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
			twoPasses = asBool(videoRoot, field, false);
        }

        // maxRate
        {
            field = "MaxRate";
            if (isMetadataPresent(videoRoot, field))
            {
                int maxRate = asInt(videoRoot, field, 0);

                ffmpegVideoMaxRateParameter =
                        "-maxrate " + to_string(maxRate) + "k "
                ;
            }
        }

        // bufSize
        {
            field = "BufSize";
            if (isMetadataPresent(videoRoot, field))
            {
                int bufSize = asInt(videoRoot, field, 0);

                ffmpegVideoBufSizeParameter =
                        "-bufsize " + to_string(bufSize) + "k "
                ;
            }
        }

        // frameRate
        {
            field = "FrameRate";
            if (isMetadataPresent(videoRoot, field))
            {
                int frameRate = asInt(videoRoot, field, 0);

                ffmpegVideoFrameRateParameter =
                        "-r " + to_string(frameRate) + " "
                ;

                // keyFrameIntervalInSeconds
                {
                    field = "KeyFrameIntervalInSeconds";
                    if (isMetadataPresent(videoRoot, field))
                    {
                        int keyFrameIntervalInSeconds = asInt(videoRoot, field, 0);

                        // -g specifies the number of frames in a GOP
                        ffmpegVideoKeyFramesRateParameter =
                                "-g " + to_string(frameRate * keyFrameIntervalInSeconds) + " "
                        ;
                    }
                }
            }
        }
    }
    
    // if (contentType == "video" || contentType == "audio")
    {
        field = "Audio";
        if (!isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value audioRoot = encodingProfileRoot[field]; 

        // codec
        {
            field = "Codec";
            if (!isMetadataPresent(audioRoot, field))
            {
                string errorMessage = __FILEREF__ + "ffmpeg: Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string codec = audioRoot.get(field, "XXX").asString();

            FFMpeg::encodingAudioCodecValidation(codec, _logger);

            ffmpegAudioCodecParameter   =
                    "-acodec " + codec + " "
            ;
        }

        // kBitRate
        {
            field = "KBitRate";
            if (isMetadataPresent(audioRoot, field))
            {
                int bitRate = asInt(audioRoot, field, 0);

                ffmpegAudioBitRateParameter =
                        "-b:a " + to_string(bitRate) + "k "
                ;
            }
        }
        
        // OtherOutputParameters
        {
            field = "OtherOutputParameters";
            if (isMetadataPresent(audioRoot, field))
            {
                string otherOutputParameters = audioRoot.get(field, "XXX").asString();

                ffmpegAudioOtherParameters =
                        otherOutputParameters + " "
                ;
            }
        }

        // channelsNumber
        {
            field = "ChannelsNumber";
            if (isMetadataPresent(audioRoot, field))
            {
                int channelsNumber = asInt(audioRoot, field, 0);

                ffmpegAudioChannelsParameter =
                        "-ac " + to_string(channelsNumber) + " "
                ;
            }
        }

        // sample rate
        {
            field = "SampleRate";
            if (isMetadataPresent(audioRoot, field))
            {
                int sampleRate = asInt(audioRoot, field, 0);

                ffmpegAudioSampleRateParameter =
                        "-ar " + to_string(sampleRate) + " "
                ;
            }
        }
    }
}

void FFMpeg::addToArguments(string parameter, vector<string>& argumentList)
{
	_logger->info(__FILEREF__ + "addToArguments"
			+ ", parameter: " + parameter
	);

	if (parameter != "")
	{
		string item;
		stringstream parameterStream(parameter);

		while(getline(parameterStream, item, ' '))
		{
			if (item != "")
				argumentList.push_back(item);
		}
	}
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

void FFMpeg::encodingFileFormatValidation(string fileFormat,
        shared_ptr<spdlog::logger> logger)
{
	string fileFormatLowerCase;
	fileFormatLowerCase.resize(fileFormat.size());
	transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(),
		[](unsigned char c){return tolower(c); } );

    if (fileFormatLowerCase != "3gp" 
		&& fileFormatLowerCase != "mp4" 
		&& fileFormatLowerCase != "webm" 
		&& fileFormatLowerCase != "hls"
		&& fileFormatLowerCase != "dash"
	)
    {
        string errorMessage = __FILEREF__ + "ffmpeg: fileFormat is wrong"
                + ", fileFormatLowerCase: " + fileFormatLowerCase;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpeg::encodingVideoCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger)
{    
    if (codec != "libx264" 
            && codec != "libvpx")
    {
        string errorMessage = __FILEREF__ + "ffmpeg: Video codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpeg::encodingVideoProfileValidation(
        string codec, string profile,
        shared_ptr<spdlog::logger> logger)
{
    if (codec == "libx264")
    {
        if (profile != "high" && profile != "baseline" && profile != "main"
				&& profile != "high422"	// used in case of mxf
			)
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Profile is wrong"
                    + ", codec: " + codec
                    + ", profile: " + profile;

            logger->error(errorMessage);
        
            throw runtime_error(errorMessage);
        }
    }
    else if (codec == "libvpx")
    {
        if (profile != "best" && profile != "good")
        {
            string errorMessage = __FILEREF__ + "ffmpeg: Profile is wrong"
                    + ", codec: " + codec
                    + ", profile: " + profile;

            logger->error(errorMessage);
        
            throw runtime_error(errorMessage);
        }
    }
    else
    {
        string errorMessage = __FILEREF__ + "ffmpeg: codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpeg::encodingAudioCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger)
{    
    if (codec != "aac" 
            && codec != "libfdk_aac" 
            && codec != "libvo_aacenc" 
            && codec != "libvorbis"
    )
    {
        string errorMessage = __FILEREF__ + "ffmpeg: Audio codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

bool FFMpeg::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull()
)
        return true;
    else
        return false;
}

int FFMpeg::asInt(Json::Value root, string field, int defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
			return strtol(root.asString().c_str(), nullptr, 10);
		else
			return root.asInt();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
			return strtol(root.get(field, defaultValue).asString().c_str(), nullptr, 10);
		else
			return root.get(field, defaultValue).asInt();
	}
}

int64_t FFMpeg::asInt64(Json::Value root, string field, int64_t defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
			return strtoll(root.asString().c_str(), nullptr, 10);
		else
			return root.asInt64();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
			return strtoll(root.get(field, defaultValue).asString().c_str(), nullptr, 10);
		else
			return root.get(field, defaultValue).asInt64();
	}
}

double FFMpeg::asDouble(Json::Value root, string field, double defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
			return stod(root.asString(), nullptr);
		else
			return root.asDouble();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
			return stod(root.get(field, defaultValue).asString(), nullptr);
		else
			return root.get(field, defaultValue).asDouble();
	}
}

bool FFMpeg::asBool(Json::Value root, string field, bool defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
		{
			string sTrue = "true";

			bool isEqual = root.asString().length() != sTrue.length() ? false :
				equal(root.asString().begin(), root.asString().end(), sTrue.begin(),
						[](int c1, int c2){ return toupper(c1) == toupper(c2); });

			return isEqual ? true : false;
		}
		else
			return root.asBool();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
		{
			string sTrue = "true";

			bool isEqual = root.asString().length() != sTrue.length() ? false :
				equal(root.asString().begin(), root.asString().end(), sTrue.begin(),
						[](int c1, int c2){ return toupper(c1) == toupper(c2); });

			return isEqual ? true : false;
		}
		else
			return root.get(field, defaultValue).asBool();
	}
}

