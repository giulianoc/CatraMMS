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

#ifndef FFMpegEncodingParameters_h
#define FFMpegEncodingParameters_h

#include <string>
#include "spdlog/spdlog.h"
#include "json/json.h"

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

using namespace std;

class FFMpegEncodingParameters {
private:
    shared_ptr<spdlog::logger>  _logger;

	string					_ffmpegTempDir;
	string					_multiTrackTemplateVariable;
	string					_multiTrackTemplatePart;

	int64_t					_ingestionJobKey;
	int64_t					_encodingJobKey;
	string					_encodedStagingAssetPathName;
	bool					_isVideo;
	Json::Value				_videoTracksRoot;
	Json::Value				_audioTracksRoot;
	int						_videoTrackIndexToBeUsed;
	int						_audioTrackIndexToBeUsed;

	string					_ffmpegHttpStreamingParameter;

	string					_ffmpegFileFormatParameter;

	string					_ffmpegVideoCodecParameter;
	string					_ffmpegVideoProfileParameter;
	string					_ffmpegVideoOtherParameters;
	string					_ffmpegVideoFrameRateParameter;
	string					_ffmpegVideoKeyFramesRateParameter;
	vector<tuple<string, int, int, int, string, string, string>>			_videoBitRatesInfo;

	string					_ffmpegAudioCodecParameter;
	string					_ffmpegAudioOtherParameters;
	string					_ffmpegAudioChannelsParameter;
	string					_ffmpegAudioSampleRateParameter;
	vector<string>			_audioBitRatesInfo;

	string getManifestFileName();
	string getMultiTrackTemplatePart();
	string getMultiTrackEncodedStagingTemplateAssetPathName();

public:
	string					_httpStreamingFileFormat;    

    FFMpegEncodingParameters(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string encodedStagingAssetPathName,
		Json::Value encodingProfileDetailsRoot,
		bool isVideo,   // if false it means is audio
		Json::Value videoTracksRoot,
		Json::Value audioTracksRoot,
		int videoTrackIndexToBeUsed,
		int audioTrackIndexToBeUsed,

		bool& twoPasses,	// out

		string ffmpegTempDir,
		shared_ptr<spdlog::logger> logger);
    
    ~FFMpegEncodingParameters();

	void applyEncoding(
		// -1: NO two passes
		// 0: YES two passes, first step
		// 1: YES two passes, second step
		int stepNumber,

		// out (in append)
		vector<string>& ffmpegArgumentList
	);

	void createManifestFile();

    void removeTwoPassesTemporaryFiles();

	bool getMultiTrackPathNames(vector<string>& sourcesPathName);
	void removeMultiTrackPathNames();

	void applyEncoding_audioGroup(
		// -1: NO two passes
		// 0: YES two passes, first step
		// 1: YES two passes, second step
		int stepNumber,

		// out (in append)
		vector<string>& ffmpegArgumentList);

	void createManifestFile_audioGroup();

	static void settingFfmpegParameters(
		shared_ptr<spdlog::logger> logger,
		Json::Value encodingProfileDetailsRoot,
		bool isVideo,   // if false it means is audio
        
		string& httpStreamingFileFormat,
		string& ffmpegHttpStreamingParameter,

		string& ffmpegFileFormatParameter,

		string& ffmpegVideoCodecParameter,
		string& ffmpegVideoProfileParameter,
		string& ffmpegVideoOtherParameters,
		bool& twoPasses,
		string& ffmpegVideoFrameRateParameter,
		string& ffmpegVideoKeyFramesRateParameter,
		vector<tuple<string, int, int, int, string, string, string>>& videoBitRatesInfo,

		string& ffmpegAudioCodecParameter,
		string& ffmpegAudioOtherParameters,
		string& ffmpegAudioChannelsParameter,
		string& ffmpegAudioSampleRateParameter,
		vector<string>& audioBitRatesInfo
	);

	static void addToArguments(string parameter, vector<string>& argumentList);

	static void encodingFileFormatValidation(string fileFormat,
		shared_ptr<spdlog::logger> logger);

    static void encodingAudioCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger);

    static void encodingVideoProfileValidation(
        string codec, string profile,
        shared_ptr<spdlog::logger> logger);

};

#endif

