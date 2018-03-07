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
#include <sstream>
#ifdef __LOCALENCODER__
#else
    #include <curlpp/cURLpp.hpp>
    #include <curlpp/Easy.hpp>
    #include <curlpp/Options.hpp>
    #include <curlpp/Exception.hpp>
    #include <curlpp/Infos.hpp>
#endif
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/Convert.h"
#include "EncoderVideoAudioProxy.h"


EncoderVideoAudioProxy::EncoderVideoAudioProxy()
{
}

EncoderVideoAudioProxy::~EncoderVideoAudioProxy() 
{
}

void EncoderVideoAudioProxy::setData(
        Json::Value configuration,
        mutex* mtEncodingJobs,
        EncodingJobStatus* status,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem,
        #ifdef __LOCALENCODER__
            int* pRunningEncodingsNumber,
        #endif
        shared_ptr<spdlog::logger> logger
)
{
    _logger                 = logger;
    _configuration          = configuration;
    
    _mtEncodingJobs         = mtEncodingJobs;
    _status                 = status;
    
    _mmsEngineDBFacade      = mmsEngineDBFacade;
    _mmsStorage             = mmsStorage;
    _encodingItem           = encodingItem;
    
    _mp4Encoder             = _configuration["encoding"].get("mp4Encoder", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->mp4Encoder: " + _mp4Encoder
    );
    _mpeg2TSEncoder         = _configuration["encoding"].get("mpeg2TSEncoder", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->mpeg2TSEncoder: " + _mpeg2TSEncoder
    );
        
    #ifdef __LOCALENCODER__
        _ffmpegMaxCapacity      = 1;
        
        _pRunningEncodingsNumber  = pRunningEncodingsNumber;
        
        _ffmpeg = make_shared<FFMpeg>(configuration, logger);
    #endif
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
        _logger->warn(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob MaxCapacityReached"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::MaxCapacityReached, _encodingItem->_ingestionJobKey);
        
        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    catch(EncoderError e)
    {
        _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what());
        
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
            
    try
    {
        processEncodedContentVideoAudio(stagingEncodedAssetPathName);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processEncodedContentVideoAudio: " + e.what());
        
        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

        _logger->error(__FILEREF__ + "Remove"
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

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }

    try
    {
        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob NoError"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        );
        
        _mmsEngineDBFacade->updateEncodingJob (
            _encodingItem->_encodingJobKey, 
            MMSEngineDBFacade::EncodingError::NoError, 
            _encodingItem->_ingestionJobKey);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob failed: " + e.what());

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }
        
        // throw e;
        return;
    }
    
    {
        lock_guard<mutex> locker(*_mtEncodingJobs);

        *_status = EncodingJobStatus::Free;
    }        
}

string EncoderVideoAudioProxy::encodeContentVideoAudio()
{
    string stagingEncodedAssetPathName;
    
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
        + ", _encodingItem->_encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
        + ", _mp4Encoder: " + _mp4Encoder
    );

    if (
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4 &&
            _mp4Encoder == "FFMPEG") ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS &&
            _mpeg2TSEncoder == "FFMPEG") ||
        _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe &&
            _mpeg2TSEncoder == "FFMPEG")
    )
    {
        stagingEncodedAssetPathName = encodeContent_VideoAudio_through_ffmpeg();
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode";
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    return stagingEncodedAssetPathName;
}

