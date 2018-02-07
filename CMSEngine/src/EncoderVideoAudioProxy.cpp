/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EnodingsManager.cpp
 * Author: giuliano
 * 
 * Created on February 4, 2018, 7:18 PM
 */

#include <fstream>
#include "catralibraries/ProcessUtility.h"
#include "EncoderVideoAudioProxy.h"

EncoderVideoAudioProxy::EncoderVideoAudioProxy(
        mutex* mtEncodingJobs,
        EncodingJobStatus* status,
        shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
        shared_ptr<CMSStorage> cmsStorage,
        shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem,
        shared_ptr<spdlog::logger> logger
)
{
    _logger                 = logger;
    
    _mtEncodingJobs         = mtEncodingJobs;
    _status                 = status;
    
    _cmsEngineDBFacade      = cmsEngineDBFacade;
    _cmsStorage             = cmsStorage;
    _encodingItem          = encodingItem;
    
    _ffmpegPathName        = "/app/ffmpeg-1.0-usr_include_Centos5_64/bin/ffmpeg ";
    _3GPPEncoder            = "FFMPEG";
    _mpeg2TSEncoder         = "FFMPEG";

}

EncoderVideoAudioProxy::~EncoderVideoAudioProxy() 
{
    lock_guard<mutex> locker(*_mtEncodingJobs);
    
    *_status = EncodingJobStatus::Free;

}

