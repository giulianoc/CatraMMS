/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   FFMPEGEncoder.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#ifndef FFMPEGEncoder_h
#define FFMPEGEncoder_h

#include "APICommon.h"
#include "FFMpeg.h"

class FFMPEGEncoder: public APICommon {
public:
    FFMPEGEncoder(const char* configurationPathName);
    
    ~FFMPEGEncoder();
    
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength);

    virtual void manageRequestAndResponse(
            shared_ptr<FCGX_Request> request,
            string requestURI,
            string requestMethod,
            unordered_map<string, string> queryParameters,
            tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
            unsigned long contentLength,
            string requestBody,
            string xCatraMMSResumeHeader
    );
    
private:
    struct Encoding
    {
        bool                    _running;
        int64_t                 _encodingJobKey;
        shared_ptr<FFMpeg>      _ffmpeg;
    };

    mutex                       _encodingMutex;
    int                         _maxEncodingsCapability;
    vector<shared_ptr<Encoding>>    _encodingsCapability;

    void encodeContent(
        shared_ptr<FCGX_Request> request,
        shared_ptr<Encoding> encoding,
        string requestBody);
};

#endif