int EncoderVideoAudioProxy::getEncodingProgress(int64_t encodingJobKey)
{
    int encodingProgress = 0;
    
    #ifdef __LOCALENCODER__
        try
        {
            encodingProgress = _ffmpeg->getEncodingProgress();
        }
        catch(FFMpegEncodingStatusNotAvailable e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->getEncodingProgress failed"
                + ", e.what(): " + e.what()
            );

            throw EncodingStatusNotAvailable();
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->getEncodingProgress failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
    #else
        string ffmpegEncoderURL;
        ostringstream response;
        try
        {
            // string ffmpegEncoderHost = _configuration["ffmpeg"].get("encoderHost", "").asString();
            int ffmpegEncoderPort = _configuration["ffmpeg"].get("encoderPort", "").asInt();
            _logger->info(__FILEREF__ + "Configuration item"
                + ", ffmpeg->encoderPort: " + to_string(ffmpegEncoderPort)
            );
            string ffmpegEncoderURI = _configuration["ffmpeg"].get("encoderURI", "").asString();
            _logger->info(__FILEREF__ + "Configuration item"
                + ", ffmpeg->encoderURI: " + ffmpegEncoderURI
            );
            ffmpegEncoderURL = 
                    string("http://")
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(ffmpegEncoderPort)
                    + ffmpegEncoderURI
                    + "/" + to_string(encodingJobKey)
            ;
            
            list<string> header;

            {
                string encoderUser = _configuration["ffmpeg"].get("encoderUser", "").asString();
                _logger->info(__FILEREF__ + "Configuration item"
                    + ", ffmpeg->encoderUser: " + encoderUser
                );
                string encoderPassword = _configuration["ffmpeg"].get("encoderPassword", "").asString();
                _logger->info(__FILEREF__ + "Configuration item"
                    + ", ffmpeg->encoderPassword: " + "..."
                );
                string userPasswordEncoded = Convert::base64_encode(encoderUser + ":" + encoderPassword);
                string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

                header.push_back(basicAuthorization);
            }
            
            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

            request.setOpt(new curlpp::options::HttpHeader(header));

            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Encoding media file"
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
            );
            request.perform();
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            _logger->info(__FILEREF__ + "Encoded media file"
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
            );
            
            try
            {
                Json::Value encodeProgressResponse;
                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(response.str().c_str(),
                        response.str().c_str() + response.str().size(), 
                        &encodeProgressResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", response.str(): " + response.str();
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                encodingProgress = encodeProgressResponse.get("encodingProgress", "XXX").asInt();
            }
            catch(...)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", response.str(): " + response.str()
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Progress URL failed (LogicError)"
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );
            
            throw e;
        }
        catch (curlpp::RuntimeError & e) 
        {
            _logger->error(__FILEREF__ + "Progress URL failed (RuntimeError)"
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Progress URL failed (exception)"
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Progress URL failed (exception)"
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
    #endif

    return encodingProgress;
}

string EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg()
{
    
    string stagingEncodedAssetPathName;
    string encodedFileName;
    string mmsSourceAssetPathName;

    
    #ifdef __LOCALENCODER__
        if (*_pRunningEncodingsNumber > _ffmpegMaxCapacity)
        {
            _logger->info("Max ffmpeg encoder capacity is reached");

            throw MaxConcurrentJobsReached();
        }
    #endif

    // stagingEncodedAssetPathName preparation
    {
        mmsSourceAssetPathName = _mmsStorage->getMMSAssetPathName(
            _encodingItem->_mmsPartitionNumber,
            _encodingItem->_customer->_directoryName,
            _encodingItem->_relativePath,
            _encodingItem->_fileName);

        size_t extensionIndex = _encodingItem->_fileName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                    + ", _encodingItem->_fileName: " + _encodingItem->_fileName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName =
                _encodingItem->_fileName.substr(0, extensionIndex)
                + "_" 
                + to_string(_encodingItem->_encodingProfileKey);
        if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4)
            encodedFileName.append(".mp4");
        else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
                _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe)
            ;
        else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM)
            encodedFileName.append(".webm");
        else
        {
            string errorMessage = __FILEREF__ + "Unknown technology";
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool removeLinuxPathIfExist = true;
        stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
            _encodingItem->_customer->_directoryName,
            _encodingItem->_relativePath,
            encodedFileName,
            -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
            -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
            removeLinuxPathIfExist);

        if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
            _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe)
        {
            // In this case, the path is a directory where to place the segments

            if (!FileIO::directoryExisting(stagingEncodedAssetPathName)) 
            {
                _logger->info(__FILEREF__ + "Create directory"
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
    }

    #ifdef __LOCALENCODER__
        (*_pRunningEncodingsNumber)++;

        try
        {
            _ffmpeg->encodeContent(
                mmsSourceAssetPathName,
                _encodingItem->_durationInMilliSeconds,
                encodedFileName,
                stagingEncodedAssetPathName,
                _encodingItem->_details,
                _encodingItem->_contentType == MMSEngineDBFacade::ContentType::Video,
                _encodingItem->_physicalPathKey,
                _encodingItem->_customer->_directoryName,
                _encodingItem->_relativePath,
                _encodingItem->_encodingJobKey,
                _encodingItem->_ingestionJobKey);

            (*_pRunningEncodingsNumber)++;
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->encodeContent failed"
                + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
                + ", encodedFileName: " + encodedFileName
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _encodingItem->_details: " + _encodingItem->_details
                + ", _encodingItem->_contentType: " + MMSEngineDBFacade::toString(_encodingItem->_contentType)
                + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            );
            
            (*_pRunningEncodingsNumber)++;
            
            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->encodeContent failed"
                + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
                + ", encodedFileName: " + encodedFileName
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _encodingItem->_details: " + _encodingItem->_details
                + ", _encodingItem->_contentType: " + MMSEngineDBFacade::toString(_encodingItem->_contentType)
                + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            );
            
            (*_pRunningEncodingsNumber)++;
            
            throw e;
        }
    #else
        string ffmpegEncoderURL;
        ostringstream response;
        try
        {
            string ffmpegEncoderHost = _configuration["ffmpeg"].get("encoderHost", "").asString();
            _logger->info(__FILEREF__ + "Configuration item"
                + ", ffmpeg->encoderHost: " + ffmpegEncoderHost
            );
            int ffmpegEncoderPort = _configuration["ffmpeg"].get("encoderPort", "").asInt();
            _logger->info(__FILEREF__ + "Configuration item"
                + ", ffmpeg->encoderPort: " + to_string(ffmpegEncoderPort)
            );
            string ffmpegEncoderURI = _configuration["ffmpeg"].get("encoderURI", "").asString();
            _logger->info(__FILEREF__ + "Configuration item"
                + ", ffmpeg->encoderURI: " + ffmpegEncoderURI
            );
            ffmpegEncoderURL = 
                    string("http://")
                    + ffmpegEncoderHost + ":"
                    + to_string(ffmpegEncoderPort)
                    + ffmpegEncoderURI
            ;
            string body;
            {
                Json::Value encodingMedatada;
                
                encodingMedatada["mmsSourceAssetPathName"] = mmsSourceAssetPathName;
                encodingMedatada["durationInMilliSeconds"] = (Json::LargestUInt) (_encodingItem->_durationInMilliSeconds);
                encodingMedatada["encodedFileName"] = encodedFileName;
                encodingMedatada["stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
                Json::Value encodingDetails;
                {
                    try
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(_encodingItem->_details.c_str(),
                                _encodingItem->_details.c_str() + _encodingItem->_details.size(), 
                                &encodingDetails, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse the _encodingItem->_details"
                                    + ", _encodingItem->_details: " + _encodingItem->_details;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                    catch(...)
                    {
                        string errorMessage = string("_encodingItem->_details json is not well format")
                                + ", _encodingItem->_details: " + _encodingItem->_details
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                encodingMedatada["encodingProfileDetails"] = encodingDetails;
                encodingMedatada["contentType"] = MMSEngineDBFacade::toString(_encodingItem->_contentType);
                encodingMedatada["physicalPathKey"] = (Json::LargestUInt) (_encodingItem->_physicalPathKey);
                encodingMedatada["customerDirectoryName"] = _encodingItem->_customer->_directoryName;
                encodingMedatada["relativePath"] = _encodingItem->_relativePath;
                encodingMedatada["encodingJobKey"] = (Json::LargestUInt) (_encodingItem->_encodingJobKey);
                encodingMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);

                {
                    Json::StreamWriterBuilder wbuilder;

                    body = Json::writeString(wbuilder, encodingMedatada);
                }
            }
            
            list<string> header;

            header.push_back("Content-Type: application/json");
            {
                string encoderUser = _configuration["ffmpeg"].get("encoderUser", "").asString();
                _logger->info(__FILEREF__ + "Configuration item"
                    + ", ffmpeg->encoderUser: " + encoderUser
                );
                string encoderPassword = _configuration["ffmpeg"].get("encoderPassword", "").asString();
                _logger->info(__FILEREF__ + "Configuration item"
                    + ", ffmpeg->encoderPassword: " + "..."
                );
                string userPasswordEncoded = Convert::base64_encode(encoderUser + ":" + encoderPassword);
                string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

                header.push_back(basicAuthorization);
            }
            
            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

            request.setOpt(new curlpp::options::HttpHeader(header));
            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Encoding media file"
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
            request.perform();
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            _logger->info(__FILEREF__ + "Encoded media file"
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
            );
            
            try
            {
                Json::Value encodeContentResponse;
                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(response.str().c_str(),
                        response.str().c_str() + response.str().size(), 
                        &encodeContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", response.str(): " + response.str();
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                _currentUsedFFMpegEncoderHost = encodeContentResponse.get("ffmpegEncoderHost", "XXX").asString();
            }
            catch(...)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", response.str(): " + response.str()
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                // encoding is finished, no exception is raised in case the response is not parsed
                // throw runtime_error(errorMessage);
            }
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );
            
            throw e;
        }
        catch (curlpp::RuntimeError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (exception)"
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
    #endif

    return stagingEncodedAssetPathName;
}

void EncoderVideoAudioProxy::processEncodedContentVideoAudio(string stagingEncodedAssetPathName)
{
    string encodedFileName;
    string mmsAssetPathName;
    unsigned long mmsPartitionIndexUsed;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        bool partitionIndexToBeCalculated = true;
        bool deliveryRepositoriesToo = true;

        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            _encodingItem->_customer->_directoryName,
            encodedFileName,
            _encodingItem->_relativePath,

            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

            deliveryRepositoriesToo,
            _encodingItem->_customer->_territories
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        throw e;
    }

    try
    {
        unsigned long long mmsAssetSizeInBytes;
        {
            FileIO::DirectoryEntryType_t detSourceFileType = 
                    FileIO::getDirectoryEntryType(mmsAssetPathName);

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
                    detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                string errorMessage = __FILEREF__ + "Wrong directory entry type"
                        + ", mmsAssetPathName: " + mmsAssetPathName
                        ;

                _logger->error(errorMessage);
                throw runtime_error(errorMessage);
            }

            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                mmsAssetSizeInBytes = FileIO::getDirectorySizeInBytes(mmsAssetPathName);   
            }
            else
            {
                bool inCaseOfLinkHasItToBeRead = false;
                mmsAssetSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                        inCaseOfLinkHasItToBeRead);   
            }
        }


        int64_t encodedPhysicalPathKey = _mmsEngineDBFacade->saveEncodedContentMetadata(
            _encodingItem->_customer->_customerKey,
            _encodingItem->_mediaItemKey,
            encodedFileName,
            _encodingItem->_relativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
            _encodingItem->_encodingProfileKey);
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveEncodedContentMetadata failed"
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Remove"
            + ", mmsAssetPathName: " + mmsAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(mmsAssetPathName);
        }

        throw e;
    }
}