void EncoderVideoAudioProxy::operator()()
{
    string stagingEncodedAssetPathName;
    try
    {
        stagingEncodedAssetPathName = encodeContentVideoAudio();
    }
    catch(MaxConcurrentJobsReached e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        _cmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::MaxCapacityReached, _encodingItem->_ingestionJobKey);
        
        throw e;
    }
    catch(EncoderError e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        _cmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError, _encodingItem->_ingestionJobKey);

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        _cmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        throw e;
    }
    catch(exception e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        _cmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        throw e;
    }
            
    try
    {
        processEncodedContentVideoAudio(stagingEncodedAssetPathName);
    }
    catch(MaxConcurrentJobsReached e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->error(string("Remove")
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

        _cmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::MaxCapacityReached, _encodingItem->_ingestionJobKey);
        
        throw e;
    }
    catch(EncoderError e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->error(string("Remove")
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

        _cmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError, _encodingItem->_ingestionJobKey);

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->error(string("Remove")
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

        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        _cmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        throw e;
    }
    catch(exception e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->error(string("Remove")
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

        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        _cmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        throw e;
    }

    try
    {
        _cmsEngineDBFacade->updateEncodingJob (
            _encodingItem->_encodingJobKey, 
            CMSEngineDBFacade::EncodingError::NoError, 
            _encodingItem->_ingestionJobKey);
    }
    catch(exception e)
    {
        _logger->error(string("_cmsEngineDBFacade->updateEncodingJob failed: ") + e.what());

        throw e;
    }
}

string EncoderVideoAudioProxy::encodeContentVideoAudio()
{
    string stagingEncodedAssetPathName;
    
    if (
        (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::ThreeGPP &&
            _3GPPEncoder == "FFMPEG") ||
        (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::MPEG2_TS &&
            _mpeg2TSEncoder == "FFMPEG") ||
        _encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::WEBM ||
        (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::Adobe &&
            _mpeg2TSEncoder == "FFMPEG")
    )
    {
        stagingEncodedAssetPathName = encodeContent_VideoAudio_through_ffmpeg();
    }
    else if (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = "No Encoder available to encode WindowsMedia technology";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = "Unknown technology and no Encoder available to encode";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    return stagingEncodedAssetPathName;
}

string EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg()
{
    string cmsSourceAssetPathName = _cmsStorage->getCMSAssetPathName(
        _encodingItem->_cmsPartitionNumber,
        _encodingItem->_customer->_directoryName,
        _encodingItem->_relativePath,
        _encodingItem->_fileName);
    
    bool removeLinuxPathIfExist = true;
    
    size_t extensionIndex = _encodingItem->_fileName.find_last_of(".");
    if (extensionIndex == string::npos)
    {
        string errorMessage = string("No extension find in the asset file name")
                + ", _encodingItem->_fileName: " + _encodingItem->_fileName;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    string encodedFileName =
            _encodingItem->_fileName.substr(0, extensionIndex)
            + "_" 
            + to_string(_encodingItem->_encodingProfileKey);
    if (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::ThreeGPP)
        encodedFileName.append(".3gp");
    else if (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
            _encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::Adobe)
        ;
    else if (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::WEBM)
        encodedFileName.append(".webm");
    else
    {
        string errorMessage = "Unknown technology";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    string stagingEncodedAssetPathName = _cmsStorage->getStagingAssetPathName(
        _encodingItem->_customer->_directoryName,
        _encodingItem->_relativePath,
        encodedFileName,
        -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
        -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
        removeLinuxPathIfExist);
    
    if (_encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
        _encodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::Adobe)
    {
        // In this case, the path is a directory where to place the segments

        if (!FileIO::directoryExisting(stagingEncodedAssetPathName)) 
        {
            _logger->info(string("Create directory")
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(
                    stagingEncodedAssetPathName,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }        
    }

    string ffmpegEncodingProfilePathName = _cmsStorage->getFFMPEGEncodingProfilePathName(
        _encodingItem->_contentType, _encodingItem->_encodingProfileKey);

    ifstream ffmpegEncodingProfileJson(ffmpegEncodingProfilePathName, std::ifstream::binary);
    Json::Value encodingProfileRoot;
    try
    {
        ffmpegEncodingProfileJson >> encodingProfileRoot;
    }
    catch(...)
    {
        throw runtime_error(string("wrong encoding profile json format")
                + ", ffmpegEncodingProfilePathName: " + ffmpegEncodingProfilePathName
                );
    }
    
    /*
    {
        fileFormat = "3gp",         // mandatory, 3gp, webm or segment
        "video": {
            codec = "libx264",     // mandatory, libx264 or libvpx
            profile = "high",      // optional, if libx264 -> high or baseline or main. if libvpx -> best or good
            resolution = "-1:480", // mandatory
            bitRate = "500k",      // mandatory
            maxRate = "500k",      // optional
            bufSize = "1000k",     // optional
            frameRate = "25",      // optional
            keyFrameIntervalInSeconds = 5   // optional and only if framerate is present
        },
        "audio": {
            codec = "libaacplus",  // mandatory, libaacplus, libvo_aacenc or libvorbis
            bitRate = "128k"      // mandatory
        }
    }
    */
    string field;
    bool segmentFileFormat;
    string stagingManifestAssetPathName;
    
    string ffmpegFileFormatParameter;

    string ffmpegVideoCodecParameter;
    string ffmpegVideoProfileParameter;
    string ffmpegVideoResolutionParameter;
    string ffmpegVideoBitRateParameter;
    string ffmpegVideoMaxRateParameter;
    string ffmpegVideoBufSizeParameter;
    string ffmpegVideoFrameRateParameter;
    string ffmpegVideoKeyFramesRateParameter;

    string ffmpegAudioCodecParameter;
    string ffmpegAudioBitRateParameter;
    
    // fileFormat
    {
        field = "fileFormat";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = string("Field is not present or it is null")
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        string fileFormat = encodingProfileRoot.get(field, "XXX").asString();

        if (fileFormat != "3gp" && fileFormat != "webm" && fileFormat != "segment")
        {
            string errorMessage = string(field) + "is wrong"
                    + ", fileFormat: " + fileFormat;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        if (fileFormat != "segment")
        {
            segmentFileFormat = true;
            
            stagingManifestAssetPathName =
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

    if (_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Video)
    {
        field = "video";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = string("Field is not present or it is null")
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value videoRoot = encodingProfileRoot[field]; 

        // codec
        string codec;
        {
            field = "codec";
            if (!_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string errorMessage = string("Field is not present or it is null")
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            codec = videoRoot.get(field, "XXX").asString();

            if (codec != "libx264" && codec != "libvpx")
            {
                string errorMessage = string(field) + "is wrong"
                        + ", codec: " + codec;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            ffmpegVideoCodecParameter   =
                    "-vcodec " + codec + " "
            ;
        }

        // profile
        {
            field = "profile";
            if (_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string profile = videoRoot.get(field, "XXX").asString();

                if (codec == "libx264")
                {
                    if (profile != "high" && profile != "baseline" && profile != "main")
                    {
                        string errorMessage = string(field) + "is wrong"
                                + ", profile: " + profile;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    ffmpegVideoProfileParameter =
                            "-vprofile " + profile + " "
                    ;
                }
                else if (codec == "libvpx")
                {
                    if (profile != "best" && profile != "good")
                    {
                        string errorMessage = string(field) + "is wrong"
                                + ", profile: " + profile;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    ffmpegVideoProfileParameter =
                            "-quality " + profile + " "
                    ;
                }
                else
                {
                    string errorMessage = string("codec is wrong")
                            + ", codec: " + codec;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

        }

        // resolution
        {
            field = "resolution";
            if (!_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string errorMessage = string("Field is not present or it is null")
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string resolution = videoRoot.get(field, "XXX").asString();

            ffmpegVideoResolutionParameter =
                    "-vf scale= " + resolution + " "
            ;
        }

        // bitRate
        {
            field = "bitRate";
            if (!_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string errorMessage = string("Field is not present or it is null")
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string bitRate = videoRoot.get(field, "XXX").asString();

            ffmpegVideoBitRateParameter =
                    "-b:v " + bitRate + " "
            ;
        }

        // maxRate
        {
            field = "maxRate";
            if (_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
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
            if (_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string bufSize = videoRoot.get(field, "XXX").asString();

                ffmpegVideoBufSizeParameter =
                        "-bufsize " + bufSize + " "
                ;
            }
        }

        // frameRate
        {
            field = "frameRate";
            if (_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
            {
                string frameRate = videoRoot.get(field, "XXX").asString();

                int iFrameRate = stoi(frameRate);

                ffmpegVideoFrameRateParameter =
                        "-r " + frameRate + " "
                ;

                // keyFrameIntervalInSeconds
                {
                    field = "keyFrameIntervalInSeconds";
                    if (_cmsEngineDBFacade->isMetadataPresent(videoRoot, field))
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
    }
    
    if (_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Video ||
            _encodingItem->_contentType == CMSEngineDBFacade::ContentType::Audio)
    {
        field = "audio";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = string("Field is not present or it is null")
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value audioRoot = encodingProfileRoot[field]; 

        // codec
        {
            field = "codec";
            if (!_cmsEngineDBFacade->isMetadataPresent(audioRoot, field))
            {
                string errorMessage = string("Field is not present or it is null")
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string codec = audioRoot.get(field, "XXX").asString();

            if (codec != "libaacplus" && codec != "libvo_aacenc" && codec != "libvorbis")
            {
                string errorMessage = string(field) + "is wrong"
                        + ", codec: " + codec;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            ffmpegAudioCodecParameter   =
                    "-acodec " + codec + " "
            ;
        }

        // bitRate
        {
            field = "bitRate";
            if (!_cmsEngineDBFacade->isMetadataPresent(audioRoot, field))
            {
                string errorMessage = string("Field is not present or it is null")
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
    
    if (segmentFileFormat)
    {
        string stagingEncodedSegmentAssetPathName =
                stagingEncodedAssetPathName 
                + "/"
                + encodedFileName
                + "_%04d.ts"
        ;
        
        string ffmpegExecuteCommand =
                _ffmpegPathName
                + "-y -i " + cmsSourceAssetPathName + " "
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
                + stagingEncodedSegmentAssetPathName
        ;
        
        _logger->info(string("Executing ffmpeg command")
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );
        
        int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = string("ffmpeg command failed")
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }

        _logger->info(string("Encoded file generated")
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );
        
        // changes to be done to the manifest, see EncoderThread.cpp
    }
    else
    {
        string ffmpegExecuteCommand =
                _ffmpegPathName
                + "-y -i " + cmsSourceAssetPathName + " "
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
                + "-pass 1 -passlogfile /tmp/ffmpeg " + encodedFileName + " "
                + "-an "
                + ffmpegFileFormatParameter
                + "/dev/null "
        ;
        
        _logger->info(string("Executing ffmpeg command")
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );
        
        int executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = string("ffmpeg command failed")
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
        
        ffmpegExecuteCommand =
                _ffmpegPathName
                + "-y -i " + cmsSourceAssetPathName + " "
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
                + "-pass 2 -passlogfile /tmp/ffmpeg " + encodedFileName + " "
                + ffmpegAudioCodecParameter
                + ffmpegAudioBitRateParameter
                + ffmpegFileFormatParameter
                + stagingEncodedAssetPathName
        ;
        
        _logger->info(string("Executing ffmpeg command")
            + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
        );
        
        executeCommandStatus = ProcessUtility:: execute (ffmpegExecuteCommand);
        if (executeCommandStatus != 0)
        {
            string errorMessage = string("ffmpeg command failed")
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }

        _logger->info(string("Encoded file generated")
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );
        
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long ulFileSize = FileIO::getFileSizeInBytes (
            stagingEncodedAssetPathName, inCaseOfLinkHasItToBeRead);
        
        if (ulFileSize == 0)
        {
            string errorMessage = string("ffmpeg command failed, encoded file size is 0")
                    + ", ffmpegExecuteCommand: " + ffmpegExecuteCommand
            ;
            
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
    } 
    
    return stagingEncodedAssetPathName;
}

void EncoderVideoAudioProxy::processEncodedContentVideoAudio(string stagingEncodedAssetPathName)
{
    size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
    if (fileNameIndex == string::npos)
    {
        string errorMessage = string("No fileName find in the asset path name")
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    string encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

    bool partitionIndexToBeCalculated = true;
    bool deliveryRepositoriesToo = true;
    unsigned long cmsPartitionIndexUsed;
    
    string cmsAssetPathName = _cmsStorage->moveAssetInCMSRepository(
        stagingEncodedAssetPathName,
        _encodingItem->_customer->_directoryName,
        encodedFileName,
        _encodingItem->_relativePath,

        partitionIndexToBeCalculated,
        &cmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

        deliveryRepositoriesToo,
        _encodingItem->_customer->_territories
        );

    unsigned long long cmsAssetSizeInBytes;
    {
        FileIO::DirectoryEntryType_t detSourceFileType = 
                FileIO::getDirectoryEntryType(cmsAssetPathName);

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
                detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            string errorMessage = string("Wrong directory entry type")
                    + ", cmsAssetPathName: " + cmsAssetPathName
                    ;

            _logger->error(errorMessage);
            throw runtime_error(errorMessage);
        }
        
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            cmsAssetSizeInBytes = FileIO::getDirectorySizeInBytes(cmsAssetPathName);   
        }
        else
        {
            bool inCaseOfLinkHasItToBeRead = false;
            cmsAssetSizeInBytes = FileIO::getFileSizeInBytes(cmsAssetPathName,
                    inCaseOfLinkHasItToBeRead);   
        }
    }
    
    int64_t encodedPhysicalPathKey =
            _cmsEngineDBFacade->saveEncodedContentMetadata(
        _encodingItem->_customer->_customerKey,
        _encodingItem->_mediaItemKey,
        encodedFileName,
        _encodingItem->_relativePath,
        cmsPartitionIndexUsed,
        cmsAssetSizeInBytes,
        _encodingItem->_encodingProfileKey);
}

