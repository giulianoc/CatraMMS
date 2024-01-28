/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   FFMPEGFilters.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#ifndef FFMpegFilters_h
#define FFMpegFilters_h

#include <string>
#include <filesystem>
#include <chrono>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "json/json.h"


using namespace std;

class FFMpegFilters {
public:

	FFMpegFilters(string ffmpegTtfFontDir) ;

	~FFMpegFilters();

	tuple<string, string, string> addFilters(
		Json::Value filtersRoot,
		string ffmpegVideoResolutionParameter,
		string ffmpegDrawTextFilter,
		int64_t streamingDurationInSeconds);

	string addVideoFilters(
		Json::Value filtersRoot,
		string ffmpegVideoResolutionParameter,
		string ffmpegDrawTextFilter,
		int64_t streamingDurationInSeconds);

	string addAudioFilters(
		Json::Value filtersRoot,
		int64_t streamingDurationInSeconds);

	string getFilter(
		Json::Value filtersRoot,
		int64_t streamingDurationInSeconds);

private:
	string		_ffmpegTtfFontDir;
};

#endif

