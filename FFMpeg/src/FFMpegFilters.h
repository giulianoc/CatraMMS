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
#include "nlohmann/json.hpp"


using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

class FFMpegFilters {
public:

	FFMpegFilters(string ffmpegTtfFontDir) ;

	~FFMpegFilters();

	tuple<string, string, string> addFilters(
		json filtersRoot,
		string ffmpegVideoResolutionParameter,
		string ffmpegDrawTextFilter,
		int64_t streamingDurationInSeconds);

	string addVideoFilters(
		json filtersRoot,
		string ffmpegVideoResolutionParameter,
		string ffmpegDrawTextFilter,
		int64_t streamingDurationInSeconds);

	string addAudioFilters(
		json filtersRoot,
		int64_t streamingDurationInSeconds);

	string getFilter(
		json filtersRoot,
		int64_t streamingDurationInSeconds);

private:
	string		_ffmpegTtfFontDir;
};

#endif

