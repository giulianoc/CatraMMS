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
    FFMPEGEncoder(Json::Value configuration, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            mutex* fcgiAcceptMutex,
            shared_ptr<spdlog::logger> logger);
    
    ~FFMPEGEncoder();
    
    /*
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength);
    */

    virtual void manageRequestAndResponse(
            FCGX_Request& request,
            string requestURI,
            string requestMethod,
            unordered_map<string, string> queryParameters,
            bool basicAuthenticationPresent,
            tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool, bool, bool,bool,bool,bool,bool>& userKeyWorkspaceAndFlags,
			string apiKey,
            unsigned long contentLength,
            string requestBody,
            unordered_map<string, string>& requestDetails
    );
    
private:
    struct Encoding
    {
        bool                    _running;
        int64_t                 _encodingJobKey;
        shared_ptr<FFMpeg>		_ffmpeg;
		pid_t					_childPid;
    };

	// no encoding, just copying the video/audio tracks
    struct LiveRecording
    {
        bool                    _running;
        int64_t                 _encodingJobKey;
        shared_ptr<FFMpeg>      _ffmpeg;
		pid_t					_childPid;

		int64_t					_ingestionJobKey;
		Json::Value				_encodingParametersRoot;
		Json::Value				_liveRecorderParametersRoot;
        string					_transcoderStagingContentsPath;
        string					_stagingContentsPath;
        string					_segmentListFileName;
        string					_recordedFileNamePrefix;
		string					_lastRecordedAssetFileName;
    };

	struct EncodingCompleted
	{
		int64_t					_encodingJobKey;
		bool					_completedWithError;
		bool					_killedByUser;
		chrono::system_clock::time_point	_timestamp;
    };

    mutex                       _encodingMutex;
    int                         _maxEncodingsCapability;
    vector<shared_ptr<Encoding>>    _encodingsCapability;

    mutex                       _liveRecordingMutex;
    int                         _maxLiveRecordingsCapability;
    vector<shared_ptr<LiveRecording>>    _liveRecordingsCapability;
	int							_liveRecorderChunksIngestionCheckInSeconds;
	bool						_liveRecorderChunksIngestionThreadShutdown;

    mutex						_encodingCompletedMutex;
	int							_encodingCompletedRetentionInSeconds;
    map<int64_t, shared_ptr<EncodingCompleted>>		_encodingCompletedMap;
	chrono::system_clock::time_point				_lastEncodingCompletedCheck;

    string								_mmsAPIProtocol;
    string								_mmsAPIHostname;
    int									_mmsAPIPort;
    // string								_mmsAPIUser;
    // string								_mmsAPIPassword;
    string								_mmsAPIIngestionURI;

    void encodeContent(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
            int64_t encodingJobKey,
        string requestBody);
    
    void overlayImageOnVideo(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
            int64_t encodingJobKey,
        string requestBody);

    void overlayTextOnVideo(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

    void generateFrames(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

    void slideShow(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void liveRecorder(
        // FCGX_Request& request,
        shared_ptr<LiveRecording> liveRecording,
        int64_t encodingJobKey,
        string requestBody);
	void liveRecorderChunksIngestionThread();
	string liveRecorder_processLastGeneratedLiveRecorderFiles(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		bool highAvailability, bool main, int segmentDurationInSeconds, string outputFileFormat,                                                                              
		Json::Value liveRecorderParametersRoot,
		string transcoderStagingContentsPath,
		string stagingContentsPath,
		string segmentListFileName,
		string recordedFileNamePrefix,
		string lastRecordedAssetFileName);
	time_t liveRecorder_getMediaLiveRecorderStartTime(int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName);
	time_t liveRecorder_getMediaLiveRecorderEndTime(int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName);
	bool liveRecorder_isLastLiveRecorderFile(int64_t ingestionJobKey, int64_t encodingJobKey,
			time_t currentRecordedFileCreationTime, string transcoderStagingContentsPath,
			string recordedFileNamePrefix);
	void liveRecorder_ingestRecordedMedia(
		int64_t ingestionJobKey,
		string transcoderStagingContentsPath, string currentRecordedAssetFileName,
		string stagingContentsPath,
		string title,
		Json::Value userDataRoot,
		string fileFormat,
		Json::Value liveRecorderParametersRoot);

	void videoSpeed(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void pictureInPicture(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void addEncodingCompleted(
        int64_t encodingJobKey, bool completedWithError,
		bool killedByUser);

	void removeEncodingCompletedIfPresent(int64_t encodingJobKey);

	void encodingCompletedRetention();
};

#endif

