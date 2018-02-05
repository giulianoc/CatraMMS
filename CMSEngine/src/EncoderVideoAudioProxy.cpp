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
    EncodingItem* pEncodingItem,
    shared_ptr<spdlog::logger> logger) 
{
    _logger                 = logger;
    _cmsEngineDBFacade      = cmsEngineDBFacade;
    _pEncodingItem          = pEncodingItem;
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
    
}

void EncoderVideoAudioProxy::processEncodedContentVideoAudio()
{
    
}
