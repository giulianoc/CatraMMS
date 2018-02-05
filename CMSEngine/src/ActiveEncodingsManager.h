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

#ifndef ActiveEncodingsManager_h
#define ActiveEncodingsManager_h

#include <chrono>
#include <vector>
#include <condition_variable>
#include "CMSEngineDBFacade.h"
#include "EncodingItem.h"
#include "spdlog/spdlog.h"

class ActiveEncodingsManager {
public:
    ActiveEncodingsManager(shared_ptr<spdlog::logger> logger);

    virtual ~ActiveEncodingsManager();
    
    void operator ()();

private:
    struct EncodingJob
    {
        EncodingJobStatus			_status;
        chrono::system_clock::time_point	_encodingJobStart;

        EncodingItem				_encodingItem;
        void					*_encodingThread;
    };

    shared_ptr<spdlog::logger>                  _logger;
    
    condition_variable                          _cvAddedEncodingJob;
    mutex                                       _mtEncodingJobs;
    
    vector<EncodingJob>                        _highPriorityEncodingJobs;
    vector<EncodingJob>                        _defaultPriorityEncodingJobs;
    vector<EncodingJob>                        _lowPriorityEncodingJobs;

    void processEncodingJob(EncodingJob encodingJob);
};

#endif

