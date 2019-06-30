/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EncodersLoadBalancer.cpp
 * Author: giuliano
 * 
 * Created on April 28, 2018, 2:33 PM
 */

#include "EncodersLoadBalancer.h"

EncodersLoadBalancer::EncodersLoadBalancer(
    Json::Value configuration,
    shared_ptr<spdlog::logger> logger
)
{
    _logger             = logger;
    
    Json::Value encodersPools = configuration["ffmpeg"]["hosts"];
    
    for (auto const& encodersPoolName : encodersPools.getMemberNames())
    {
        _logger->info(__FILEREF__ + "encodersPools"
            + ", encodersPoolName: " + encodersPoolName
        );

        EncodersPoolDetails encodersPoolDetails;
        
        encodersPoolDetails._lastEncoderUsed = -1;
        
        Json::Value encodersPool = encodersPools[encodersPoolName];
        
        for (int encoderPoolIndex = 0; encoderPoolIndex < encodersPool.size(); encoderPoolIndex++)
        {
            string encoderHostName = encodersPool[encoderPoolIndex].asString();
            
            _logger->info(__FILEREF__ + "encodersPool"
                + ", encoderHostName: " + encoderHostName
            );
        
            encodersPoolDetails._encoders.push_back(encoderHostName);
        }
        
        _encodersPools[encodersPoolName] = encodersPoolDetails;
    }
}

EncodersLoadBalancer::~EncodersLoadBalancer() {
}

string EncodersLoadBalancer::getEncoderHost(shared_ptr<Workspace> workspace, string encoderToSkip) 
{
    string defaultEncodersPool = "common";

    map<string, EncodersPoolDetails>::iterator it = _encodersPools.find(workspace->_directoryName);
    if (it == _encodersPools.end())
        it = _encodersPools.find(defaultEncodersPool);

    if (it == _encodersPools.end())
    {
        string errorMessage = "No encoders pools found";
        _logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);                    
    }
    
    it->second._lastEncoderUsed     = (it->second._lastEncoderUsed + 1) % it->second._encoders.size();
	if (encoderToSkip != "" && encoderToSkip == it->second._encoders[it->second._lastEncoderUsed])
		it->second._lastEncoderUsed     = (it->second._lastEncoderUsed + 1) % it->second._encoders.size();        

    return it->second._encoders[it->second._lastEncoderUsed];
}
