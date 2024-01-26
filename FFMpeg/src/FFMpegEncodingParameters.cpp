/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   FFMPEGEncoder.cpp
 * Author: giuliano
 * 
 * Created on February 18, 2018, 1:27 AM
 */
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include <regex>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

FFMpegEncodingParameters::FFMpegEncodingParameters(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	Json::Value encodingProfileDetailsRoot,
	bool isVideo,   // if false it means is audio
	int videoTrackIndexToBeUsed,
	int audioTrackIndexToBeUsed,
	string encodedStagingAssetPathName,
	Json::Value videoTracksRoot,	// used only in case of _audioGroup
	Json::Value audioTracksRoot,	// used only in case of _audioGroup

	bool& twoPasses,	// out

	string ffmpegTempDir,
	string ffmpegTtfFontDir,
	shared_ptr<spdlog::logger> logger) 
{
    _logger             = logger;

    _ffmpegTempDir		= ffmpegTempDir;
	_ffmpegTtfFontDir	= ffmpegTtfFontDir;

	_multiTrackTemplateVariable = "__HEIGHT__";
	_multiTrackTemplatePart = _multiTrackTemplateVariable + "p";

	_ingestionJobKey				= ingestionJobKey;
	_encodingJobKey					= encodingJobKey;
	_encodedStagingAssetPathName	= encodedStagingAssetPathName;
	_isVideo						= isVideo;
	_videoTracksRoot				= videoTracksRoot;
	_audioTracksRoot				= audioTracksRoot;
	_videoTrackIndexToBeUsed		= videoTrackIndexToBeUsed;
	_audioTrackIndexToBeUsed		= audioTrackIndexToBeUsed;

	_initialized = false;
	twoPasses = false;
	if (encodingProfileDetailsRoot == Json::nullValue)
		return;

    try
    {
		_httpStreamingFileFormat = "";
		_ffmpegHttpStreamingParameter = "";

		_ffmpegFileFormatParameter = "";

		_ffmpegVideoCodecParameter = "";
		_ffmpegVideoProfileParameter = "";
		_ffmpegVideoOtherParameters = "";
		_ffmpegVideoFrameRateParameter = "";
		_ffmpegVideoKeyFramesRateParameter = "";
		_videoBitRatesInfo.clear();

		_ffmpegAudioCodecParameter = "";
		_ffmpegAudioOtherParameters = "";
		_ffmpegAudioChannelsParameter = "";
		_ffmpegAudioSampleRateParameter = "";
		_audioBitRatesInfo.clear();

		FFMpegEncodingParameters::settingFfmpegParameters(
			_logger,
			encodingProfileDetailsRoot,
			_isVideo,

			_httpStreamingFileFormat,
			_ffmpegHttpStreamingParameter,

			_ffmpegFileFormatParameter,

			_ffmpegVideoCodecParameter,
			_ffmpegVideoProfileParameter,
			_ffmpegVideoOtherParameters,
			twoPasses,
			_ffmpegVideoFrameRateParameter,
			_ffmpegVideoKeyFramesRateParameter,
			_videoBitRatesInfo,

			_ffmpegAudioCodecParameter,
			_ffmpegAudioOtherParameters,
			_ffmpegAudioChannelsParameter,
			_ffmpegAudioSampleRateParameter,
			_audioBitRatesInfo
		);

		_initialized = true;
    }
    catch(runtime_error& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: init failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
	}
	catch(exception& e)
	{
		_logger->error(__FILEREF__ + "FFMpeg: init failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
	}
}

FFMpegEncodingParameters::~FFMpegEncodingParameters() 
{
    
}

void FFMpegEncodingParameters::applyEncoding(
	// -1: NO two passes
	// 0: YES two passes, first step
	// 1: YES two passes, second step
	int stepNumber,

	// In alcuni casi i parametro del file di output non deve essere aggiunto, ad esempio
	// per il LiveRecorder o LiveProxy o nei casi in cui il file di output viene deciso
	// dal chiamante senza seguire il fileFormat dell'encoding profile
	// Nel caso outputFileToBeAdded sia false, stepNumber has to be -1 (NO two passes)
	bool outputFileToBeAdded,

	// La risoluzione di un video viene gestita tramite un filtro video.
	// Il problema è che non possiamo avere due filtri video (-vf) in un comando.
	// Cosi', se un filtro video è stato già aggiunto al comando, non è possibile aggiungere qui
	// un ulteriore filtro video per configurare la risoluzione di un video.
	// Per cui, se abbiamo già un filtro video nel comando, la risoluzione deve essere inizializzata nel filtro
	// e quindi applyEncoding non la aggiunge
	bool videoResolutionToBeAdded,

	Json::Value filtersRoot,

	// out (in append)
	vector<string>& ffmpegArgumentList
)
{
	try
	{
		if (!_initialized)
		{
			string errorMessage = string("FFMpegEncodingParameters is not initialized")
				+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			;
			throw runtime_error(errorMessage);
		}

		FFMpegFilters ffmpegFilters(_ffmpegTtfFontDir);

		if (_httpStreamingFileFormat != "")
		{
			// hls or dash output

			string manifestFileName;
			string segmentTemplateDirectory;
			string segmentTemplatePathFileName;
			string stagingTemplateManifestAssetPathName;

			if (outputFileToBeAdded)
			{	
				manifestFileName = getManifestFileName();
				if (_httpStreamingFileFormat == "hls")
				{
					segmentTemplateDirectory =
						_encodedStagingAssetPathName + "/" + _multiTrackTemplatePart;

					segmentTemplatePathFileName =
						segmentTemplateDirectory 
						+ "/"
						+ to_string(_ingestionJobKey)
						+ "_"
						+ to_string(_encodingJobKey)
						+ "_%04d.ts"
					;
				}

				stagingTemplateManifestAssetPathName =
					segmentTemplateDirectory
					+ "/"
					+ manifestFileName;
			}

			if (stepNumber == 0 || stepNumber == 1)
			{
				// YES two passes

				// check su if (outputFileToBeAdded) è inutile, con 2 passi outputFileToBeAdded deve essere true

				// used also in removeTwoPassesTemporaryFiles
				string prefixPasslogFileName = 
					to_string(_ingestionJobKey)
					+ "_"
					+ to_string(_encodingJobKey)
				;
                string ffmpegTemplatePassLogPathFileName = _ffmpegTempDir
					+ "/"
					+ prefixPasslogFileName
					+ "_" + _multiTrackTemplatePart + ".passlog";
				;

				if (stepNumber == 0)	// YES two passes, first step
				{
					for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= _videoBitRatesInfo [videoIndex];

						string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						int videoHeight = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegAudioBitRateParameter = "";

						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
							ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
							ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						if (_videoTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:v:") + to_string(_videoTrackIndexToBeUsed));
						}
						if (_audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(_audioTrackIndexToBeUsed));
						}
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
						if (videoResolutionToBeAdded)
						{
							if (filtersRoot == Json::nullValue)
								FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
									ffmpegArgumentList);
							else
							{
								string videoFilters = ffmpegFilters.addVideoFilters(filtersRoot, ffmpegVideoResolutionParameter,
									"", -1);

								if (videoFilters != "")
									FFMpegEncodingParameters::addToArguments(string("-filter:v ") + videoFilters,
										ffmpegArgumentList);
								else
									FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
										ffmpegArgumentList);
							}
						}

						// It should be useless to add the audio parameters in phase 1 but,
						// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
						//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
						//  So, this is the reason, I'm adding phase 2 as well
						// + "-an "    // disable audio
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						if (_audioBitRatesInfo.size() > videoIndex)
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[videoIndex];
						else 
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[
								_audioBitRatesInfo.size() - 1];
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
						if (filtersRoot != Json::nullValue)
						{
							string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

							if (audioFilters != "")
								FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
									ffmpegArgumentList);
						}

						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						ffmpegArgumentList.push_back("-pass");
						ffmpegArgumentList.push_back("1");
						ffmpegArgumentList.push_back("-passlogfile");
						{
							string ffmpegPassLogPathFileName =
								regex_replace(ffmpegTemplatePassLogPathFileName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
						}

						// 2020-01-20: I removed the hls file format parameter
						//	because it was not working and added -f mp4.
						//	At the end it has to generate just the log file
						//	to be used in the second step
						// FFMpegEncodingParameters::addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);
						//
						// FFMpegEncodingParameters::addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
						ffmpegArgumentList.push_back("-f");
						// 2020-08-21: changed from mp4 to null
						ffmpegArgumentList.push_back("null");

						ffmpegArgumentList.push_back("/dev/null");
					}
				}
				else if (stepNumber == 1)	// YES two passes, second step
				{
					for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= _videoBitRatesInfo [videoIndex];

						string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						int videoHeight = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegAudioBitRateParameter = "";

						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
							ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
							ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						{
							string segmentDirectory = regex_replace(segmentTemplateDirectory,
								regex(_multiTrackTemplateVariable), to_string(videoHeight));

							_logger->info(__FILEREF__ + "Creating directory"
								+ ", : " + segmentDirectory
							);
							fs::create_directories(segmentDirectory);
							fs::permissions(segmentDirectory,
								fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
								| fs::perms::group_read | fs::perms::group_exec
								| fs::perms::others_read | fs::perms::others_exec,
								fs::perm_options::replace);
						}

						if (_videoTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:v:") + to_string(_videoTrackIndexToBeUsed));
						}
						if (_audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(_audioTrackIndexToBeUsed));
						}
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
						if (videoResolutionToBeAdded)
						{
							if (filtersRoot == Json::nullValue)
								FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
									ffmpegArgumentList);
							else
							{
								string videoFilters = ffmpegFilters.addVideoFilters(filtersRoot, ffmpegVideoResolutionParameter,
									"", -1);

								if (videoFilters != "")
									FFMpegEncodingParameters::addToArguments(string("-filter:v ") + videoFilters,
										ffmpegArgumentList);
								else
									FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
										ffmpegArgumentList);
							}
						}

						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						if (_audioBitRatesInfo.size() > videoIndex)
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[videoIndex];
						else 
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[
								_audioBitRatesInfo.size() - 1];
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
						if (filtersRoot != Json::nullValue)
						{
							string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

							if (audioFilters != "")
								FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
									ffmpegArgumentList);
						}

						FFMpegEncodingParameters::addToArguments(_ffmpegHttpStreamingParameter, ffmpegArgumentList);

						if (_httpStreamingFileFormat == "hls")
						{
							string segmentPathFileName =
								regex_replace(segmentTemplatePathFileName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back("-hls_segment_filename");
							ffmpegArgumentList.push_back(segmentPathFileName);
						}

						{
							string stagingManifestAssetPathName =
								regex_replace(stagingTemplateManifestAssetPathName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
							ffmpegArgumentList.push_back(stagingManifestAssetPathName);
						}

						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						ffmpegArgumentList.push_back("-pass");
						ffmpegArgumentList.push_back("2");
						ffmpegArgumentList.push_back("-passlogfile");
						{
							string ffmpegPassLogPathFileName =
								regex_replace(ffmpegTemplatePassLogPathFileName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
						}
					}
				}
			}
			else	// NO two passes
			{
				for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size(); videoIndex++)
				{
					tuple<string, int, int, int, string, string, string> videoBitRateInfo
						= _videoBitRatesInfo [videoIndex];

					string ffmpegVideoResolutionParameter = "";
					int videoBitRateInKbps = -1;
					int videoHeight = -1;
					string ffmpegVideoBitRateParameter = "";
					string ffmpegVideoMaxRateParameter = "";
					string ffmpegVideoBufSizeParameter = "";
					string ffmpegAudioBitRateParameter = "";

					tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
						ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
						ffmpegVideoBufSizeParameter) = videoBitRateInfo;

					if (outputFileToBeAdded)
					{
						string segmentDirectory =
							regex_replace(segmentTemplateDirectory,
								regex(_multiTrackTemplateVariable), to_string(videoHeight));

						_logger->info(__FILEREF__ + "Creating directory"
							+ ", : " + segmentDirectory
						);
						fs::create_directories(segmentDirectory);
						fs::permissions(segmentDirectory,
							fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
							| fs::perms::group_read | fs::perms::group_exec
							| fs::perms::others_read | fs::perms::others_exec,
							fs::perm_options::replace);
					}

					if (_videoTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:v:") + to_string(_videoTrackIndexToBeUsed));
					}
					if (_audioTrackIndexToBeUsed >= 0)
					{
						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:a:") + to_string(_audioTrackIndexToBeUsed));
					}
					FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
					if (videoResolutionToBeAdded)
					{
						if (filtersRoot == Json::nullValue)
							FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
								ffmpegArgumentList);
						else
						{
							string videoFilters = ffmpegFilters.addVideoFilters(filtersRoot, ffmpegVideoResolutionParameter,
								"", -1);

							if (videoFilters != "")
								FFMpegEncodingParameters::addToArguments(string("-filter:v ") + videoFilters,
									ffmpegArgumentList);
							else
								FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
									ffmpegArgumentList);
						}
					}
					ffmpegArgumentList.push_back("-threads");
					ffmpegArgumentList.push_back("0");
					FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
					if (_audioBitRatesInfo.size() > videoIndex)
						ffmpegAudioBitRateParameter = _audioBitRatesInfo[videoIndex];
					else 
						ffmpegAudioBitRateParameter = _audioBitRatesInfo[
							_audioBitRatesInfo.size() - 1];
					FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
					if (filtersRoot != Json::nullValue)
					{
						string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

						if (audioFilters != "")
							FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
								ffmpegArgumentList);
					}

					if (outputFileToBeAdded)
					{
						FFMpegEncodingParameters::addToArguments(_ffmpegHttpStreamingParameter, ffmpegArgumentList);

						if (_httpStreamingFileFormat == "hls")
						{
							string segmentPathFileName =
								regex_replace(segmentTemplatePathFileName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back("-hls_segment_filename");
							ffmpegArgumentList.push_back(segmentPathFileName);
						}

						{
							string stagingManifestAssetPathName =
								regex_replace(stagingTemplateManifestAssetPathName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
							ffmpegArgumentList.push_back(stagingManifestAssetPathName);
						}
					}
				}
			}
		}
		else	// NO hls or dash output
		{
			/* 2021-09-10: In case videoBitRatesInfo has more than one bitrates,
			 *	it has to be created one file for each bit rate and than
			 *	merge all in the last file with a copy command, i.e.:
			 *		- ffmpeg -i ./1.mp4 -i ./2.mp4 -c copy -map 0 -map 1 ./3.mp4
			*/

			if (stepNumber == 0 || stepNumber == 1)	// YES two passes
			{
				// check su if (outputFileToBeAdded) è inutile, con 2 passi outputFileToBeAdded deve essere true

				// used also in removeTwoPassesTemporaryFiles
				string prefixPasslogFileName = 
					to_string(_ingestionJobKey)
					+ "_"
					+ to_string(_encodingJobKey)
				;
                string ffmpegTemplatePassLogPathFileName = _ffmpegTempDir
					+ "/"
					+ prefixPasslogFileName
					+ "_" + _multiTrackTemplatePart + ".passlog";
				;

				if (stepNumber == 0)	// YES two passes, first step
				{
					for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= _videoBitRatesInfo [videoIndex];

						string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						int videoHeight = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegAudioBitRateParameter = "";

						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
							ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
							ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						if (_videoTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:v:") + to_string(_videoTrackIndexToBeUsed));
						}
						if (_audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(_audioTrackIndexToBeUsed));
						}
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
						if (videoResolutionToBeAdded)
						{
							if (filtersRoot == Json::nullValue)
								FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
									ffmpegArgumentList);
							else
							{
								string videoFilters = ffmpegFilters.addVideoFilters(filtersRoot, ffmpegVideoResolutionParameter,
									"", -1);

								if (videoFilters != "")
									FFMpegEncodingParameters::addToArguments(string("-filter:v ") + videoFilters,
										ffmpegArgumentList);
								else
									FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
										ffmpegArgumentList);
							}
						}
						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						ffmpegArgumentList.push_back("-pass");
						ffmpegArgumentList.push_back("1");
						ffmpegArgumentList.push_back("-passlogfile");
						{
							string ffmpegPassLogPathFileName =
								regex_replace(ffmpegTemplatePassLogPathFileName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
						}
						// It should be useless to add the audio parameters in phase 1 but,
						// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
						//	it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
						//	So, this is the reason, I'm adding phase 2 as well
						// + "-an "    // disable audio
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						if (_audioBitRatesInfo.size() > videoIndex)
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[videoIndex];
						else 
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[
								_audioBitRatesInfo.size() - 1];
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
						if (filtersRoot != Json::nullValue)
						{
							string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

							if (audioFilters != "")
								FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
									ffmpegArgumentList);
						}

						// 2020-08-21: changed from ffmpegFileFormatParameter to -f null
						// FFMpegEncodingParameters::addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
						ffmpegArgumentList.push_back("-f");
						ffmpegArgumentList.push_back("null");

						ffmpegArgumentList.push_back("/dev/null");
					}
				}
				else if (stepNumber == 1)	// YES two passes, second step
				{
					string stagingTemplateEncodedAssetPathName = getMultiTrackEncodedStagingTemplateAssetPathName();

					for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size(); videoIndex++)
					{
						tuple<string, int, int, int, string, string, string> videoBitRateInfo
							= _videoBitRatesInfo [videoIndex];

						string ffmpegVideoResolutionParameter = "";
						int videoBitRateInKbps = -1;
						int videoHeight = -1;
						string ffmpegVideoBitRateParameter = "";
						string ffmpegVideoMaxRateParameter = "";
						string ffmpegVideoBufSizeParameter = "";
						string ffmpegAudioBitRateParameter = "";

						tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, videoHeight,
							ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
							ffmpegVideoBufSizeParameter) = videoBitRateInfo;

						if (_videoTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:v:") + to_string(_videoTrackIndexToBeUsed));
						}
						if (_audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(_audioTrackIndexToBeUsed));
						}
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
						if (videoResolutionToBeAdded)
						{
							if (filtersRoot == Json::nullValue)
								FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
									ffmpegArgumentList);
							else
							{
								string videoFilters = ffmpegFilters.addVideoFilters(filtersRoot, ffmpegVideoResolutionParameter,
									"", -1);

								if (videoFilters != "")
									FFMpegEncodingParameters::addToArguments(string("-filter:v ") + videoFilters,
										ffmpegArgumentList);
								else
									FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
										ffmpegArgumentList);
							}
						}
						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						ffmpegArgumentList.push_back("-pass");
						ffmpegArgumentList.push_back("2");
						ffmpegArgumentList.push_back("-passlogfile");
						{
							string ffmpegPassLogPathFileName =
								regex_replace(ffmpegTemplatePassLogPathFileName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
						}
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						if (_audioBitRatesInfo.size() > videoIndex)
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[videoIndex];
						else 
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[
								_audioBitRatesInfo.size() - 1];
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
						if (filtersRoot != Json::nullValue)
						{
							string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

							if (audioFilters != "")
								FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
									ffmpegArgumentList);
						}

						FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
						if (_videoBitRatesInfo.size() > 1)
						{
							string newStagingEncodedAssetPathName =
								regex_replace(stagingTemplateEncodedAssetPathName,
									regex(_multiTrackTemplateVariable), to_string(videoHeight));
							ffmpegArgumentList.push_back(newStagingEncodedAssetPathName);
						}
						else
							ffmpegArgumentList.push_back(_encodedStagingAssetPathName);
					}
                }
            }
            else	// NO two passes
            {
				string stagingTemplateEncodedAssetPathName;
				if (outputFileToBeAdded)
					stagingTemplateEncodedAssetPathName = getMultiTrackEncodedStagingTemplateAssetPathName();

				if (_isVideo)
				{
					if (_videoBitRatesInfo.size() != 0)
					{
						for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size();
							videoIndex++)
						{
							tuple<string, int, int, int, string, string, string> videoBitRateInfo
								= _videoBitRatesInfo [videoIndex];

							string ffmpegVideoResolutionParameter = "";
							int videoBitRateInKbps = -1;
							int videoHeight = -1;
							string ffmpegVideoBitRateParameter = "";
							string ffmpegVideoMaxRateParameter = "";
							string ffmpegVideoBufSizeParameter = "";
							string ffmpegAudioBitRateParameter = "";

							tie(ffmpegVideoResolutionParameter, videoBitRateInKbps,
								ignore, videoHeight,
								ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
								ffmpegVideoBufSizeParameter) = videoBitRateInfo;

							if (_videoTrackIndexToBeUsed >= 0)
							{
								ffmpegArgumentList.push_back("-map");
								ffmpegArgumentList.push_back(
									string("0:v:") + to_string(_videoTrackIndexToBeUsed));
							}
							if (_audioTrackIndexToBeUsed >= 0)
							{
								ffmpegArgumentList.push_back("-map");
								ffmpegArgumentList.push_back(
									string("0:a:") + to_string(_audioTrackIndexToBeUsed));
							}
							FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
							if (videoResolutionToBeAdded)
							{
								if (filtersRoot == Json::nullValue)
									FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
										ffmpegArgumentList);
								else
								{
									string videoFilters = ffmpegFilters.addVideoFilters(filtersRoot, ffmpegVideoResolutionParameter,
										"", -1);

									if (videoFilters != "")
										FFMpegEncodingParameters::addToArguments(string("-filter:v ") + videoFilters,
											ffmpegArgumentList);
									else
										FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
											ffmpegArgumentList);
								}
							}
							ffmpegArgumentList.push_back("-threads");
							ffmpegArgumentList.push_back("0");
							FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
							if (_audioBitRatesInfo.size() > videoIndex)
								ffmpegAudioBitRateParameter = _audioBitRatesInfo[videoIndex];
							else 
								ffmpegAudioBitRateParameter = _audioBitRatesInfo[
									_audioBitRatesInfo.size() - 1];
							FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
							FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
							if (filtersRoot != Json::nullValue)
							{
								string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

								if (audioFilters != "")
									FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
										ffmpegArgumentList);
							}

							if (outputFileToBeAdded)
							{
								FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
								if (_videoBitRatesInfo.size() > 1)
								{
									string newStagingEncodedAssetPathName =
										regex_replace(stagingTemplateEncodedAssetPathName,
											regex(_multiTrackTemplateVariable), to_string(videoHeight));
									ffmpegArgumentList.push_back(newStagingEncodedAssetPathName);
								}
								else
									ffmpegArgumentList.push_back(_encodedStagingAssetPathName);
							}
						}
					}
					else
					{
						// 2023-05-07: è un video senza videoBitRates. E' lo scenario in cui gli è stato dato
						//	un profile di encoding solo audio.
						//	In questo scenario _ffmpegVideoCodecParameter è stato inizializzato con "c:v copy "
						//	in settingFfmpegParameters

						string ffmpegAudioBitRateParameter = "";

						if (_videoTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:v:") + to_string(_videoTrackIndexToBeUsed));
						}
						if (_audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(_audioTrackIndexToBeUsed));
						}
						FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
						// FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
						// if (videoResolutionToBeAdded)
						// 	FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
						// 		ffmpegArgumentList);
						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						// if (_audioBitRatesInfo.size() > videoIndex)
						// 	ffmpegAudioBitRateParameter = _audioBitRatesInfo[videoIndex];
						// else 
							ffmpegAudioBitRateParameter = _audioBitRatesInfo[
								_audioBitRatesInfo.size() - 1];
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
						if (filtersRoot != Json::nullValue)
						{
							string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

							if (audioFilters != "")
								FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
									ffmpegArgumentList);
						}

						if (outputFileToBeAdded)
						{
							// FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
							// if (_videoBitRatesInfo.size() > 1)
							// {
							// 	string newStagingEncodedAssetPathName =
							// 		regex_replace(stagingTemplateEncodedAssetPathName,
							// 			regex(_multiTrackTemplateVariable), to_string(videoHeight));
							// 	ffmpegArgumentList.push_back(newStagingEncodedAssetPathName);
							// }
							// else
								ffmpegArgumentList.push_back(_encodedStagingAssetPathName);
						}
					}
				}
				else
				{
					for (int audioIndex = 0; audioIndex < _audioBitRatesInfo.size();
						audioIndex++)
					{
						string ffmpegAudioBitRateParameter = _audioBitRatesInfo[audioIndex];

						if (_audioTrackIndexToBeUsed >= 0)
						{
							ffmpegArgumentList.push_back("-map");
							ffmpegArgumentList.push_back(
								string("0:a:") + to_string(_audioTrackIndexToBeUsed));
						}
						ffmpegArgumentList.push_back("-threads");
						ffmpegArgumentList.push_back("0");
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);
						if (filtersRoot != Json::nullValue)
						{
							string audioFilters = ffmpegFilters.addAudioFilters(filtersRoot, -1);

							if (audioFilters != "")
								FFMpegEncodingParameters::addToArguments(string("-filter:a ") + audioFilters,
									ffmpegArgumentList);
						}

						if (outputFileToBeAdded)
						{
							FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
							/*
							if (videoBitRatesInfo.size() > 1)
							{
								string newStagingEncodedAssetPathName =
									regex_replace(stagingTemplateEncodedAssetPathName,
										regex(_multiTrackTemplateVariable), to_string(videoHeight));
								ffmpegArgumentList.push_back(newStagingEncodedAssetPathName);
							}
							else
							*/
								ffmpegArgumentList.push_back(_encodedStagingAssetPathName);
						}
					}
				}
            }
		}
	}
    catch(runtime_error& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: applyEncoding failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
    catch(exception& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: applyEncoding failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
}

void FFMpegEncodingParameters::createManifestFile()
{
	try
	{
		if (!_initialized)
		{
			string errorMessage = string("FFMpegEncodingParameters is not initialized")
				+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			;
			throw runtime_error(errorMessage);
		}

		string manifestFileName = getManifestFileName();

		// create the master playlist
		/*
			#EXTM3U
			#EXT-X-VERSION:3
			#EXT-X-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360
			360p.m3u8
			#EXT-X-STREAM-INF:BANDWIDTH=1400000,RESOLUTION=842x480
			480p.m3u8
			#EXT-X-STREAM-INF:BANDWIDTH=2800000,RESOLUTION=1280x720
			720p.m3u8
			#EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080
			1080p.m3u8
		 */
		string endLine = "\n";
		string masterManifest =
			"#EXTM3U" + endLine
			+ "#EXT-X-VERSION:3" + endLine
		;

		for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size(); videoIndex++)
		{
			tuple<string, int, int, int, string, string, string> videoBitRateInfo
				= _videoBitRatesInfo [videoIndex];

			int videoBitRateInKbps = -1;
			int videoWidth = -1;
			int videoHeight = -1;

			tie(ignore, videoBitRateInKbps, videoWidth, videoHeight,
				ignore, ignore,
				ignore) = videoBitRateInfo;

			masterManifest +=
				"#EXT-X-STREAM-INF:BANDWIDTH="
					+ to_string(videoBitRateInKbps * 1000)
					+ ",RESOLUTION=" + to_string(videoWidth)
					+ "x" + to_string(videoHeight) + endLine
				;

			string manifestRelativePathName;
			{
				manifestRelativePathName = _multiTrackTemplatePart
					+ "/" + manifestFileName;
				manifestRelativePathName =
					regex_replace(manifestRelativePathName,
						regex(_multiTrackTemplateVariable), to_string(videoHeight));
			}
			masterManifest +=
				manifestRelativePathName + endLine;
		}

		string masterManifestPathFileName = _encodedStagingAssetPathName + "/"
			+ manifestFileName;

		_logger->info(__FILEREF__ + "Writing Master Manifest File"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", masterManifestPathFileName: " + masterManifestPathFileName
			+ ", masterManifest: " + masterManifest
		);
		ofstream ofMasterManifestFile(masterManifestPathFileName);
		ofMasterManifestFile << masterManifest;
	}
	catch(runtime_error& e)
	{
		_logger->error(__FILEREF__ + "FFMpeg: createManifestFile_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
    catch(exception& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: createManifestFile_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
}

void FFMpegEncodingParameters::applyEncoding_audioGroup(
	// -1: NO two passes
	// 0: YES two passes, first step
	// 1: YES two passes, second step
	int stepNumber,

	// out (in append)
	vector<string>& ffmpegArgumentList
)
{
	try
	{
		if (!_initialized)
		{
			string errorMessage = string("FFMpegEncodingParameters is not initialized")
				+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			;
			throw runtime_error(errorMessage);
		}

		/*
		 * The command will be like this:

		ffmpeg -y -i /var/catramms/storage/MMSRepository/MMS_0000/ws2/000/228/001/1247989_source.mp4

			-map 0:1 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/ita/1247992_384637_%04d.ts -f hls /home/mms/tmp/ita/1247992_384637.m3u8

			-map 0:2 -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/eng/1247992_384637_%04d.ts -f hls /home/mms/tmp/eng/1247992_384637.m3u8

			-map 0:0 -codec:v libx264 -profile:v high422 -b:v 800k -preset veryfast -level 4.0 -crf 22 -r 25 -vf scale=640:360 -threads 0 -hls_time 10 -hls_list_size 0 -hls_segment_filename /home/mms/tmp/low/1247992_384637_%04d.ts -f hls /home/mms/tmp/low/1247992_384637.m3u8

		Manifest will be like:
		#EXTM3U
		#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="ita",NAME="ita",AUTOSELECT=YES, DEFAULT=YES,URI="ita/8896718_1509416.m3u8"
		#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES, DEFAULT=YES,URI="eng/8896718_1509416.m3u8"
		#EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO="audio"
		0/8896718_1509416.m3u8


		https://developer.apple.com/documentation/http_live_streaming/example_playlists_for_http_live_streaming/adding_alternate_media_to_a_playlist#overview
		https://github.com/videojs/http-streaming/blob/master/docs/multiple-alternative-audio-tracks.md

		*/

		string ffmpegVideoResolutionParameter = "";
		int videoBitRateInKbps = -1;
		string ffmpegVideoBitRateParameter = "";
		string ffmpegVideoMaxRateParameter = "";
		string ffmpegVideoBufSizeParameter = "";
		string ffmpegAudioBitRateParameter = "";

		tuple<string, int, int, int, string, string, string> videoBitRateInfo
			= _videoBitRatesInfo[0];
		tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore,
			ffmpegVideoBitRateParameter,
			ffmpegVideoMaxRateParameter, ffmpegVideoBufSizeParameter) = videoBitRateInfo;

		ffmpegAudioBitRateParameter = _audioBitRatesInfo[0];

		_logger->info(__FILEREF__ + "Special encoding in order to allow audio/language selection by the player"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		);

		// the manifestFileName naming convention is used also in EncoderVideoAudioProxy.cpp
		string manifestFileName = getManifestFileName();

		if (stepNumber == 0 || stepNumber == 1)	// YES two passes
		{
			// used also in removeTwoPassesTemporaryFiles
			string prefixPasslogFileName = 
				to_string(_ingestionJobKey)
				+ "_"
				+ to_string(_encodingJobKey) + ".passlog";
			string ffmpegPassLogPathFileName = _ffmpegTempDir // string(stagingEncodedAssetPath)
				+ "/"
				+ prefixPasslogFileName
			;

			if (stepNumber == 0)	// YES two passes, first step
			{
				// It should be useless to add the audio parameters in phase 1 but,
				// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
				//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
				//  So, this is the reason, I'm adding phase 2 as well
				// + "-an "    // disable audio
				if (_audioTracksRoot != Json::nullValue)
				{
					for (int index = 0; index < _audioTracksRoot.size(); index++)
					{
						Json::Value audioTrack = _audioTracksRoot[index];

						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:") + to_string(audioTrack.get("trackIndex", -1).asInt()));

						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);

						FFMpegEncodingParameters::addToArguments(_ffmpegHttpStreamingParameter, ffmpegArgumentList);

						string audioTrackDirectoryName = JSONUtils::asString(audioTrack, "language", "");

						{
							string segmentPathFileName =
								_encodedStagingAssetPathName 
								+ "/"
								+ audioTrackDirectoryName
								+ "/"
								+ to_string(_ingestionJobKey)
								+ "_"
								+ to_string(_encodingJobKey)
								+ "_%04d.ts"
							;
							ffmpegArgumentList.push_back("-hls_segment_filename");
							ffmpegArgumentList.push_back(segmentPathFileName);
						}

						FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
						{
							string stagingManifestAssetPathName =
								_encodedStagingAssetPathName
								+ "/" + audioTrackDirectoryName
								+ "/" + manifestFileName;
							ffmpegArgumentList.push_back(stagingManifestAssetPathName);
						}
					}
				}

				if (_videoTracksRoot != Json::nullValue)
				{
					Json::Value videoTrack = _videoTracksRoot[0];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(videoTrack.get("trackIndex", -1).asInt()));
				}
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
					ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("1");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);
				// 2020-01-20: I removed the hls file format parameter because it was not working
				//	and added -f mp4. At the end it has to generate just the log file
				//	to be used in the second step
				// FFMpegEncodingParameters::addToArguments(ffmpegHttpStreamingParameter, ffmpegArgumentList);
				//
				// FFMpegEncodingParameters::addToArguments(ffmpegFileFormatParameter, ffmpegArgumentList);
				ffmpegArgumentList.push_back("-f");
				// 2020-08-21: changed from mp4 to null
				ffmpegArgumentList.push_back("null");

				ffmpegArgumentList.push_back("/dev/null");
			}
			else if (stepNumber == 1)	// YES two passes, second step
			{
				// It should be useless to add the audio parameters in phase 1 but,
				// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
				//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
				//  So, this is the reason, I'm adding phase 2 as well
				// + "-an "    // disable audio
				if (_audioTracksRoot != Json::nullValue)
				{
					for (int index = 0; index < _audioTracksRoot.size(); index++)
					{
						Json::Value audioTrack = _audioTracksRoot[index];

						ffmpegArgumentList.push_back("-map");
						ffmpegArgumentList.push_back(
							string("0:") + to_string(audioTrack.get("trackIndex", -1).asInt()));

						FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
						FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);

						FFMpegEncodingParameters::addToArguments(_ffmpegHttpStreamingParameter, ffmpegArgumentList);

						string audioTrackDirectoryName = JSONUtils::asString(audioTrack, "language", "");

						{
							string segmentPathFileName =
								_encodedStagingAssetPathName 
								+ "/"
								+ audioTrackDirectoryName
								+ "/"
								+ to_string(_ingestionJobKey)
								+ "_"
								+ to_string(_encodingJobKey)
								+ "_%04d.ts"
							;
							ffmpegArgumentList.push_back("-hls_segment_filename");
							ffmpegArgumentList.push_back(segmentPathFileName);
						}

						FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
						{
							string stagingManifestAssetPathName =
								_encodedStagingAssetPathName
								+ "/" + audioTrackDirectoryName
								+ "/" + manifestFileName;
							ffmpegArgumentList.push_back(stagingManifestAssetPathName);
						}
					}
				}

				if (_videoTracksRoot != Json::nullValue)
				{
					Json::Value videoTrack = _videoTracksRoot[0];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(videoTrack.get("trackIndex", -1).asInt()));
				}
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
				FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
					ffmpegArgumentList);
				ffmpegArgumentList.push_back("-threads");
				ffmpegArgumentList.push_back("0");
				ffmpegArgumentList.push_back("-pass");
				ffmpegArgumentList.push_back("2");
				ffmpegArgumentList.push_back("-passlogfile");
				ffmpegArgumentList.push_back(ffmpegPassLogPathFileName);

				FFMpegEncodingParameters::addToArguments(_ffmpegHttpStreamingParameter, ffmpegArgumentList);

				string videoTrackDirectoryName;
				if (_videoTracksRoot != Json::nullValue)
				{
					Json::Value videoTrack = _videoTracksRoot[0];

					videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
				}

				{
					string segmentPathFileName =
						_encodedStagingAssetPathName 
						+ "/"
						+ videoTrackDirectoryName
						+ "/"
						+ to_string(_ingestionJobKey)
						+ "_"
						+ to_string(_encodingJobKey)
						+ "_%04d.ts"
					;
					ffmpegArgumentList.push_back("-hls_segment_filename");
					ffmpegArgumentList.push_back(segmentPathFileName);
				}

				FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
				{
					string stagingManifestAssetPathName =
						_encodedStagingAssetPathName
						+ "/" + videoTrackDirectoryName
						+ "/" + manifestFileName;
					ffmpegArgumentList.push_back(stagingManifestAssetPathName);
				}
			}
		}
		else
		{
			// It should be useless to add the audio parameters in phase 1 but,
			// it happened once that the passed 2 failed. Looking on Internet (https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=2464)
			//  it suggested to add the audio parameters too in phase 1. Really, adding the audio prameters, phase 2 was successful.
			//  So, this is the reason, I'm adding phase 2 as well
			// + "-an "    // disable audio
			if (_audioTracksRoot != Json::nullValue)
			{
				for (int index = 0; index < _audioTracksRoot.size(); index++)
				{
					Json::Value audioTrack = _audioTracksRoot[index];

					ffmpegArgumentList.push_back("-map");
					ffmpegArgumentList.push_back(
						string("0:") + to_string(audioTrack.get("trackIndex", -1).asInt()));

					FFMpegEncodingParameters::addToArguments(_ffmpegAudioCodecParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegAudioOtherParameters, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegAudioChannelsParameter, ffmpegArgumentList);
					FFMpegEncodingParameters::addToArguments(_ffmpegAudioSampleRateParameter, ffmpegArgumentList);

					FFMpegEncodingParameters::addToArguments(_ffmpegHttpStreamingParameter, ffmpegArgumentList);

					string audioTrackDirectoryName = JSONUtils::asString(audioTrack, "language", "");

					{
						string segmentPathFileName =
							_encodedStagingAssetPathName 
							+ "/"
							+ audioTrackDirectoryName
							+ "/"
							+ to_string(_ingestionJobKey)
							+ "_"
							+ to_string(_encodingJobKey)
							+ "_%04d.ts"
						;
						ffmpegArgumentList.push_back("-hls_segment_filename");
						ffmpegArgumentList.push_back(segmentPathFileName);
					}

					FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
					{
						string stagingManifestAssetPathName =
							_encodedStagingAssetPathName
							+ "/" + audioTrackDirectoryName
							+ "/" + manifestFileName;
						ffmpegArgumentList.push_back(stagingManifestAssetPathName);
					}
				}
			}

			if (_videoTracksRoot != Json::nullValue)
			{
				Json::Value videoTrack = _videoTracksRoot[0];

				ffmpegArgumentList.push_back("-map");
				ffmpegArgumentList.push_back(
					string("0:") + to_string(videoTrack.get("trackIndex", -1).asInt()));
			}
			FFMpegEncodingParameters::addToArguments(_ffmpegVideoCodecParameter, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(_ffmpegVideoProfileParameter, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(_ffmpegVideoOtherParameters, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(_ffmpegVideoFrameRateParameter, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(_ffmpegVideoKeyFramesRateParameter, ffmpegArgumentList);
			FFMpegEncodingParameters::addToArguments(string("-vf ") + ffmpegVideoResolutionParameter,
				ffmpegArgumentList);
			ffmpegArgumentList.push_back("-threads");
			ffmpegArgumentList.push_back("0");

			FFMpegEncodingParameters::addToArguments(_ffmpegHttpStreamingParameter, ffmpegArgumentList);

			string videoTrackDirectoryName;	
			if (_videoTracksRoot != Json::nullValue)
			{
				Json::Value videoTrack = _videoTracksRoot[0];

				videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
			}

			{
				string segmentPathFileName =
					_encodedStagingAssetPathName 
					+ "/"
					+ videoTrackDirectoryName
					+ "/"
					+ to_string(_ingestionJobKey)
					+ "_"
					+ to_string(_encodingJobKey)
					+ "_%04d.ts"
				;
				ffmpegArgumentList.push_back("-hls_segment_filename");
				ffmpegArgumentList.push_back(segmentPathFileName);
			}

			FFMpegEncodingParameters::addToArguments(_ffmpegFileFormatParameter, ffmpegArgumentList);
			{
				string stagingManifestAssetPathName =
					_encodedStagingAssetPathName
					+ "/" + videoTrackDirectoryName
					+ "/" + manifestFileName;
				ffmpegArgumentList.push_back(stagingManifestAssetPathName);
			}
		}
	}
    catch(runtime_error& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: applyEncoding_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
    catch(exception& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: applyEncoding_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
}

void FFMpegEncodingParameters::createManifestFile_audioGroup()
{
	try
	{
		if (!_initialized)
		{
			string errorMessage = string("FFMpegEncodingParameters is not initialized")
				+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			;
			throw runtime_error(errorMessage);
		}

		string manifestFileName = getManifestFileName();

		string mainManifestPathName = _encodedStagingAssetPathName + "/"
			+ manifestFileName;

		string mainManifest;

		mainManifest = string("#EXTM3U") + "\n";

		if (_audioTracksRoot != Json::nullValue)
		{
			for (int index = 0; index < _audioTracksRoot.size(); index++)
			{
				Json::Value audioTrack = _audioTracksRoot[index];

				string audioTrackDirectoryName = JSONUtils::asString(audioTrack, "language", "");

				string audioLanguage = JSONUtils::asString(audioTrack, "language", "");

				string audioManifestLine = "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",LANGUAGE=\""
					+ audioLanguage + "\",NAME=\"" + audioLanguage + "\",AUTOSELECT=YES, DEFAULT=YES,URI=\""
					+ audioTrackDirectoryName + "/" + manifestFileName + "\"";
				
				mainManifest += (audioManifestLine + "\n");
			}
		}

		string videoManifestLine = "#EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO=\"audio\"";
		mainManifest += (videoManifestLine + "\n");

		string videoTrackDirectoryName;
		if (_videoTracksRoot != Json::nullValue)
		{
			Json::Value videoTrack = _videoTracksRoot[0];

			videoTrackDirectoryName = to_string(videoTrack.get("trackIndex", -1).asInt());
		}
		mainManifest += (videoTrackDirectoryName + "/" + manifestFileName + "\n");

		ofstream manifestFile(mainManifestPathName);
		manifestFile << mainManifest;
	}
    catch(runtime_error& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: createManifestFile_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
    catch(exception& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: createManifestFile_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
}

string FFMpegEncodingParameters::getManifestFileName()
{
	try
	{
		if (!_initialized)
		{
			string errorMessage = string("FFMpegEncodingParameters is not initialized")
				+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			;
			throw runtime_error(errorMessage);
		}

		string manifestFileName = to_string(_ingestionJobKey) +
			"_" + to_string(_encodingJobKey);
		if (_httpStreamingFileFormat == "hls")
			manifestFileName += ".m3u8";
		else    // if (_httpStreamingFileFormat == "dash")
			manifestFileName += ".mpd";

		return manifestFileName;
	}
    catch(runtime_error& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: createManifestFile_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
    catch(exception& e)
    {
		_logger->error(__FILEREF__ + "FFMpeg: createManifestFile_audioGroup failed"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", e.what(): " + e.what()
		);

		throw e;
    }
}



void FFMpegEncodingParameters::settingFfmpegParameters(
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
)
{
    string field;

	{
		string sEncodingProfileDetailsRoot = JSONUtils::toString(encodingProfileDetailsRoot);

		logger->info(__FILEREF__ + "settingFfmpegParameters"
			", sEncodingProfileDetailsRoot: " + sEncodingProfileDetailsRoot
		);
	}

    // fileFormat
    string fileFormat;
	string fileFormatLowerCase;
    {
		field = "fileFormat";
		if (!JSONUtils::isMetadataPresent(encodingProfileDetailsRoot, field))
		{
			string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
				+ ", Field: " + field;
            logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        fileFormat = JSONUtils::asString(encodingProfileDetailsRoot, field, "");
		fileFormatLowerCase.resize(fileFormat.size());
		transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(),
			[](unsigned char c){return tolower(c); } );

        FFMpegEncodingParameters::encodingFileFormatValidation(fileFormat, logger);

        if (fileFormatLowerCase == "hls")
        {
			httpStreamingFileFormat = "hls";

            ffmpegFileFormatParameter = 
				+ "-f hls "
			;

			long segmentDurationInSeconds = 10;

			field = "HLS";
			if (JSONUtils::isMetadataPresent(encodingProfileDetailsRoot, field))
			{
				Json::Value hlsRoot = encodingProfileDetailsRoot[field]; 

				field = "segmentDuration";
				segmentDurationInSeconds = JSONUtils::asInt(hlsRoot, field, 10);
			}

            ffmpegHttpStreamingParameter = 
				"-hls_time " + to_string(segmentDurationInSeconds) + " ";

			// hls_list_size: set the maximum number of playlist entries. If set to 0 the list file
			//	will contain all the segments. Default value is 5.
            ffmpegHttpStreamingParameter += "-hls_list_size 0 ";
		}
		else if (fileFormatLowerCase == "dash")
        {
			httpStreamingFileFormat = "dash";

            ffmpegFileFormatParameter = 
				+ "-f dash "
			;

			long segmentDurationInSeconds = 10;

			field = "DASH";
			if (JSONUtils::isMetadataPresent(encodingProfileDetailsRoot, field))
			{
				Json::Value dashRoot = encodingProfileDetailsRoot[field]; 

				field = "segmentDuration";
				segmentDurationInSeconds = JSONUtils::asInt(dashRoot, field, 10);
			}

            ffmpegHttpStreamingParameter =
				"-seg_duration " + to_string(segmentDurationInSeconds) + " ";

			// hls_list_size: set the maximum number of playlist entries. If set to 0 the list file
			//	will contain all the segments. Default value is 5.
            // ffmpegHttpStreamingParameter += "-hls_list_size 0 ";

			// it is important to specify -init_seg_name because those files
			// will not be removed in EncoderVideoAudioProxy.cpp
            ffmpegHttpStreamingParameter +=
				"-init_seg_name init-stream$RepresentationID$.$ext$ ";

			// the only difference with the ffmpeg default is that default is $Number%05d$
			// We had to change it to $Number%01d$ because otherwise the generated file containing
			// 00001 00002 ... but the videojs player generates file name like 1 2 ...
			// and the streaming was not working
            ffmpegHttpStreamingParameter +=
				"-media_seg_name chunk-stream$RepresentationID$-$Number%01d$.$ext$ ";
		}
        else
        {
            httpStreamingFileFormat = "";

			if (fileFormatLowerCase == "ts" || fileFormatLowerCase == "mts")
			{
				// if "-f ts filename.ts" is added the following error happens:
				//		...Requested output format 'ts' is not a suitable output format
				// Without "-f ts", just filename.ts works fine
				// Same for mts
				ffmpegFileFormatParameter = "";
			}
			else
			{
				ffmpegFileFormatParameter =
					" -f " + fileFormatLowerCase + " "
				;
			}
        }
    }

    if (isVideo)
    {
		field = "video";
		if (JSONUtils::isMetadataPresent(encodingProfileDetailsRoot, field))
		{
			Json::Value videoRoot = encodingProfileDetailsRoot[field]; 

			// codec
			string codec;
			{
				field = "codec";
				if (!JSONUtils::isMetadataPresent(videoRoot, field))
				{
					string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
                        + ", Field: " + field;
					logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				codec = JSONUtils::asString(videoRoot, field, "");

				// 2020-03-27: commented just to avoid to add the check every time a new codec is added
				//		In case the codec is wrong, ffmpeg will generate the error later
				// FFMpeg::encodingVideoCodecValidation(codec, logger);

				ffmpegVideoCodecParameter   =
                    "-codec:v " + codec + " "
				;
			}

			// profile
			{
				field = "profile";
				if (JSONUtils::isMetadataPresent(videoRoot, field))
				{
					string profile = JSONUtils::asString(videoRoot, field, "");

					if (codec == "libx264" || codec == "libvpx")
					{
						FFMpegEncodingParameters::encodingVideoProfileValidation(codec, profile, logger);
						if (codec == "libx264")
						{
							ffmpegVideoProfileParameter =
								"-profile:v " + profile + " "
							;
						}
						else if (codec == "libvpx")
						{
							ffmpegVideoProfileParameter =
								"-quality " + profile + " "
							;
						}
					}
					else if (profile != "")
					{
						ffmpegVideoProfileParameter =
							"-profile:v " + profile + " "
						;
						/*
						string errorMessage = __FILEREF__ + "FFMpeg: codec is wrong"
                            + ", codec: " + codec;
						logger->error(errorMessage);

						throw runtime_error(errorMessage);
						*/
					}
				}
			}

			// OtherOutputParameters
			{
				field = "otherOutputParameters";
				if (JSONUtils::isMetadataPresent(videoRoot, field))
				{
					string otherOutputParameters = JSONUtils::asString(videoRoot, field, "");

					ffmpegVideoOtherParameters =
                        otherOutputParameters + " "
					;
				}
			}

			// twoPasses
			{
				field = "twoPasses";
				if (!JSONUtils::isMetadataPresent(videoRoot, field))
				{
					string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
                        + ", Field: " + field;
					logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				twoPasses = JSONUtils::JSONUtils::asBool(videoRoot, field, false);
			}

			// frameRate
			{
				field = "frameRate";
				if (JSONUtils::isMetadataPresent(videoRoot, field))
				{
					int frameRate = JSONUtils::asInt(videoRoot, field, 0);

					if (frameRate != 0)
					{
						ffmpegVideoFrameRateParameter =
							"-r " + to_string(frameRate) + " "
						;

						// keyFrameIntervalInSeconds
						{
							/*
								Un tipico codec video utilizza la compressione temporale, ovvero la maggior parte
								dei fotogrammi memorizza solo la differenza rispetto ai fotogrammi precedenti
								(e in alcuni casi futuri). Quindi, per decodificare questi fotogrammi, è necessario
								fare riferimento ai fotogrammi precedenti, al fine di generare un'immagine completa.
								In breve, i fotogrammi chiave sono fotogrammi che non si basano su altri fotogrammi
								per la decodifica e su cui si basano altri fotogrammi per essere decodificati.

								Se un video deve essere tagliato o segmentato, senza transcodifica (ricompressione),
								la segmentazione può avvenire solo in corrispondenza dei fotogrammi chiave, in modo
								che il primo fotogramma di un segmento sia un fotogramma chiave. Se così non fosse,
								i fotogrammi di un segmento fino al fotogramma chiave successivo non potrebbero
								essere riprodotti.

								Un codificatore come x264 in genere genera fotogrammi chiave solo se rileva che si è verificato
								un cambio di scena*. Ciò non favorisce la segmentazione, poiché i fotogrammi chiave
								possono essere generati a intervalli irregolari. Per garantire la creazione di segmenti
								di lunghezze identiche e prevedibili, è possibile utilizzare l'opzione force_key_frames
								per garantire il posizionamento desiderato dei fotogrammi chiave.
							*/

							field = "keyFrameIntervalInSeconds";
							if (JSONUtils::isMetadataPresent(videoRoot, field))
							{
								int keyFrameIntervalInSeconds = JSONUtils::asInt(videoRoot, field, 5);

								field = "forceKeyFrames";
								bool forceKeyFrames = JSONUtils::asInt(videoRoot, field, false);

								// -g specifies the number of frames in a GOP
								if (forceKeyFrames)
									ffmpegVideoKeyFramesRateParameter =
										"-force_key_frames expr:gte(t,n_forced*" + to_string(keyFrameIntervalInSeconds) + ") ";
								else
									ffmpegVideoKeyFramesRateParameter =
										"-g " + to_string(frameRate * keyFrameIntervalInSeconds) + " ";
							}
						}
					}
				}
			}

			field = "bitRates";
			if (!JSONUtils::isMetadataPresent(videoRoot, field))
			{
				string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
					+ ", Field: " + field;
				logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value bitRatesRoot = videoRoot[field];

			videoBitRatesInfo.clear();
			{
				for (int bitRateIndex = 0; bitRateIndex < bitRatesRoot.size(); bitRateIndex++)
				{
					Json::Value bitRateInfo = bitRatesRoot[bitRateIndex];

					// resolution
					string ffmpegVideoResolution;
					int videoWidth;
					int videoHeight;
					{
						field = "width";
						if (!JSONUtils::isMetadataPresent(bitRateInfo, field))
						{
							string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
								+ ", Field: " + field;
							logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
						videoWidth = JSONUtils::asInt(bitRateInfo, field, 0);
						if (videoWidth == -1 && codec == "libx264")
							videoWidth   = -2;     // h264 requires always a even width/height

						field = "height";
						if (!JSONUtils::isMetadataPresent(bitRateInfo, field))
						{
							string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
								+ ", Field: " + field;
							logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
						videoHeight = JSONUtils::asInt(bitRateInfo, field, 0);
						if (videoHeight == -1 && codec == "libx264")
							videoHeight   = -2;     // h264 requires always a even width/height

						// forceOriginalAspectRatio could be: decrease or increase
						string forceOriginalAspectRatio;
						field = "forceOriginalAspectRatio";
						forceOriginalAspectRatio = JSONUtils::asString(bitRateInfo, field, "");

						bool pad = false;
						if (forceOriginalAspectRatio != "")
						{
							field = "pad";
							pad = JSONUtils::asBool(bitRateInfo, field, false);
						}

						// -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2"

						// ffmpegVideoResolution = "-vf scale=w=" + to_string(videoWidth)
						ffmpegVideoResolution = "scale=w=" + to_string(videoWidth)
							+ ":h=" + to_string(videoHeight);
						if (forceOriginalAspectRatio != "")
						{
							ffmpegVideoResolution += (":force_original_aspect_ratio=" + forceOriginalAspectRatio);
							if (pad)
								ffmpegVideoResolution += (",pad=" + to_string(videoWidth)
									+ ":" + to_string(videoHeight)
									+ ":(ow-iw)/2:(oh-ih)/2");
						}

						// ffmpegVideoResolution += " ";
					}

					string ffmpegVideoBitRate;
					int kBitRate;
					{
						field = "kBitRate";
						if (!JSONUtils::isMetadataPresent(bitRateInfo, field))
						{
							string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
								+ ", Field: " + field;
							logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						kBitRate = JSONUtils::asInt(bitRateInfo, field, 0);

						ffmpegVideoBitRate = "-b:v " + to_string(kBitRate) + "k ";
					}

					// maxRate
					string ffmpegVideoMaxRate;
					{
						field = "KMaxRate";
						if (JSONUtils::isMetadataPresent(bitRateInfo, field))
						{
							int maxRate = JSONUtils::asInt(bitRateInfo, field, 0);

							ffmpegVideoMaxRate = "-maxrate " + to_string(maxRate) + "k ";
						}
					}

					// bufSize
					string ffmpegVideoBufSize;
					{
						field = "KBufferSize";
						if (JSONUtils::isMetadataPresent(bitRateInfo, field))
						{
							int bufferSize = JSONUtils::asInt(bitRateInfo, field, 0);

							ffmpegVideoBufSize = "-bufsize " + to_string(bufferSize) + "k ";
						}
					}

					videoBitRatesInfo.push_back(make_tuple(ffmpegVideoResolution, kBitRate,
						videoWidth, videoHeight, ffmpegVideoBitRate,
						ffmpegVideoMaxRate, ffmpegVideoBufSize));
				}
			}
		}
		else
		{
			// 2023-05-07: Si tratta di un video e l'encoding profile non ha il field "video".
			//	Per cui sarà un Encoding Profile solo audio. In questo caso "copy" le traccie video sorgenti

			twoPasses = false;
			ffmpegVideoCodecParameter   =
				"-codec:v copy "
			;
		}
	}
    
    // if (contentType == "video" || contentType == "audio")
    {
        field = "audio";
        if (!JSONUtils::isMetadataPresent(encodingProfileDetailsRoot, field))
        {
            string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
                    + ", Field: " + field;
            logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value audioRoot = encodingProfileDetailsRoot[field]; 

        // codec
        {
            field = "codec";
            if (!JSONUtils::isMetadataPresent(audioRoot, field))
            {
                string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
                        + ", Field: " + field;
                logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string codec = JSONUtils::asString(audioRoot, field, "");

            FFMpegEncodingParameters::encodingAudioCodecValidation(codec, logger);

            ffmpegAudioCodecParameter   =
                    "-acodec " + codec + " "
            ;
        }

        // kBitRate
		/*
        {
            field = "kBitRate";
            if (JSONUtils::isMetadataPresent(audioRoot, field))
            {
                int bitRate = JSONUtils::asInt(audioRoot, field, 0);

                ffmpegAudioBitRateParameter =
                        "-b:a " + to_string(bitRate) + "k "
                ;
            }
        }
		*/
        
        // OtherOutputParameters
        {
            field = "otherOutputParameters";
            if (JSONUtils::isMetadataPresent(audioRoot, field))
            {
                string otherOutputParameters = JSONUtils::asString(audioRoot, field, "");

                ffmpegAudioOtherParameters =
                        otherOutputParameters + " "
                ;
            }
        }

        // channelsNumber
        {
            field = "channelsNumber";
            if (JSONUtils::isMetadataPresent(audioRoot, field))
            {
                int channelsNumber = JSONUtils::asInt(audioRoot, field, 0);

                ffmpegAudioChannelsParameter =
                        "-ac " + to_string(channelsNumber) + " "
                ;
            }
        }

        // sample rate
        {
            field = "sampleRate";
            if (JSONUtils::isMetadataPresent(audioRoot, field))
            {
                int sampleRate = JSONUtils::asInt(audioRoot, field, 0);

                ffmpegAudioSampleRateParameter =
                        "-ar " + to_string(sampleRate) + " "
                ;
            }
        }

		field = "bitRates";
		if (!JSONUtils::isMetadataPresent(audioRoot, field))
		{
			string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
				+ ", Field: " + field;
			logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		Json::Value bitRatesRoot = audioRoot[field];

		audioBitRatesInfo.clear();
		{
			for (int bitRateIndex = 0; bitRateIndex < bitRatesRoot.size(); bitRateIndex++)
			{
				Json::Value bitRateInfo = bitRatesRoot[bitRateIndex];

				string ffmpegAudioBitRate;
				{
					field = "kBitRate";
					if (!JSONUtils::isMetadataPresent(bitRateInfo, field))
					{
						string errorMessage = __FILEREF__ + "FFMpeg: Field is not present or it is null"
							+ ", Field: " + field;
						logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					int kBitRate = JSONUtils::asInt(bitRateInfo, field, 0);

					ffmpegAudioBitRate = "-b:a " + to_string(kBitRate) + "k ";
				}

				audioBitRatesInfo.push_back(ffmpegAudioBitRate);
			}
		}
    }
}

void FFMpegEncodingParameters::addToArguments(string parameter, vector<string>& argumentList)
{
	// _logger->info(__FILEREF__ + "addToArguments"
	// 		+ ", parameter: " + parameter
	// );

	if (parameter != "")
	{
		string item;
		stringstream parameterStream(parameter);

		while(getline(parameterStream, item, ' '))
		{
			if (item != "")
				argumentList.push_back(item);
		}
	}
}

void FFMpegEncodingParameters::encodingFileFormatValidation(string fileFormat,
        shared_ptr<spdlog::logger> logger)
{
	string fileFormatLowerCase;
	fileFormatLowerCase.resize(fileFormat.size());
	transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(),
		[](unsigned char c){return tolower(c); } );

    if (fileFormatLowerCase != "3gp" 
		&& fileFormatLowerCase != "mp4" 
		&& fileFormatLowerCase != "mov"
		&& fileFormatLowerCase != "webm" 
		&& fileFormatLowerCase != "hls"
		&& fileFormatLowerCase != "dash"
		&& fileFormatLowerCase != "ts"
		&& fileFormatLowerCase != "mts"
		&& fileFormatLowerCase != "mkv"
		&& fileFormatLowerCase != "avi"
		&& fileFormatLowerCase != "flv"
		&& fileFormatLowerCase != "ogg"
		&& fileFormatLowerCase != "wmv"
		&& fileFormatLowerCase != "yuv"
		&& fileFormatLowerCase != "mpg"
		&& fileFormatLowerCase != "mpeg"
		&& fileFormatLowerCase != "mjpeg"
		&& fileFormatLowerCase != "mxf"
	)
    {
        string errorMessage = __FILEREF__ + "FFMpeg: fileFormat is wrong"
                + ", fileFormatLowerCase: " + fileFormatLowerCase;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpegEncodingParameters::encodingAudioCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger)
{
    if (codec != "aac" 
            && codec != "libfdk_aac" 
            && codec != "libvo_aacenc" 
            && codec != "libvorbis"
            && codec != "pcm_s16le"
            && codec != "pcm_s32le"
    )
    {
        string errorMessage = __FILEREF__ + "FFMpeg: Audio codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpegEncodingParameters::encodingVideoProfileValidation(
        string codec, string profile,
        shared_ptr<spdlog::logger> logger)
{
    if (codec == "libx264")
    {
        if (profile != "high" && profile != "baseline" && profile != "main"
				&& profile != "high422"	// used in case of mxf
			)
        {
            string errorMessage = __FILEREF__ + "FFMpeg: Profile is wrong"
                    + ", codec: " + codec
                    + ", profile: " + profile;

            logger->error(errorMessage);
        
            throw runtime_error(errorMessage);
        }
    }
    else if (codec == "libvpx")
    {
        if (profile != "best" && profile != "good")
        {
            string errorMessage = __FILEREF__ + "FFMpeg: Profile is wrong"
                    + ", codec: " + codec
                    + ", profile: " + profile;

            logger->error(errorMessage);
        
            throw runtime_error(errorMessage);
        }
    }
    else
    {
        string errorMessage = __FILEREF__ + "FFMpeg: codec is wrong"
                + ", codec: " + codec;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void FFMpegEncodingParameters::removeTwoPassesTemporaryFiles()
{
    try
    {
		string prefixPasslogFileName = 
			to_string(_ingestionJobKey)
			+ "_"
			+ to_string(_encodingJobKey)
		;

		for (fs::directory_entry const& entry: fs::directory_iterator(_ffmpegTempDir))
        {
            try
            {
                if (!entry.is_regular_file())
                    continue;

                if (entry.path().filename().string().size() >= prefixPasslogFileName.size()
					&& entry.path().filename().string().compare(0, prefixPasslogFileName.size(), prefixPasslogFileName) == 0) 
                {
                    _logger->info(__FILEREF__ + "Remove"
                        + ", pathFileName: " + entry.path().string());
                    fs::remove_all(entry.path());
                }
            }
            catch(runtime_error& e)
            {
                string errorMessage = __FILEREF__ + "listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
            catch(exception& e)
            {
                string errorMessage = __FILEREF__ + "listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
        }
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "removeTwoPassesTemporaryFiles failed"
            + ", e.what(): " + e.what()
        );
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "removeTwoPassesTemporaryFiles failed");
    }
}

string FFMpegEncodingParameters::getMultiTrackEncodedStagingTemplateAssetPathName()
{

	size_t extensionIndex = _encodedStagingAssetPathName.find_last_of(".");
	if (extensionIndex == string::npos)
	{
		string errorMessage = __FILEREF__ + "No extension found"
			+ ", _encodedStagingAssetPathName: " + _encodedStagingAssetPathName;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	// I tried the string::insert method but it did not work
	return _encodedStagingAssetPathName.substr(0, extensionIndex)
		+ "_" + _multiTrackTemplatePart
		+ _encodedStagingAssetPathName.substr(extensionIndex)
	;
}

bool FFMpegEncodingParameters::getMultiTrackPathNames(vector<string>& sourcesPathName)
{
	if (_videoBitRatesInfo.size() <= 1)
		return false;	// no multi tracks

	// all the tracks generated in different files have to be copied
	// into the encodedStagingAssetPathName file
	// The command willl be:
	//		ffmpeg -i ... -i ... -c copy -map 0 -map 1 ... <dest file>

	sourcesPathName.clear();

	for (int videoIndex = 0; videoIndex < _videoBitRatesInfo.size(); videoIndex++)
	{
		tuple<string, int, int, int, string, string, string> videoBitRateInfo
			= _videoBitRatesInfo [videoIndex];

		int videoHeight = -1;

		tie(ignore, ignore,
			ignore, videoHeight,
			ignore, ignore,
			ignore) = videoBitRateInfo;

		string encodedStagingTemplateAssetPathName = getMultiTrackEncodedStagingTemplateAssetPathName();

		string newStagingEncodedAssetPathName =
			regex_replace(encodedStagingTemplateAssetPathName,
				regex(_multiTrackTemplateVariable), to_string(videoHeight));
		sourcesPathName.push_back(newStagingEncodedAssetPathName);
	}

	return true;	// yes multi tracks
}

void FFMpegEncodingParameters::removeMultiTrackPathNames()
{
	vector<string> sourcesPathName;

	if (!getMultiTrackPathNames(sourcesPathName))
		return;	// no multi tracks

	for (string sourcePathName: sourcesPathName)
	{
		_logger->info(__FILEREF__ + "Remove"
			+ ", sourcePathName: " + sourcePathName);
		fs::remove_all(sourcePathName);
	}
}

