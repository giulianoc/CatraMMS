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
    shared_ptr<spdlog::logger> logger) 
{
    _logger = logger;
}

EncoderVideoAudioProxy::~EncoderVideoAudioProxy() 
{
}

void EncoderVideoAudioProxy::operator()()
{
    try
    {
        encodeContentVideoAudio();
    }
    catch(MaxJobsReached e)
    {
        _cmsEngineDBFacade->updateEncoderJob (encoderJobKey, CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
        
    }
}
