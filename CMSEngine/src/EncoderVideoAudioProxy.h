/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EnodingsManager.h
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#ifndef EncoderVideoAudioProxy_h
#define EncoderVideoAudioProxy_h

#include "spdlog/spdlog.h"

class EncoderVideoAudioProxy {
public:
    EncoderVideoAudioProxy(shared_ptr<spdlog::logger> logger);

    virtual ~EncoderVideoAudioProxy();
    
    void operator ()();

private:
};

#endif

