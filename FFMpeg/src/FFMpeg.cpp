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
        string encodedFileName,
        string stagingEncodedAssetPathName,
        string encodingProfileDetails,
        bool isVideo,   // if false it means is audio
        int64_t physicalPathKey,
        string customerDirectoryName,
        string relativePath,
        int64_t encodingJobKey,
        int64_t ingestionJobKey)
{
    try
    {
        bool segmentFileFormat;    
        string ffmpegFileFormatParameter = "";

        string ffmpegVideoCodecParameter = "";
        string ffmpegVideoProfileParameter = "";
        string ffmpegVideoResolutionParameter = "";
        string ffmpegVideoBitRateParameter = "";
        string ffmpegVideoMaxRateParameter = "";
        string ffmpegVideoBufSizeParameter = "";
        string ffmpegVideoFrameRateParameter = "";
        string ffmpegVideoKeyFramesRateParameter = "";

        string ffmpegAudioCodecParameter = "";
        string ffmpegAudioBitRateParameter = "";

        _currentDurationInMilliSeconds      = durationInMilliSeconds;
        _currentMMSSourceAssetPathName      = mmsSourceAssetPathName;
        _currentStagingEncodedAssetPathName = stagingEncodedAssetPathName;
        _currentIngestionJobKey             = ingestionJobKey;
        _currentEncodingJobKey              = encodingJobKey;
        
        settingFfmpegPatameters(
            stagingEncodedAssetPathName,

            encodingProfileDetails,
            isVideo,

            segmentFileFormat,
            ffmpegFileFormatParameter,

            ffmpegVideoCodecParameter,
            ffmpegVideoProfileParameter,
            ffmpegVideoResolutionParameter,
            ffmpegVideoBitRateParameter,
            _twoPasses,
            ffmpegVideoMaxRateParameter,
            ffmpegVideoBufSizeParameter,
            ffmpegVideoFrameRateParameter,
            ffmpegVideoKeyFramesRateParameter,

            ffmpegAudioCodecParameter,
            ffmpegAudioBitRateParameter
        );

        string stagingEncodedAssetPath;
        {
            size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
            if (fileNameIndex == string::npos)
            {
                string errorMessage = __FILEREF__ + "No fileName find in the staging encoded asset path name"
                        + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            
            stagingEncodedAssetPath = stagingEncodedAssetPathName.substr(0, fileNameIndex);
        }
        _outputFfmpegPathFileName = string(stagingEncodedAssetPath)
                + to_string(physicalPathKey)
                + ".ffmpegoutput";
        /*
        _outputFfmpegPathFileName = _mmsStorage->getStagingAssetPathName (
            customerDirectoryName,
            relativePath,
            ffmpegoutputPathName,
            -1,         // long long llMediaItemKey,
            -1,         // long long llPhysicalPathKey,
            true // removeLinuxPathIfExist
        );
         */

        if (segmentFileFormat)
        {
            string stagingEncodedSegmentAssetPathName =
                    stagingEncodedAssetPathName 
                    + "/"
                    + encodedFileName
                    + "_%04d.ts"
            ;

            string ffmpegExecuteCommand =
                    _ffmpegPath + "/ffmpeg "
                    + "-y -i " + mmsSourceAssetPathName + " "
                    + ffmpegVideoCodecParameter
                    + ffmpegVideoProfileParameter
                    + "-preset slow "
                    + ffmpegVideoBitRateParameter
                    + ffmpegVideoMaxRateParameter
                    + ffmpegVideoBufSizeParameter
                    + ffmpegVideoFrameRateParameter
                    + ffmpegVideoKeyFramesRateParameter
                    + ffmpegVideoResolutionParameter
                    + "-threads 0 "
                    + ffmpegAudioCodecParameter
                    + ffmpegAudioBitRateParameter
                    + ffmpegFileFormatParameter
                    + stagingEncodedSegmentAssetPathName + " "
                    + "> " + _outputFfmpegPathFileName + " "
                    + "2>&1"
            ;

            #ifdef __APPLE__
                ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
            #endif

            _logger->info(__FILEREF__ + "Executing ffmpeg command"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            );

            try
            {
                int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                if (executeCommandStatus != 0)
                {
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch(runtime_error e)
            {
                string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                        _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                string errorMessage = __FILEREF__ + "ffmpeg command failed"
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                        + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                throw e;
            }

            bool exceptionInCaseOfError = false;
            FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

            _logger->info(__FILEREF__ + "Encoded file generated"
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // changes to be done to the manifest, see EncoderThread.cpp
        }
        else
        {
            string ffmpegExecuteCommand;
            if (_twoPasses)
            {
                string ffmpegPassLogPathFileName = string(stagingEncodedAssetPath)
                    + to_string(physicalPathKey)
                    + "_"
                    + encodedFileName
                    + ".passlog"
                    ;

                // bool removeLinuxPathIfExist = true;
                /*
                string ffmpegPassLogPathFileName = _mmsStorage->getStagingAssetPathName (
                    customerDirectoryName,
                    relativePath,
                    passLogFileName,
                    -1,         // long long llMediaItemKey,
                    -1,         // long long llPhysicalPathKey,
                    true    // removeLinuxPathIfExist
                );
                 */

                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + "-y -i " + mmsSourceAssetPathName + " "
                        + ffmpegVideoCodecParameter
                        + ffmpegVideoProfileParameter
                        + "-preset slow "
                        + ffmpegVideoBitRateParameter
                        + ffmpegVideoMaxRateParameter
                        + ffmpegVideoBufSizeParameter
                        + ffmpegVideoFrameRateParameter
                        + ffmpegVideoKeyFramesRateParameter
                        + ffmpegVideoResolutionParameter
                        + "-threads 0 "
                        + "-pass 1 -passlogfile " + ffmpegPassLogPathFileName + " "
                        + "-an "
                        + ffmpegFileFormatParameter
                        + "/dev/null "
                        + "> " + _outputFfmpegPathFileName + " "
                        + "2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif

                _logger->info(__FILEREF__ + "Executing ffmpeg command"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                );

                try
                {
                    int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                            + ", e.what(): " + e.what()
                    ;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    FileIO::remove(ffmpegPassLogPathFileName, exceptionInCaseOfError);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                    throw e;
                }

                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + "-y -i " + mmsSourceAssetPathName + " "
                        + ffmpegVideoCodecParameter
                        + ffmpegVideoProfileParameter
                        + "-preset slow "
                        + ffmpegVideoBitRateParameter
                        + ffmpegVideoMaxRateParameter
                        + ffmpegVideoBufSizeParameter
                        + ffmpegVideoFrameRateParameter
                        + ffmpegVideoKeyFramesRateParameter
                        + ffmpegVideoResolutionParameter
                        + "-threads 0 "
                        + "-pass 2 -passlogfile " + ffmpegPassLogPathFileName + " "
                        + ffmpegAudioCodecParameter
                        + ffmpegAudioBitRateParameter
                        + ffmpegFileFormatParameter
                        + stagingEncodedAssetPathName + " "
                        + "> " + _outputFfmpegPathFileName 
                        + " 2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif

                _logger->info(__FILEREF__ + "Executing ffmpeg command"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                );

                _currentlyAtSecondPass = true;
                try
                {
                    int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                            + ", e.what(): " + e.what()
                    ;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    FileIO::remove(ffmpegPassLogPathFileName, exceptionInCaseOfError);
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                    throw e;
                }

                bool exceptionInCaseOfError = false;
                FileIO::remove(ffmpegPassLogPathFileName, exceptionInCaseOfError);
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }
            else
            {
                ffmpegExecuteCommand =
                        _ffmpegPath + "/ffmpeg "
                        + "-y -i " + mmsSourceAssetPathName + " "
                        + ffmpegVideoCodecParameter
                        + ffmpegVideoProfileParameter
                        + "-preset slow "
                        + ffmpegVideoBitRateParameter
                        + ffmpegVideoMaxRateParameter
                        + ffmpegVideoBufSizeParameter
                        + ffmpegVideoFrameRateParameter
                        + ffmpegVideoKeyFramesRateParameter
                        + ffmpegVideoResolutionParameter
                        + "-threads 0 "
                        + ffmpegAudioCodecParameter
                        + ffmpegAudioBitRateParameter
                        + ffmpegFileFormatParameter
                        + stagingEncodedAssetPathName + " "
                        + "> " + _outputFfmpegPathFileName 
                        + " 2>&1"
                ;

                #ifdef __APPLE__
                    ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
                #endif

                _logger->info(__FILEREF__ + "Executing ffmpeg command"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                );

                try
                {
                    int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
                    if (executeCommandStatus != 0)
                    {
                        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                        ;            
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(runtime_error e)
                {
                    string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                            _outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
                    string errorMessage = __FILEREF__ + "ffmpeg command failed"
                            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                            + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
                            + ", e.what(): " + e.what()
                    ;
                    _logger->error(errorMessage);

                    bool exceptionInCaseOfError = false;
                    FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);

                    throw e;
                }

                bool exceptionInCaseOfError = false;
                FileIO::remove(_outputFfmpegPathFileName, exceptionInCaseOfError);
            }

            _logger->info(__FILEREF__ + "Encoded file generated"
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);

            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "ffmpeg command failed, encoded file size is 0"
                        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        } 
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", e.what(): " + e.what()
        );

        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->info(__FILEREF__ + "Remove"
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(stagingEncodedAssetPathName);
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg encode failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->info(__FILEREF__ + "Remove"
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(stagingEncodedAssetPathName);
        }

        throw e;
    }
}

int FFMpeg::getEncodingProgress()
{
    int encodingPercentage;


    try
    {        
        if (!FileIO::isFileExisting(_outputFfmpegPathFileName.c_str()))
        {
            _logger->info(__FILEREF__ + "Encoding status not available"
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
            _logger->error(__FILEREF__ + "Failure reading the encoding status file"
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
    catch(...)
    {
        _logger->info(__FILEREF__ + "getEncodingProgress failed"
            + ", _currentMMSSourceAssetPathName: " + _currentMMSSourceAssetPathName
            + ", _currentStagingEncodedAssetPathName: " + _currentStagingEncodedAssetPathName
            + ", _currentIngestionJobKey: " + to_string(_currentIngestionJobKey)
            + ", _currentEncodingJobKey: " + to_string(_currentEncodingJobKey)
        );

        throw FFMpegEncodingStatusNotAvailable();
    }

    
    return encodingPercentage;
}

tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> FFMpeg::getMediaInfo(string mmsAssetPathName)
{
    size_t fileNameIndex = mmsAssetPathName.find_last_of("/");
    if (fileNameIndex == string::npos)
    {
        string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                + ", mmsAssetPathName: " + mmsAssetPathName;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    string sourceFileName = mmsAssetPathName.substr(fileNameIndex + 1);

    string      detailsPathFileName =
            string("/tmp/") + sourceFileName + ".json";
    
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

    _logger->info(__FILEREF__ + "Executing ffprobe command"
        + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
    );

    try
    {
        int executeCommandStatus = ProcessUtility:: execute (ffprobeExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "ffprobe command failed"
                    + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(exception e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                detailsPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffprobe command failed"
                + ", ffprobeExecuteCommand: " + ffprobeExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
        ;
        _logger->error(errorMessage);

        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);

        throw e;
    }

    int64_t durationInMilliSeconds = -1;
    long bitRate;
    string videoCodecName;
    string videoProfile;
    int videoWidth = -1;
    int videoHeight = -1;
    string videoAvgFrameRate;
    long videoBitRate;
    string audioCodecName;
    long audioSampleRate;
    int audioChannels;
    long audioBitRate;
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
                string errorMessage = __FILEREF__ + "failed to parse the media details"
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
            string errorMessage = string("media json is not well format")
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", mediaDetails: " + mediaDetails
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string field = "format";
        if (!isMetadataPresent(detailsRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        Json::Value formatRoot = detailsRoot[field];

        field = "duration";
        if (!isMetadataPresent(formatRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string duration = formatRoot.get(field, "XXX").asString();
        durationInMilliSeconds = atoll(duration.c_str()) * 1000;

        field = "bit_rate";
        if (!isMetadataPresent(formatRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string bit_rate = formatRoot.get(field, "XXX").asString();
        bitRate = atoll(bit_rate.c_str());
        
        field = "streams";
        if (!isMetadataPresent(detailsRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
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
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
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
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoCodecName = streamRoot.get(field, "XXX").asString();

                field = "profile";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoProfile = streamRoot.get(field, "XXX").asString();

                field = "width";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoWidth = streamRoot.get(field, "XXX").asInt();

                field = "height";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoHeight = streamRoot.get(field, "XXX").asInt();
                
                field = "avg_frame_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoAvgFrameRate = streamRoot.get(field, "XXX").asString();

                field = "bit_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                videoBitRate = stol(streamRoot.get(field, "XXX").asString());
            }
            else if (codecType == "audio" && !audioFound)
            {
                audioFound = true;

                field = "codec_name";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                audioCodecName = streamRoot.get(field, "XXX").asString();

                field = "sample_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                audioSampleRate = stol(streamRoot.get(field, "XXX").asString());

                field = "channels";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                audioChannels = streamRoot.get(field, "XXX").asInt();
                
                field = "bit_rate";
                if (!isMetadataPresent(streamRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", mmsAssetPathName: " + mmsAssetPathName
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                audioBitRate = stol(streamRoot.get(field, "XXX").asString());
            }
        }

        bool exceptionInCaseOfError = false;
        FileIO::remove(detailsPathFileName, exceptionInCaseOfError);
    }

    /*
    if (durationInMilliSeconds == -1)
    {
        string errorMessage = __FILEREF__ + "durationInMilliSeconds was not able to be retrieved from media"
                + ", mmsAssetPathName: " + mmsAssetPathName
                + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds);
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    else if (width == -1 || height == -1)
    {
        string errorMessage = __FILEREF__ + "width/height were not able to be retrieved from media"
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

void FFMpeg::generateScreenshotToIngest(
    string imagePathName,
    double timePositionInSeconds,
    int sourceImageWidth,
    int sourceImageHeight,
    string mmsAssetPathName)
{
    // ffmpeg -y -i [source.wmv] -f mjpeg -ss [10] -vframes 1 -an -s [176x144] [thumbnail_image.jpg]
    // -y: overwrite output files
    // -i: input file name
    // -f: force format
    // -ss: set the start time offset
    // -vframes: set the number of video frames to record
    // -an: disable audio
    // -s set frame size (WxH or abbreviation)

    size_t extensionIndex = imagePathName.find_last_of("/");
    if (extensionIndex == string::npos)
    {
        string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                + ", imagePathName: " + imagePathName;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string outputFfmpegPathFileName =
            string("/tmp/")
            + imagePathName.substr(extensionIndex + 1)
            + ".generateScreenshot.log"
            ;
    
    string ffmpegExecuteCommand = 
            _ffmpegPath + "/ffmpeg "
            + "-y -i " + mmsAssetPathName + " "
            + "-f mjpeg -ss " + to_string(timePositionInSeconds) + " "
            + "-vframes 1 -an -s " + to_string(sourceImageWidth) + "x" + to_string(sourceImageHeight) + " "
            + imagePathName + " "
            + "> " + outputFfmpegPathFileName + " "
            + "2>&1"
            ;

    #ifdef __APPLE__
        ffmpegExecuteCommand.insert(0, string("export DYLD_LIBRARY_PATH=") + getenv("DYLD_LIBRARY_PATH") + "; ");
    #endif

    _logger->info(__FILEREF__ + "Executing ffmpeg command"
        + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
    );

    try
    {
        int executeCommandStatus = ProcessUtility::execute (ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = __FILEREF__ + "ffmpeg command failed"
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(exception e)
    {
        string lastPartOfFfmpegOutputFile = getLastPartOfFile(
                outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
        string errorMessage = __FILEREF__ + "ffmpeg command failed"
                + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
                + ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile
        ;
        _logger->error(errorMessage);

        bool exceptionInCaseOfError = false;
        FileIO::remove(outputFfmpegPathFileName, exceptionInCaseOfError);

        throw e;
    }

    bool inCaseOfLinkHasItToBeRead = false;
    unsigned long ulFileSize = FileIO::getFileSizeInBytes (
        imagePathName, inCaseOfLinkHasItToBeRead);

    if (ulFileSize == 0)
    {
        string errorMessage = __FILEREF__ + "ffmpeg command failed, image file size is 0"
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
}

void FFMpeg::settingFfmpegPatameters(
        string stagingEncodedAssetPathName,
        
        string encodingProfileDetails,
        bool isVideo,   // if false it means is audio
        
        bool& segmentFileFormat,
        string& ffmpegFileFormatParameter,

        string& ffmpegVideoCodecParameter,
        string& ffmpegVideoProfileParameter,
        string& ffmpegVideoResolutionParameter,
        string& ffmpegVideoBitRateParameter,
        bool& twoPasses,
        string& ffmpegVideoMaxRateParameter,
        string& ffmpegVideoBufSizeParameter,
        string& ffmpegVideoFrameRateParameter,
        string& ffmpegVideoKeyFramesRateParameter,

        string& ffmpegAudioCodecParameter,
        string& ffmpegAudioBitRateParameter
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
            string errorMessage = __FILEREF__ + "failed to parse the encoder details"
                    + ", errors: " + errors
                    + ", encodingProfileDetails: " + encodingProfileDetails
                    ;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
    }
    catch(...)
    {
        throw runtime_error(string("wrong encoding profile json format")
                + ", encodingProfileDetails: " + encodingProfileDetails
                );
    }

    // fileFormat
    string fileFormat;
    {
        field = "fileFormat";
        if (!isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        fileFormat = encodingProfileRoot.get(field, "XXX").asString();

        FFMpeg::encodingFileFormatValidation(fileFormat, _logger);
        
        if (fileFormat == "segment")
        {
            segmentFileFormat = true;
            
            string stagingManifestAssetPathName =
                    stagingEncodedAssetPathName
                    + "/index.m3u8";
            
            ffmpegFileFormatParameter =
                    "-vbsf h264_mp4toannexb "
                    "-flags "
                    "-global_header "
                    "-map 0 "
                    "-f segment "
                    "-segment_time 10 "
                    "-segment_list " + stagingManifestAssetPathName + " "
            ;
        }
        else
        {
            segmentFileFormat = false;

            ffmpegFileFormatParameter =
                    " -f " + fileFormat + " "
            ;
        }
    }

    if (isVideo)
    {
        field = "video";
        if (!isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value videoRoot = encodingProfileRoot[field]; 

        // codec
        string codec;
        {
            field = "codec";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
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
            field = "profile";
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
                    string errorMessage = __FILEREF__ + "codec is wrong"
                            + ", codec: " + codec;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // resolution
        {
            field = "width";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string width = videoRoot.get(field, "XXX").asString();
            if (width == "-1" && codec == "libx264")
                width   = "-2";     // h264 requires always a even width/height
        
            field = "height";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string height = videoRoot.get(field, "XXX").asString();
            if (height == "-1" && codec == "libx264")
                height   = "-2";     // h264 requires always a even width/height

            ffmpegVideoResolutionParameter =
                    "-vf scale=" + width + ":" + height + " "
            ;
        }

        // bitRate
        {
            field = "bitRate";
            if (!isMetadataPresent(videoRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string bitRate = videoRoot.get(field, "XXX").asString();

            ffmpegVideoBitRateParameter =
                    "-b:v " + bitRate + " "
            ;
        }

        // bitRate
        {
            field = "twoPasses";
            if (!isMetadataPresent(videoRoot, field) 
                    && fileFormat != "segment") // twoPasses is used ONLY if it is NOT segment
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            if (fileFormat != "segment")
                twoPasses = videoRoot.get(field, "XXX").asBool();
        }

        // maxRate
        {
            field = "maxRate";
            if (isMetadataPresent(videoRoot, field))
            {
                string maxRate = videoRoot.get(field, "XXX").asString();

                ffmpegVideoMaxRateParameter =
                        "-maxrate " + maxRate + " "
                ;
            }
        }

        // bufSize
        {
            field = "bufSize";
            if (isMetadataPresent(videoRoot, field))
            {
                string bufSize = videoRoot.get(field, "XXX").asString();

                ffmpegVideoBufSizeParameter =
                        "-bufsize " + bufSize + " "
                ;
            }
        }

        /*
        // frameRate
        {
            field = "frameRate";
            if (isMetadataPresent(videoRoot, field))
            {
                string frameRate = videoRoot.get(field, "XXX").asString();

                int iFrameRate = stoi(frameRate);

                ffmpegVideoFrameRateParameter =
                        "-r " + frameRate + " "
                ;

                // keyFrameIntervalInSeconds
                {
                    field = "keyFrameIntervalInSeconds";
                    if (isMetadataPresent(videoRoot, field))
                    {
                        string keyFrameIntervalInSeconds = videoRoot.get(field, "XXX").asString();

                        int iKeyFrameIntervalInSeconds = stoi(keyFrameIntervalInSeconds);

                        ffmpegVideoKeyFramesRateParameter =
                                "-g " + to_string(iFrameRate * iKeyFrameIntervalInSeconds) + " "
                        ;
                    }
                }
            }
        }
         */
    }
    
    // if (contentType == "video" || contentType == "audio")
    {
        field = "audio";
        if (!isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value audioRoot = encodingProfileRoot[field]; 

        // codec
        {
            field = "codec";
            if (!isMetadataPresent(audioRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
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

        // bitRate
        {
            field = "bitRate";
            if (!isMetadataPresent(audioRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string bitRate = audioRoot.get(field, "XXX").asString();

            ffmpegAudioBitRateParameter =
                    "-b:a " + bitRate + " "
            ;
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
    if (fileFormat != "3gp" 
            && fileFormat != "mp4" 
            && fileFormat != "webm" 
            && fileFormat != "segment"
            )
    {
        string errorMessage = __FILEREF__ + "fileFormat is wrong"
                + ", fileFormat: " + fileFormat;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpeg::encodingVideoCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger)
{    
    if (codec != "libx264" && codec != "libvpx")
    {
        string errorMessage = __FILEREF__ + "Video codec is wrong"
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
        if (profile != "high" && profile != "baseline" && profile != "main")
        {
            string errorMessage = __FILEREF__ + "Profile is wrong"
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
            string errorMessage = __FILEREF__ + "Profile is wrong"
                    + ", codec: " + codec
                    + ", profile: " + profile;

            logger->error(errorMessage);
        
            throw runtime_error(errorMessage);
        }
    }
    else
    {
        string errorMessage = __FILEREF__ + "codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpeg::encodingAudioCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger)
{    
    if (codec != "libaacplus" 
            && codec != "libfdk_aac" 
            && codec != "libvo_aacenc" 
            && codec != "libvorbis"
    )
    {
        string errorMessage = __FILEREF__ + "Audio codec is wrong"
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
