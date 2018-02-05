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

#include "EncoderVideoAudioProxy.h"

EncoderVideoAudioProxy::EncoderVideoAudioProxy(
        shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
        shared_ptr<CMSStorage> cmsStorage,
        EncodingItem* pEncodingItem,
        shared_ptr<spdlog::logger> logger
)
{
    _logger                 = logger;
    _cmsEngineDBFacade      = cmsEngineDBFacade;
    _cmsStorage             = cmsStorage;
    _pEncodingItem          = pEncodingItem;
    
    _3GPPEncoder            = "FFMPEG";
    _mpeg2TSEncoder         = "FFMPEG";

}

EncoderVideoAudioProxy::~EncoderVideoAudioProxy() 
{
}

void EncoderVideoAudioProxy::operator()()
{
    try
    {
        encodeContentVideoAudio();
        processEncodedContentVideoAudio();
    }
    catch(MaxConcurrentJobsReached e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        _cmsEngineDBFacade->updateEncodingJob (_pEncodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::MaxCapacityReached, _pEncodingItem->_ingestionJobKey);
        
        throw e;
    }
    catch(EncoderError e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        _cmsEngineDBFacade->updateEncodingJob (_pEncodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError, _pEncodingItem->_ingestionJobKey);

        throw e;
    }
    catch(exception e)
    {
        _logger->error(string("encodeContentVideoAudio: ") + e.what());
        
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        _cmsEngineDBFacade->updateEncodingJob (
                _pEncodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                _pEncodingItem->_ingestionJobKey);

        throw e;
    }
            
    try
    {
        _cmsEngineDBFacade->updateEncodingJob (
            _pEncodingItem->_encodingJobKey, 
            CMSEngineDBFacade::EncodingError::NoError, 
            _pEncodingItem->_ingestionJobKey);
    }
    catch(exception e)
    {
        _logger->error(string("_cmsEngineDBFacade->updateEncodingJob failed: ") + e.what());

        throw e;
    }
}

void EncoderVideoAudioProxy::encodeContentVideoAudio()
{
    string cmsAssetPathName = _cmsStorage->getCMSAssetPathName(
        _pEncodingItem->_cmsPartitionNumber,
        _pEncodingItem->_customerDirectoryName,
        _pEncodingItem->_relativePath,
        _pEncodingItem->_fileName);
    
    if (
        (_pEncodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::3GPP &&
            _3GPPEncoder == "FFMPEG") ||
        (_pEncodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::MPEG2_TS &&
            _mpeg2TSEncoder == "FFMPEG") ||
        _pEncodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::WEBM ||
        (_pEncodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::Adobe &&
            _mpeg2TSEncoder == "FFMPEG")
    )
    {
        
    }
    else if (_pEncodingItem->_encodingProfileTechnology == CMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        
    }
    else
    {
        
    }


}

void EncoderVideoAudioProxy::processEncodedContentVideoAudio()
{
    
}
