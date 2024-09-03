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
#include "FFMpeg.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include <fstream>
/*
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "spdlog/fmt/fmt.h"
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
*/

pair<string, string> FFMpeg::retrieveStreamingYouTubeURL(int64_t ingestionJobKey, string youTubeURL)
{
	SPDLOG_INFO(
		"retrieveStreamingYouTubeURL"
		", youTubeURL: {}"
		", ingestionJobKey: {}",
		youTubeURL, ingestionJobKey
	);

	string detailsYouTubeProfilesPath;
	{
		detailsYouTubeProfilesPath = fmt::format("{}/{}-youTubeProfiles.txt", _ffmpegTempDir, ingestionJobKey);

		// string youTubeExecuteCommand = _pythonPathName + " " + _youTubeDlPath + "/youtube-dl " + "--list-formats " + youTubeURL + " " + " > " +
		// 						   detailsYouTubeProfilesPath + " 2>&1";
		string youTubeExecuteCommand =
			fmt::format("{} {}/youtube-dl --list-formats {} > {} 2>&1", _pythonPathName, _youTubeDlPath, youTubeURL, detailsYouTubeProfilesPath);

		try
		{
			SPDLOG_INFO(
				"retrieveStreamingYouTubeURL: Executing youtube command"
				", ingestionJobKey: {}"
				", youTubeExecuteCommand: {}",
				ingestionJobKey, youTubeExecuteCommand
			);

			chrono::system_clock::time_point startYouTubeCommand = chrono::system_clock::now();

			int executeCommandStatus = ProcessUtility::execute(youTubeExecuteCommand);
			if (executeCommandStatus != 0)
			{
				// it could be also that the live is not available
				// ERROR: f2vW_XyTW4o: YouTube said: This live stream recording is not available.

				string lastPartOfFfmpegOutputFile;
				if (fs::exists(detailsYouTubeProfilesPath))
					lastPartOfFfmpegOutputFile = getLastPartOfFile(detailsYouTubeProfilesPath, _charsToBeReadFromFfmpegErrorOutput);
				else
					lastPartOfFfmpegOutputFile = string("file not found: ") + detailsYouTubeProfilesPath;

				string errorMessage = fmt::format(
					"retrieveStreamingYouTubeURL: youTube command failed"
					", ingestionJobKey: {}"
					", executeCommandStatus: {}"
					", youTubeExecuteCommand: {}"
					", lastPartOfFfmpegOutputFile: {}",
					ingestionJobKey, executeCommandStatus, youTubeExecuteCommand, lastPartOfFfmpegOutputFile
				);
				SPDLOG_ERROR(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = fmt::format(
					"retrieveStreamingYouTubeURL: command failed"
					", ingestionJobKey: {}",
					ingestionJobKey
				);
				throw runtime_error(errorMessage);
			}
			else if (!fs::exists(detailsYouTubeProfilesPath))
			{
				string errorMessage = fmt::format(
					"retrieveStreamingYouTubeURL: youTube command failed. no profiles file created"
					", ingestionJobKey: {}"
					", executeCommandStatus: {}"
					", youTubeExecuteCommand: {}",
					ingestionJobKey, executeCommandStatus, youTubeExecuteCommand
				);
				SPDLOG_ERROR(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = fmt::format(
					"retrieveStreamingYouTubeURL: command failed. no profiles file created"
					", ingestionJobKey: {}",
					ingestionJobKey
				);
				throw runtime_error(errorMessage);
			}

			chrono::system_clock::time_point endYouTubeCommand = chrono::system_clock::now();

			SPDLOG_INFO(
				"retrieveStreamingYouTubeURL: Executed youTube command"
				", ingestionJobKey: {}"
				", youTubeExecuteCommand: {}"
				", detailsYouTubeProfilesPath size: {}"
				", @FFMPEG statistics@ - duration (secs): @{}@",
				ingestionJobKey, youTubeExecuteCommand, fs::file_size(detailsYouTubeProfilesPath),
				chrono::duration_cast<chrono::seconds>(endYouTubeCommand - startYouTubeCommand).count()
			);
		}
		catch (runtime_error &e)
		{
			string errorMessage = fmt::format(
				"retrieveStreamingYouTubeURL, youTube command failed"
				", ingestionJobKey: {}"
				", e.what(): {}",
				ingestionJobKey, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			if (fs::exists(detailsYouTubeProfilesPath))
			{
				SPDLOG_INFO(
					"Remove"
					", detailsYouTubeProfilesPath: {}",
					detailsYouTubeProfilesPath
				);
				fs::remove_all(detailsYouTubeProfilesPath);
			}

			throw e;
		}
		catch (exception &e)
		{
			string errorMessage = fmt::format(
				"retrieveStreamingYouTubeURL, youTube command failed"
				", ingestionJobKey: {}"
				", e.what(): {}",
				ingestionJobKey, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			if (fs::exists(detailsYouTubeProfilesPath))
			{
				SPDLOG_INFO(
					"Remove"
					", detailsYouTubeProfilesPath: {}",
					detailsYouTubeProfilesPath
				);
				fs::remove_all(detailsYouTubeProfilesPath);
			}

			throw e;
		}
	}

	int selectedFormatCode = -1;
	string extension;
	try
	{
		// txt output will be like:
		/*
[youtube] f2vW_XyTW4o: Downloading webpage
[youtube] f2vW_XyTW4o: Downloading m3u8 information
[youtube] f2vW_XyTW4o: Downloading MPD manifest
[info] Available formats for f2vW_XyTW4o:
format code  extension  resolution note
91           mp4        256x144    HLS  197k , avc1.42c00b, 30.0fps, mp4a.40.5@ 48k
92           mp4        426x240    HLS  338k , avc1.4d4015, 30.0fps, mp4a.40.5@ 48k
93           mp4        640x360    HLS  829k , avc1.4d401e, 30.0fps, mp4a.40.2@128k
94           mp4        854x480    HLS 1380k , avc1.4d401f, 30.0fps, mp4a.40.2@128k
95           mp4        1280x720   HLS 2593k , avc1.4d401f, 30.0fps, mp4a.40.2@256k (best)
		*/

		ifstream detailsFile(detailsYouTubeProfilesPath);
		string line;
		bool formatCodeLabelFound = false;
		int lastFormatCode = -1;
		int bestFormatCode = -1;
		while (getline(detailsFile, line))
		{
			_logger->info(
				__FILEREF__ + "retrieveStreamingYouTubeURL, Details youTube profiles" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath + ", formatCodeLabelFound: " + to_string(formatCodeLabelFound) +
				", lastFormatCode: " + to_string(lastFormatCode) + ", bestFormatCode: " + to_string(bestFormatCode) + ", line: " + line
			);

			if (formatCodeLabelFound)
			{
				int lastDigit = 0;
				while (lastDigit < line.length() && isdigit(line[lastDigit]))
					lastDigit++;
				if (lastDigit > 0)
				{
					string formatCode = line.substr(0, lastDigit);
					lastFormatCode = stoi(formatCode);

					if (line.find("(best)") != string::npos)
						bestFormatCode = lastFormatCode;

					int startExtensionIndex = lastDigit;
					while (startExtensionIndex < line.length() && isspace(line[startExtensionIndex]))
						startExtensionIndex++;
					int endExtensionIndex = startExtensionIndex;
					while (endExtensionIndex < line.length() && !isspace(line[endExtensionIndex]))
						endExtensionIndex++;

					extension = line.substr(startExtensionIndex, endExtensionIndex - startExtensionIndex);
				}
			}
			else if (line.find("format code") != string::npos)
				formatCodeLabelFound = true;
		}

		_logger->info(
			__FILEREF__ + "retrieveStreamingYouTubeURL, Details youTube profiles, final info" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath + ", formatCodeLabelFound: " + to_string(formatCodeLabelFound) +
			", lastFormatCode: " + to_string(lastFormatCode) + ", bestFormatCode: " + to_string(bestFormatCode) + ", line: " + line
		);

		{
			_logger->info(
				__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath
			);
			fs::remove_all(detailsYouTubeProfilesPath);
		}

		if (bestFormatCode != -1)
			selectedFormatCode = bestFormatCode;
		else if (lastFormatCode != -1)
			selectedFormatCode = lastFormatCode;
		else
		{
			string errorMessage =
				__FILEREF__ + "retrieveStreamingYouTubeURL: no format code found" + ", ingestionJobKey: " + to_string(ingestionJobKey);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL: profile error processing or format code not found" +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		if (fs::exists(detailsYouTubeProfilesPath))
		{
			_logger->info(__FILEREF__ + "Remove" + ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath);
			fs::remove_all(detailsYouTubeProfilesPath);
		}

		throw e;
	}
	catch (exception &e)
	{
		string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL: profiles error processing" +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
		_logger->error(errorMessage);

		if (fs::exists(detailsYouTubeProfilesPath))
		{
			_logger->info(__FILEREF__ + "Remove" + ", detailsYouTubeProfilesPath: " + detailsYouTubeProfilesPath);
			fs::remove_all(detailsYouTubeProfilesPath);
		}

		throw e;
	}

	string streamingYouTubeURL;
	{
		string detailsYouTubeURLPath = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + "-youTubeUrl.txt";

		string youTubeExecuteCommand = _pythonPathName + " " + _youTubeDlPath + "/youtube-dl " + "-f " + to_string(selectedFormatCode) + " " + "-g " +
									   youTubeURL + " " + " > " + detailsYouTubeURLPath + " 2>&1";

		try
		{
			_logger->info(
				__FILEREF__ + "retrieveStreamingYouTubeURL: Executing youtube command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", youTubeExecuteCommand: " + youTubeExecuteCommand
			);

			chrono::system_clock::time_point startYouTubeCommand = chrono::system_clock::now();

			int executeCommandStatus = ProcessUtility::execute(youTubeExecuteCommand);
			if (executeCommandStatus != 0)
			{
				string errorMessage =
					__FILEREF__ + "retrieveStreamingYouTubeURL: youTube command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", executeCommandStatus: " + to_string(executeCommandStatus) + ", youTubeExecuteCommand: " + youTubeExecuteCommand;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL: command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}
			else if (!fs::exists(detailsYouTubeURLPath))
			{
				string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL: youTube command failed. no URL file created" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", executeCommandStatus: " + to_string(executeCommandStatus) +
									  ", youTubeExecuteCommand: " + youTubeExecuteCommand;
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL: command failed. no URL file created" +
							   ", ingestionJobKey: " + to_string(ingestionJobKey);
				throw runtime_error(errorMessage);
			}

			chrono::system_clock::time_point endYouTubeCommand = chrono::system_clock::now();

			_logger->info(
				__FILEREF__ + "retrieveStreamingYouTubeURL: Executed youTube command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", youTubeExecuteCommand: " + youTubeExecuteCommand + ", @FFMPEG statistics@ - duration (secs): @" +
				to_string(chrono::duration_cast<chrono::seconds>(endYouTubeCommand - startYouTubeCommand).count()) + "@"
			);

			{
				ifstream urlFile(detailsYouTubeURLPath);
				std::stringstream buffer;
				buffer << urlFile.rdbuf();

				streamingYouTubeURL = buffer.str();
				streamingYouTubeURL = StringUtils::trimNewLineToo(streamingYouTubeURL);

				_logger->info(
					__FILEREF__ + "retrieveStreamingYouTubeURL: Executed youTube command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", youTubeExecuteCommand: " + youTubeExecuteCommand + ", streamingYouTubeURL: " + streamingYouTubeURL
				);
			}

			{
				_logger->info(__FILEREF__ + "Remove" + ", detailsYouTubeURLPath: " + detailsYouTubeURLPath);
				fs::remove_all(detailsYouTubeProfilesPath);
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL, youTube command failed" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
			_logger->error(errorMessage);

			if (fs::exists(detailsYouTubeURLPath))
			{
				_logger->info(__FILEREF__ + "Remove" + ", detailsYouTubeURLPath: " + detailsYouTubeURLPath);
				fs::remove_all(detailsYouTubeURLPath);
			}

			throw e;
		}
		catch (exception &e)
		{
			string errorMessage = __FILEREF__ + "retrieveStreamingYouTubeURL, youTube command failed" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
			_logger->error(errorMessage);

			if (fs::exists(detailsYouTubeURLPath))
			{
				_logger->info(__FILEREF__ + "Remove" + ", detailsYouTubeURLPath: " + detailsYouTubeURLPath);
				fs::remove_all(detailsYouTubeProfilesPath);
			}

			throw e;
		}
	}

	return make_pair(streamingYouTubeURL, extension);
}
