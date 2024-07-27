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
#include "FFMpegEncodingParameters.h"
#include "FFMpegFilters.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "catralibraries/ProcessUtility.h"
#include <fstream>
#include <regex>

void FFMpeg::liveProxy2(
	int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder, long maxStreamingDurationInMinutes, mutex *inputsRootMutex,
	json *inputsRoot, json outputsRoot, pid_t *pChildPid, chrono::system_clock::time_point *pProxyStart, long *numberOfRestartBecauseOfFailure
)
{
	_currentApiName = APIName::LiveProxy;

	_logger->info(
		__FILEREF__ + "Received " + toString(_currentApiName) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", encodingJobKey: " + to_string(encodingJobKey) + ", inputsRoot->size: " + to_string(inputsRoot->size())
	);

	setStatus(
		ingestionJobKey, encodingJobKey
		/*
		videoDurationInMilliSeconds,
		mmsAssetPathName
		stagingEncodedAssetPathName
		*/
	);

	// Creating multi outputs: https://trac.ffmpeg.org/wiki/Creating%20multiple%20outputs
	if (inputsRoot->size() == 0)
	{
		string errorMessage = __FILEREF__ + "liveProxy. No input parameters" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", encodingJobKey: " + to_string(encodingJobKey) + ", inputsRoot->size: " + to_string(inputsRoot->size());
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	if (outputsRoot.size() == 0)
	{
		string errorMessage = __FILEREF__ + "liveProxy. No output parameters" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", encodingJobKey: " + to_string(encodingJobKey) + ", outputsRoot.size: " + to_string(outputsRoot.size());
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	// _logger->info(__FILEREF__ + "Calculating timedInput"
	// 	+ ", ingestionJobKey: " + to_string(ingestionJobKey)
	// 	+ ", encodingJobKey: " + to_string(encodingJobKey)
	// 	+ ", inputsRoot->size: " + to_string(inputsRoot->size())
	// );
	bool timedInput = true;
	{
		lock_guard<mutex> locker(*inputsRootMutex);

		for (int inputIndex = 0; inputIndex < inputsRoot->size(); inputIndex++)
		{
			json inputRoot = (*inputsRoot)[inputIndex];

			bool timePeriod = false;
			string field = "timePeriod";
			timePeriod = JSONUtils::asBool(inputRoot, field, false);

			int64_t utcProxyPeriodStart = -1;
			field = "utcScheduleStart";
			utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// else
			// {
			// 	field = "utcProxyPeriodStart";
			// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
			// 		utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			int64_t utcProxyPeriodEnd = -1;
			field = "utcScheduleEnd";
			utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// else
			// {
			// 	field = "utcProxyPeriodEnd";
			// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
			// 		utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			if (!timePeriod || utcProxyPeriodStart == -1 || utcProxyPeriodEnd == -1)
			{
				timedInput = false;

				break;
			}
		}
	}
	_logger->info(
		__FILEREF__ + "Calculated timedInput" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " +
		to_string(encodingJobKey) + ", inputsRoot->size: " + to_string(inputsRoot->size()) + ", timedInput: " + to_string(timedInput)
	);

	if (timedInput)
	{
		int64_t utcFirstProxyPeriodStart = -1;
		int64_t utcLastProxyPeriodEnd = -1;
		{
			lock_guard<mutex> locker(*inputsRootMutex);

			for (int inputIndex = 0; inputIndex < inputsRoot->size(); inputIndex++)
			{
				json inputRoot = (*inputsRoot)[inputIndex];

				string field = "utcScheduleStart";
				int64_t utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
				// if (utcProxyPeriodStart == -1)
				// {
				// 	field = "utcProxyPeriodStart";
				// 	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
				// }
				if (utcFirstProxyPeriodStart == -1)
					utcFirstProxyPeriodStart = utcProxyPeriodStart;

				field = "utcScheduleEnd";
				utcLastProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
				// if (utcLastProxyPeriodEnd == -1)
				// {
				// 	field = "utcProxyPeriodEnd";
				// 	utcLastProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
				// }
			}
		}

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		if (utcNow < utcFirstProxyPeriodStart)
		{
			while (utcNow < utcFirstProxyPeriodStart)
			{
				time_t sleepTime = utcFirstProxyPeriodStart - utcNow;

				_logger->info(
					__FILEREF__ + "LiveProxy timing. " + "Too early to start the LiveProxy, just sleep " + to_string(sleepTime) + " seconds" +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
					", utcNow: " + to_string(utcNow) + ", utcFirstProxyPeriodStart: " + to_string(utcFirstProxyPeriodStart)
				);

				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}
			}
		}
		else if (utcLastProxyPeriodEnd < utcNow)
		{
			time_t tooLateTime = utcNow - utcLastProxyPeriodEnd;

			string errorMessage = __FILEREF__ + "LiveProxy timing. " + "Too late to start the LiveProxy" +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
								  ", utcNow: " + to_string(utcNow) + ", utcLastProxyPeriodEnd: " + to_string(utcLastProxyPeriodEnd) +
								  ", tooLateTime: " + to_string(tooLateTime);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	// max repeating is 1 because:
	//	- we have to return to the engine because the engine has to register the failure
	//	- if we increase 'max repeating':
	//		- transcoder does not return to engine even in case of failure (max repeating is > 1)
	//		- engine calls getEncodingStatus and get a 'success' (transcoding is repeating and
	//			the failure is not raised to the engine). So failures number engine variable is set to 0
	//		- transcoder, after repeating, raise the failure to engine but the engine,
	//			as mentioned before, already reset failures number to 0
	//	The result is that engine never reach max number of failures and encoding request,
	//	even if it is failing, never exit from the engine loop (EncoderVideoAudioProxy.cpp)

	//	In case ffmpeg fails after at least XXX minutes, this is not considered a failure
	//	and it will be executed again. This is very important because it makes sure ffmpeg
	//	is not failing continuously without working at all.
	//	Here follows some scenarios where it is important to execute again ffmpeg and not returning to
	//	EncoderVideoAudioProxy:
	// case 1:
	// 2022-10-20. scenario (ffmpeg is a server):
	//	- (1) streamSourceType is IP_PUSH
	//	- (2) the client just disconnected because of a client issue
	//	- (3) ffmpeg exit too early
	// In this case ffmpeg has to return to listen as soon as possible for a new connection.
	// In case we return an exception it will pass about 10-15 seconds before ffmpeg returns
	// to be executed and listen again for a new connection.
	// To make ffmpeg to listen as soon as possible, we will not return an exception
	// da almeno XXX secondi (4)
	// case 2:
	// 2022-10-26. scenario (ffmpeg is a client):
	//	Nel caso in cui devono essere "ripetuti" multiple inputs, vedi commento 2022-10-27.
	//  In questo scenario quando ffmpeg termina la prima ripetizione, deve essere eseguito
	//	nuovamente per la successiva ripetizione.
	int maxTimesRepeatingSameInput = 1;
	int currentNumberOfRepeatingSameInput = 0;
	int sleepInSecondsInCaseOfRepeating = 5;
	int currentInputIndex = -1;
	int previousInputIndex = -1;
	json currentInputRoot;
	while ((currentInputIndex =
				getNextLiveProxyInput(ingestionJobKey, encodingJobKey, inputsRoot, inputsRootMutex, currentInputIndex, timedInput, &currentInputRoot)
		   ) != -1)
	{
		vector<string> ffmpegInputArgumentList;
		long streamingDurationInSeconds = -1;
		string otherOutputOptionsBecauseOfMaxWidth;
		string endlessPlaylistListPathName;
		int pushListenTimeout;
		int64_t utcProxyPeriodStart;
		json inputFiltersRoot;

		if (previousInputIndex == -1)
			previousInputIndex = currentInputIndex;
		else
		{
			if (previousInputIndex == currentInputIndex)
				(*numberOfRestartBecauseOfFailure)++;
			else
				previousInputIndex = currentInputIndex;
		}

		try
		{
			_logger->info(
				__FILEREF__ + "liveProxyInput..." + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", inputsRoot->size: " + to_string(inputsRoot->size()) +
				", timedInput: " + to_string(timedInput) + ", currentInputIndex: " + to_string(currentInputIndex)
			);
			tuple<long, string, string, int, int64_t, json> inputDetails = liveProxyInput(
				ingestionJobKey, encodingJobKey, externalEncoder, currentInputRoot, maxStreamingDurationInMinutes, ffmpegInputArgumentList
			);
			tie(streamingDurationInSeconds, otherOutputOptionsBecauseOfMaxWidth, endlessPlaylistListPathName, pushListenTimeout, utcProxyPeriodStart,
				inputFiltersRoot /*, inputVideoTracks, inputAudioTracks*/) = inputDetails;

			{
				ostringstream ffmpegInputArgumentListStream;
				if (!ffmpegInputArgumentList.empty())
					copy(
						ffmpegInputArgumentList.begin(), ffmpegInputArgumentList.end(), ostream_iterator<string>(ffmpegInputArgumentListStream, " ")
					);
				_logger->info(
					__FILEREF__ + "liveProxy: ffmpegInputArgumentList" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", externalEncoder: " + to_string(externalEncoder) + ", currentInputRoot: " +
					JSONUtils::toString(currentInputRoot) + ", ffmpegInputArgumentList: " + ffmpegInputArgumentListStream.str()
				);
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong input parameters" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
								  ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) + ", exception: " + e.what();
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong input parameters" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
								  ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) + ", exception: " + e.what();
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> ffmpegOutputArgumentList;
		try
		{
			_logger->info(
				__FILEREF__ + "outputsRootToFfmpeg..." + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", outputsRoot.size: " + to_string(outputsRoot.size())
			);
			outputsRootToFfmpeg(
				ingestionJobKey, encodingJobKey, externalEncoder, otherOutputOptionsBecauseOfMaxWidth, inputFiltersRoot,
				// inputVideoTracks, inputAudioTracks,
				streamingDurationInSeconds, outputsRoot, ffmpegOutputArgumentList
			);

			{
				ostringstream ffmpegOutputArgumentListStream;
				if (!ffmpegOutputArgumentList.empty())
					copy(
						ffmpegOutputArgumentList.begin(), ffmpegOutputArgumentList.end(),
						ostream_iterator<string>(ffmpegOutputArgumentListStream, " ")
					);
				_logger->info(
					__FILEREF__ + "liveProxy: ffmpegOutputArgumentList" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", ffmpegOutputArgumentList: " + ffmpegOutputArgumentListStream.str()
				);
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong output parameters" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
								  ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) + ", exception: " + e.what();
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong output parameters" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
								  ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) + ", exception: " + e.what();
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		ostringstream ffmpegArgumentListStream;
		int iReturnedStatus = 0;
		chrono::system_clock::time_point startFfmpegCommand;
		chrono::system_clock::time_point endFfmpegCommand;

		try
		{
			{
				char sUtcTimestamp[64];
				tm tmUtcTimestamp;
				time_t utcTimestamp = chrono::system_clock::to_time_t(chrono::system_clock::now());

				localtime_r(&utcTimestamp, &tmUtcTimestamp);
				sprintf(
					sUtcTimestamp, "%04d-%02d-%02d-%02d-%02d-%02d", tmUtcTimestamp.tm_year + 1900, tmUtcTimestamp.tm_mon + 1, tmUtcTimestamp.tm_mday,
					tmUtcTimestamp.tm_hour, tmUtcTimestamp.tm_min, tmUtcTimestamp.tm_sec
				);

				_outputFfmpegPathFileName = fmt::format(
					"{}/{}_{}_{}_{}.{}.log", _ffmpegTempDir, "liveProxy", _currentIngestionJobKey, _currentEncodingJobKey, sUtcTimestamp,
					currentInputIndex
				);
			}

			vector<string> ffmpegArgumentList;

			ffmpegArgumentList.push_back("ffmpeg");
			for (string parameter : ffmpegInputArgumentList)
				ffmpegArgumentList.push_back(parameter);
			for (string parameter : ffmpegOutputArgumentList)
				ffmpegArgumentList.push_back(parameter);

			time_t utcNow;
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

			if (!ffmpegArgumentList.empty())
				copy(ffmpegArgumentList.begin(), ffmpegArgumentList.end(), ostream_iterator<string>(ffmpegArgumentListStream, " "));

			_logger->info(
				__FILEREF__ + "liveProxy: Executing ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
				", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
				", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str()
			);

			startFfmpegCommand = chrono::system_clock::now();

			bool redirectionStdOutput = true;
			bool redirectionStdError = true;

			ProcessUtility::forkAndExec(
				_ffmpegPath + "/ffmpeg", ffmpegArgumentList, _outputFfmpegPathFileName, redirectionStdOutput, redirectionStdError, pChildPid,
				&iReturnedStatus
			);
			*pChildPid = 0;

			endFfmpegCommand = chrono::system_clock::now();

			if (iReturnedStatus != 0)
			{
				string errorMessage = __FILEREF__ + "liveProxy: Executed ffmpeg command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
									  ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
									  ", iReturnedStatus: " + to_string(iReturnedStatus) +
									  ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
									  ", ffmpegArgumentList: " + ffmpegArgumentListStream.str();
				_logger->error(errorMessage);

				// to hide the ffmpeg staff
				errorMessage = __FILEREF__ + "liveProxy: command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							   ", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
							   ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput);
				throw runtime_error(errorMessage);
			}

			_logger->info(
				__FILEREF__ + "liveProxy: Executed ffmpeg command" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
				", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
				", ffmpegArgumentList: " + ffmpegArgumentListStream.str() + ", @FFMPEG statistics@ - ffmpegCommandDuration (secs): @" +
				to_string(chrono::duration_cast<chrono::seconds>(endFfmpegCommand - startFfmpegCommand).count()) + "@"
			);

			if (endlessPlaylistListPathName != "" && fs::exists(endlessPlaylistListPathName))
			{
				if (externalEncoder)
				{
					ifstream ifConfigurationFile(endlessPlaylistListPathName);
					if (ifConfigurationFile)
					{
						string configuration;
						string prefixFile = "file '";
						while (getline(ifConfigurationFile, configuration))
						{
							if (configuration.size() >= prefixFile.size() && 0 == configuration.compare(0, prefixFile.size(), prefixFile))
							{
								string mediaFileName = configuration.substr(prefixFile.size(), configuration.size() - prefixFile.size() - 1);

								SPDLOG_INFO(
									"Remove"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", _ffmpegEndlessRecursivePlaylist: {}",
									ingestionJobKey, encodingJobKey, _ffmpegEndlessRecursivePlaylistDir + "/" + mediaFileName
								);
								fs::remove_all(_ffmpegEndlessRecursivePlaylistDir + "/" + mediaFileName);
							}
						}

						ifConfigurationFile.close();
					}
				}

				_logger->info(
					__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
					", endlessPlaylistListPathName: " + endlessPlaylistListPathName
				);
				fs::remove_all(endlessPlaylistListPathName);
				endlessPlaylistListPathName = "";
			}

			try
			{
				outputsRootToFfmpeg_clean(ingestionJobKey, encodingJobKey, outputsRoot, externalEncoder);
			}
			catch (runtime_error &e)
			{
				string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
				_logger->error(errorMessage);

				// throw e;
			}
			catch (exception &e)
			{
				string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
				_logger->error(errorMessage);

				// throw e;
			}

			if (streamingDurationInSeconds != -1 && endFfmpegCommand - startFfmpegCommand < chrono::seconds(streamingDurationInSeconds - 60))
			{
				throw runtime_error(string("liveProxy exit before unexpectly, tried ") + to_string(maxTimesRepeatingSameInput) + " times");
			}
			else
			{
				// we finished one input and, may be, go to the next input

				currentNumberOfRepeatingSameInput = 0;
			}
		}
		catch (runtime_error &e)
		{
			*pChildPid = 0;

			bool stoppedBySigQuitOrTerm = false;

			string lastPartOfFfmpegOutputFile = getLastPartOfFile(_outputFfmpegPathFileName, _charsToBeReadFromFfmpegErrorOutput);
			string errorMessage;
			if (iReturnedStatus == 9) // 9 means: SIGKILL
			{
				errorMessage = __FILEREF__ + "ffmpeg: ffmpeg execution command failed because killed by the user" +
							   ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
							   ", currentInputIndex: " + to_string(currentInputIndex) +
							   ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
							   ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName +
							   ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
							   ", lastPartOfFfmpegOutputFile: " + lastPartOfFfmpegOutputFile + ", e.what(): " + e.what();
			}
			else
			{
				// signal: 3 is what the LiveProxy playlist is changed and
				//		we need to use the new playlist
				// lastPartOfFfmpegOutputFile is like:
				//	Child has exit abnormally because of an uncaught signal. Terminating signal: 3
				// 2023-02-18: ho verificato che SIGQUIT non ha funzionato e il processo non si è stoppato,
				//	mentre ha funzionato SIGTERM, per cui ora sto usando SIGTERM
				if (lastPartOfFfmpegOutputFile.find("signal 3") != string::npos // SIGQUIT
					|| lastPartOfFfmpegOutputFile.find("signal: 3") != string::npos ||
					lastPartOfFfmpegOutputFile.find("signal 15") != string::npos // SIGTERM
					|| lastPartOfFfmpegOutputFile.find("signal: 15") != string::npos)
				{
					stoppedBySigQuitOrTerm = true;

					errorMessage =
						__FILEREF__ + "ffmpeg execution stopped by SIGQUIT/SIGTERM (3/15): ffmpeg command failed" +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
						", currentInputIndex: " + to_string(currentInputIndex) +
						", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
						", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
						", lastPartOfFfmpegOutputFile: " + regex_replace(lastPartOfFfmpegOutputFile, regex("\n"), " ") + ", e.what(): " + e.what();
				}
				else
				{
					errorMessage =
						__FILEREF__ + "ffmpeg: ffmpeg execution command failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
						", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) + ", ffmpegCommandDuration (secs): @" +
						to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startFfmpegCommand).count()) + "@" +
						", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName + ", ffmpegArgumentList: " + ffmpegArgumentListStream.str() +
						", lastPartOfFfmpegOutputFile: " + regex_replace(lastPartOfFfmpegOutputFile, regex("\n"), " ") + ", e.what(): " + e.what();
				}
			}
			_logger->error(errorMessage);

			/*
			_logger->info(__FILEREF__ + "Remove"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", currentInputIndex: " + to_string(currentInputIndex)
				+ ", currentNumberOfRepeatingSameInput: "
					+ to_string(currentNumberOfRepeatingSameInput)
				+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
			bool exceptionInCaseOfError = false;
			fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
			*/
			renameOutputFfmpegPathFileName(ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName);

			if (endlessPlaylistListPathName != "" && fs::exists(endlessPlaylistListPathName))
			{
				if (externalEncoder)
				{
					ifstream ifConfigurationFile(endlessPlaylistListPathName);
					if (ifConfigurationFile)
					{
						string configuration;
						string prefixFile = "file '";
						while (getline(ifConfigurationFile, configuration))
						{
							if (configuration.size() >= prefixFile.size() && 0 == configuration.compare(0, prefixFile.size(), prefixFile))
							{
								string mediaFileName = configuration.substr(prefixFile.size(), configuration.size() - prefixFile.size() - 1);

								SPDLOG_INFO(
									"Remove"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", _ffmpegEndlessRecursivePlaylist: {}",
									ingestionJobKey, encodingJobKey, _ffmpegEndlessRecursivePlaylistDir + "/" + mediaFileName
								);
								fs::remove_all(_ffmpegEndlessRecursivePlaylistDir + "/" + mediaFileName);
							}
						}

						ifConfigurationFile.close();
					}
				}

				_logger->info(
					__FILEREF__ + "Remove" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
					", endlessPlaylistListPathName: " + endlessPlaylistListPathName
				);
				fs::remove_all(endlessPlaylistListPathName);
				endlessPlaylistListPathName = "";
			}

			try
			{
				outputsRootToFfmpeg_clean(ingestionJobKey, encodingJobKey, outputsRoot, externalEncoder);
			}
			catch (runtime_error &e)
			{
				string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
				_logger->error(errorMessage);

				// throw e;
			}
			catch (exception &e)
			{
				string errorMessage = __FILEREF__ + "outputsRootToFfmpeg_clean failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", e.what(): " + e.what();
				_logger->error(errorMessage);

				// throw e;
			}

			// next code will decide to throw an exception or not (we are in an error scenario)

			if (iReturnedStatus == 9) // 9 means: SIGKILL
				throw FFMpegEncodingKilledByUser();
			else if (lastPartOfFfmpegOutputFile.find("403 Forbidden") != string::npos)
				throw FFMpegURLForbidden();
			else if (lastPartOfFfmpegOutputFile.find("404 Not Found") != string::npos)
				throw FFMpegURLNotFound();
			else if (!stoppedBySigQuitOrTerm)
			{
				// see the comment before 'while'
				if (
					// terminato troppo presto
					(streamingDurationInSeconds != -1 &&
						endFfmpegCommand - startFfmpegCommand <
							chrono::seconds(streamingDurationInSeconds - 60)
					)
					// per almeno XXX minuti ha strimmato correttamente
					&& endFfmpegCommand - startFfmpegCommand > chrono::seconds(5 * 60)
				)
				{
					_logger->info(
						__FILEREF__ + "Command has to be executed again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", encodingJobKey: " + to_string(encodingJobKey) + ", ffmpegCommandDuration (secs): @" +
						to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startFfmpegCommand).count()) + "@" +
						", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
						", currentInputIndex: " + to_string(currentInputIndex) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
					);

					// in case of IP_PUSH the monitor thread, in case the client does not
					// reconnect istantaneously, kills the process.
					// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
					// we will set again the liveProxy->_proxyStart variable
					{
						// pushListenTimeout in case it is not PUSH, it will be -1
						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
								*pProxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
							else
								*pProxyStart = chrono::system_clock::from_time_t(utcProxyPeriodStart) + chrono::seconds(pushListenTimeout);
						}
						else
							*pProxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
					}
				}
				else
				{
					currentNumberOfRepeatingSameInput++;
					if (currentNumberOfRepeatingSameInput >= maxTimesRepeatingSameInput)
					{
						_logger->info(
							__FILEREF__ + "Command is NOT executed anymore, reached max number of repeating" +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
							", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
							", maxTimesRepeatingSameInput: " + to_string(maxTimesRepeatingSameInput) +
							", sleepInSecondsInCaseOfRepeating: " + to_string(sleepInSecondsInCaseOfRepeating) +
							", currentInputIndex: " + to_string(currentInputIndex) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						);
						throw e;
					}
					else
					{
						_logger->info(
							__FILEREF__ + "Command is executed again" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " +
							to_string(encodingJobKey) + ", currentNumberOfRepeatingSameInput: " + to_string(currentNumberOfRepeatingSameInput) +
							", sleepInSecondsInCaseOfRepeating: " + to_string(sleepInSecondsInCaseOfRepeating) +
							", currentInputIndex: " + to_string(currentInputIndex) + ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName
						);

						// in case of IP_PUSH the monitor thread, in case the client does not
						// reconnect istantaneously, kills the process.
						// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
						// we will set again the liveProxy->_proxyStart variable
						{
							if (utcProxyPeriodStart != -1)
							{
								if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
									*pProxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
								else
									*pProxyStart = chrono::system_clock::from_time_t(utcProxyPeriodStart) + chrono::seconds(pushListenTimeout);
							}
							else
								*pProxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
						}

						currentInputIndex--;
						this_thread::sleep_for(chrono::seconds(sleepInSecondsInCaseOfRepeating));
					}
				}
			}
			else // if (stoppedBySigQuitOrTerm)
			{
				// in case of IP_PUSH the monitor thread, in case the client does not
				// reconnect istantaneously, kills the process.
				// In general, if ffmpeg restart, liveMonitoring has to wait, for this reason
				// we will set again the liveProxy->_proxyStart variable
				{
					if (utcProxyPeriodStart != -1)
					{
						if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
							*pProxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
						else
							*pProxyStart = chrono::system_clock::from_time_t(utcProxyPeriodStart) + chrono::seconds(pushListenTimeout);
					}
					else
						*pProxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
				}

				// 2022-10-21: this is the scenario where the LiveProxy playlist is changed (signal: 3)
				//	and we need to use the new playlist
				//	This is the ffmpeg 'client side'.
				//	The above condition (!stoppedBySigQuitOrTerm) is the scenario server side.
				//	This is what happens:
				//		time A: a new playlist is received by MMS and a SigQuit/SigTerm (signal: 3/15) is sent
				//			to the ffmpeg client side
				//		time A + few milliseconds: the ffmpeg client side starts again
				//			with the new 'input' (1)
				//		time A + few seconds: The server ffmpeg recognizes the client disconnect and exit
				//		time A + few seconds + few milliseconds: The ffmpeg server side starts again (2)
				//
				//	The problem is that The ffmpeg server starts too late (2). The ffmpeg client (1)
				//	already failed because the ffmpeg server was not listening yet.
				//	So ffmpeg client exit from this method, reach the engine and returns after about 15 seconds.
				//	In this scenario the player already disconnected and has to retry again the URL to start again.
				//
				//	To avoid this problem, we add here (ffmpeg client) a delay to wait ffmpeg server to starts
				//	Based on my statistics I think 2 seconds should be enought
				int sleepInSecondsToBeSureServerIsRunning = 2;
				this_thread::sleep_for(chrono::seconds(sleepInSecondsToBeSureServerIsRunning));
			}
		}

		/*
		_logger->info(__FILEREF__ + "Remove"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", currentInputIndex: " + to_string(currentInputIndex)
			+ ", currentNumberOfRepeatingSameInput: "
				+ to_string(currentNumberOfRepeatingSameInput)
			+ ", _outputFfmpegPathFileName: " + _outputFfmpegPathFileName);
		bool exceptionInCaseOfError = false;
		fs::remove_all(_outputFfmpegPathFileName, exceptionInCaseOfError);
		*/
		renameOutputFfmpegPathFileName(ingestionJobKey, encodingJobKey, _outputFfmpegPathFileName);
	}
}

// return the index of the selected input
int FFMpeg::getNextLiveProxyInput(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	json *inputsRoot,		// IN: list of inputs
	mutex *inputsRootMutex, // IN: mutex
	int currentInputIndex,	// IN: current index on the inputs
	bool timedInput, // IN: specify if the input is "timed". If "timed", next input is calculated based on the current time, otherwise it is retrieved
					 // simply the next
	json *newInputRoot // OUT: refer the input to be run
)
{
	lock_guard<mutex> locker(*inputsRootMutex);

	int newInputIndex = -1;
	*newInputRoot = nullptr;

	if (timedInput)
	{
		int64_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

		for (int inputIndex = 0; inputIndex < inputsRoot->size(); inputIndex++)
		{
			json inputRoot = (*inputsRoot)[inputIndex];

			string field = "utcScheduleStart";
			int64_t utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// if (utcProxyPeriodStart == -1)
			// {
			// 	field = "utcProxyPeriodStart";
			// 	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			field = "utcScheduleEnd";
			int64_t utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// if (utcProxyPeriodEnd == -1)
			// {
			// 	field = "utcProxyPeriodEnd";
			// 	utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
			// }

			_logger->info(
				__FILEREF__ + "getNextLiveProxyInput" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " +
				to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) + ", timedInput: " + to_string(timedInput) +
				", inputIndex: " + to_string(inputIndex) + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) +
				", utcNow: " + to_string(utcNow) + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
			);

			if (utcProxyPeriodStart <= utcNow && utcNow < utcProxyPeriodEnd)
			{
				*newInputRoot = (*inputsRoot)[inputIndex];
				newInputIndex = inputIndex;

				break;
			}
		}
	}
	else
	{
		newInputIndex = currentInputIndex + 1;

		if (newInputIndex < inputsRoot->size())
			*newInputRoot = (*inputsRoot)[newInputIndex];
		else
			newInputIndex = -1;
	}

	_logger->info(
		__FILEREF__ + "getNextLiveProxyInput" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", encodingJobKey: " + to_string(encodingJobKey) + ", currentInputIndex: " + to_string(currentInputIndex) +
		", timedInput: " + to_string(timedInput) + ", newInputIndex: " + to_string(newInputIndex)
	);

	return newInputIndex;
}

tuple<long, string, string, int, int64_t, json> FFMpeg::liveProxyInput(
	int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder, json inputRoot, long maxStreamingDurationInMinutes,
	vector<string> &ffmpegInputArgumentList
)
{
	long streamingDurationInSeconds = -1;
	string otherOutputOptionsBecauseOfMaxWidth;
	string endlessPlaylistListPathName;
	int pushListenTimeout = -1;
	int64_t utcProxyPeriodStart = -1;
	json inputFiltersRoot = nullptr;
	// 2023-03-26: vengono per ora commentate perchè
	// - sembra non siano utilizzate in questo momento
	// - getMediaInfo impiega qualche secondo per calcolarle e, in caso di LiveChannel, serve
	//		maggiore velocità in caso di switch da un input all'altro
	// vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
	// vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;

	// "inputRoot": {
	//	"timePeriod": false, "utcScheduleEnd": -1, "utcScheduleStart": -1
	//	...
	// }
	bool timePeriod = false;
	string field = "timePeriod";
	timePeriod = JSONUtils::asBool(inputRoot, field, false);

	// int64_t utcProxyPeriodStart = -1;
	field = "utcScheduleStart";
	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
	// else
	// {
	// 	field = "utcProxyPeriodStart";
	// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
	// 		utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, field, -1);
	// }

	int64_t utcProxyPeriodEnd = -1;
	field = "utcScheduleEnd";
	utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
	// else
	// {
	// 	field = "utcProxyPeriodEnd";
	// 	if (JSONUtils::isMetadataPresent(inputRoot, field))
	// 		utcProxyPeriodEnd = JSONUtils::asInt64(inputRoot, field, -1);
	// }

	//	"streamInput": { "captureAudioDeviceNumber": -1, "captureChannelsNumber": -1, "captureFrameRate": -1, "captureHeight": -1,
	//"captureVideoDeviceNumber": -1, "captureVideoInputFormat": "", "captureWidth": -1, "confKey": 2464, "configurationLabel":
	//"Italia-nazionali-Diretta canale satellitare della Camera dei deputati", "streamSourceType": "IP_PULL", "pushListenTimeout": -1,
	//"tvAudioItalianPid": -1, "tvFrequency": -1, "tvModulation": "", "tvServiceId": -1, "tvSymbolRate": -1, "tvVideoPid": -1, "url":
	//"https://www.youtube.com/watch?v=Cnjs83yowUM", "maxWidth": -1, "userAgent": "", "otherInputOptions": "" },
	if (JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
	{
		field = "streamInput";
		json streamInputRoot = inputRoot[field];

		field = "streamSourceType";
		if (!JSONUtils::isMetadataPresent(streamInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string streamSourceType = JSONUtils::asString(streamInputRoot, field, "");

		int maxWidth = -1;
		field = "maxWidth";
		maxWidth = JSONUtils::asInt(streamInputRoot, field, -1);

		string url;
		field = "url";
		url = JSONUtils::asString(streamInputRoot, field, "");

		field = "useVideoTrackFromPhysicalPathName";
		string useVideoTrackFromPhysicalPathName = JSONUtils::asString(streamInputRoot, field, "");

		field = "useVideoTrackFromPhysicalDeliveryURL";
		string useVideoTrackFromPhysicalDeliveryURL = JSONUtils::asString(streamInputRoot, field, "");

		string userAgent;
		field = "userAgent";
		userAgent = JSONUtils::asString(streamInputRoot, field, "");

		field = "pushListenTimeout";
		pushListenTimeout = JSONUtils::asInt(streamInputRoot, field, -1);

		string otherInputOptions;
		field = "otherInputOptions";
		otherInputOptions = JSONUtils::asString(streamInputRoot, field, "");

		string captureLive_videoInputFormat;
		field = "captureVideoInputFormat";
		captureLive_videoInputFormat = JSONUtils::asString(streamInputRoot, field, "");

		int captureLive_frameRate = -1;
		field = "captureFrameRate";
		captureLive_frameRate = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_width = -1;
		field = "captureWidth";
		captureLive_width = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_height = -1;
		field = "captureHeight";
		captureLive_height = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_videoDeviceNumber = -1;
		field = "captureVideoDeviceNumber";
		captureLive_videoDeviceNumber = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_channelsNumber = -1;
		field = "captureChannelsNumber";
		captureLive_channelsNumber = JSONUtils::asInt(streamInputRoot, field, -1);

		int captureLive_audioDeviceNumber = -1;
		field = "captureAudioDeviceNumber";
		captureLive_audioDeviceNumber = JSONUtils::asInt(streamInputRoot, field, -1);

		_logger->info(
			__FILEREF__ + "liveProxy: setting dynamic -map option" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " +
			to_string(encodingJobKey) + ", timePeriod: " + to_string(timePeriod) + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) +
			", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) + ", streamSourceType: " + streamSourceType
		);

		/* 2023-03-26: vedi commento sopra in questo metodo
		if (streamSourceType == "IP_PULL"
			|| streamSourceType == "IP_PUSH"
			|| streamSourceType == "TV")
		{
			try
			{
				int timeoutInSeconds = 20;
				getMediaInfo(ingestionJobKey, false, timeoutInSeconds, url, videoTracks, audioTracks);
			}
			catch(runtime_error& e)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: getMediaInfo failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				// throw e;
			}
		}
		*/

		if (streamSourceType == "IP_PULL" && maxWidth != -1)
		{
			try
			{
				vector<tuple<int, string, string, string, string, int, int>> liveVideoTracks;
				vector<tuple<int, string, string, string, int, bool>> liveAudioTracks;

				getLiveStreamingInfo(url, userAgent, ingestionJobKey, encodingJobKey, liveVideoTracks, liveAudioTracks);

				_logger->info(
					__FILEREF__ + "liveProxy: setting dynamic -map option" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", maxWidth: " + to_string(maxWidth)
				);

				int currentVideoWidth = -1;
				string selectedVideoStreamId;
				string selectedAudioStreamId;
				for (tuple<int, string, string, string, string, int, int> videoTrack : liveVideoTracks)
				{
					int videoProgramId;
					string videoStreamId;
					string videoStreamDescription;
					string videoCodec;
					string videoYUV;
					int videoWidth;
					int videoHeight;

					tie(videoProgramId, videoStreamId, videoStreamDescription, videoCodec, videoYUV, videoWidth, videoHeight) = videoTrack;

					if (videoStreamId != "" && videoWidth != -1 && videoWidth <= maxWidth &&
						(currentVideoWidth == -1 || videoWidth > currentVideoWidth))
					{
						// look an audio belonging to the same Program
						for (tuple<int, string, string, string, int, bool> audioTrack : liveAudioTracks)
						{
							int audioProgramId;
							string audioStreamId;
							string audioStreamDescription;
							string audioCodec;
							int audioSamplingRate;
							bool audioStereo;

							tie(audioProgramId, audioStreamId, audioStreamDescription, audioCodec, audioSamplingRate, audioStereo) = audioTrack;

							if (audioStreamDescription.find("eng") != string::npos || audioStreamDescription.find("des") != string::npos)
							{
								_logger->info(
									__FILEREF__ + "liveProxy: audio track discarded" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", encodingJobKey: " + to_string(encodingJobKey) + ", audioStreamId: " + audioStreamId +
									", audioStreamDescription: " + audioStreamDescription
								);

								continue;
							}

							if (videoProgramId == audioProgramId && audioStreamId != "")
							{
								selectedVideoStreamId = videoStreamId;
								selectedAudioStreamId = audioStreamId;

								currentVideoWidth = videoWidth;

								break;
							}
						}
					}
				}

				if (selectedVideoStreamId != "" && selectedAudioStreamId != "")
				{
					otherOutputOptionsBecauseOfMaxWidth = string(" -map ") + selectedVideoStreamId + " -map " + selectedAudioStreamId;
				}

				_logger->info(
					__FILEREF__ + "liveProxy: new other output options" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", maxWidth: " + to_string(maxWidth) +
					", otherOutputOptionsBecauseOfMaxWidth: " + otherOutputOptionsBecauseOfMaxWidth
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: getLiveStreamingInfo or associate processing failed" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", e.what(): " + e.what();
				_logger->error(errorMessage);

				// throw e;
			}
		}

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		{
			if (timePeriod)
			{
				// 2024-05-23: abbiamo visto che, per alcuni canali, il processo ffmpeg dopo tanto tempo che è running, non prox-a piu tanto bene.
				// 	Per questo motivo abbiamo introdotto maxStreamingDurationInMinutes che rappresenta il max che il processo ffmpeg posso essere
				// running. 	Il processo ffmpeg ripartirà l'istante dopo in base a utcProxyPeriodEnd
				if (maxStreamingDurationInMinutes == -1)
					streamingDurationInSeconds = utcProxyPeriodEnd - utcNow;
				else
					streamingDurationInSeconds = utcProxyPeriodEnd - utcNow > maxStreamingDurationInMinutes * 60 ? maxStreamingDurationInMinutes * 60
																												 : utcProxyPeriodEnd - utcNow;

				_logger->info(
					__FILEREF__ + "LiveProxy timing. " + "Streaming duration" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", utcNow: " + to_string(utcNow) +
					", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) +
					", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
				);

				if (streamSourceType == "IP_PUSH" || streamSourceType == "TV")
				{
					if (pushListenTimeout > 0 && pushListenTimeout > streamingDurationInSeconds)
					{
						// 2021-02-02: sceanrio:
						//	streaming duration is 25 seconds
						//	timeout: 3600 seconds
						//	The result is that the process will finish after 3600 seconds, not after 25 seconds
						//	To avoid that, in this scenario, we will set the timeout equals to streamingDurationInSeconds
						_logger->info(
							__FILEREF__ + "LiveProxy timing. " +
							"Listen timeout in seconds is reduced because max after 'streamingDurationInSeconds' the process has to finish" +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
							", utcNow: " + to_string(utcNow) + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) + ", utcProxyPeriodEnd: " +
							to_string(utcProxyPeriodEnd) + ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds) +
							", pushListenTimeout: " + to_string(pushListenTimeout)
						);

						pushListenTimeout = streamingDurationInSeconds;
					}
				}
			}

			// user agent is an HTTP header and can be used only in case of http request
			bool userAgentToBeUsed = false;
			if (streamSourceType == "IP_PULL" && userAgent != "")
			{
				string httpPrefix = "http"; // it includes also https
				if (url.size() >= httpPrefix.size() && url.compare(0, httpPrefix.size(), httpPrefix) == 0)
				{
					userAgentToBeUsed = true;
				}
				else
				{
					_logger->warn(
						__FILEREF__ + "user agent cannot be used if not http" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", encodingJobKey: " + to_string(encodingJobKey) + ", url: " + url
					);
				}
			}

			// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

			// -re (input) Read input at native frame rate. By default ffmpeg attempts to read the input(s)
			//		as fast as possible. This option will slow down the reading of the input(s)
			//		to the native frame rate of the input(s). It is useful for real-time output
			//		(e.g. live streaming).
			// -hls_flags append_list: Append new segments into the end of old segment list
			//		and remove the #EXT-X-ENDLIST from the old segment list
			// -hls_time seconds: Set the target segment length in seconds. Segment will be cut on the next key frame
			//		after this time has passed.
			// -hls_list_size size: Set the maximum number of playlist entries. If set to 0 the list file
			//		will contain all the segments. Default value is 5.
			//	-nostdin: Disabling interaction on standard input, it is useful, for example, if ffmpeg is
			//		in the background process group
			ffmpegInputArgumentList.push_back("-nostdin");
			if (userAgentToBeUsed)
			{
				ffmpegInputArgumentList.push_back("-user_agent");
				ffmpegInputArgumentList.push_back(userAgent);
			}
			ffmpegInputArgumentList.push_back("-re");
			FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegInputArgumentList);
			if (streamSourceType == "IP_PUSH")
			{
				// listen/timeout depend on the protocol (https://ffmpeg.org/ffmpeg-protocols.html)
				if (url.find("http://") != string::npos || url.find("rtmp://") != string::npos)
				{
					ffmpegInputArgumentList.push_back("-listen");
					ffmpegInputArgumentList.push_back("1");
					if (pushListenTimeout > 0)
					{
						// no timeout means it will listen infinitely
						ffmpegInputArgumentList.push_back("-timeout");
						ffmpegInputArgumentList.push_back(to_string(pushListenTimeout));
					}
				}
				else if (url.find("udp://") != string::npos)
				{
					if (pushListenTimeout > 0)
					{
						// About the timeout url parameter, ffmpeg docs says: This option is only relevant
						//	in read mode: if no data arrived in more than this time interval, raise error
						// This parameter accepts microseconds and we cannot provide a huge number
						// i.e. 1h in microseconds because it will not work (it will be the max number
						// of a 'long' type).
						// For this reason we have to set max 30 minutes
						//
						// Remark: this is just a read timeout, then we have below the -t parameter
						//	that will stop the ffmpeg command after a specified time.
						//  So, in case for example we have to run this command for 1h, we will have
						//  ?timeout=1800000000 (30 mins) and -t 3600
						//  ONLY in case it is not received any data for 30 mins, this command will exit
						//  after 30 mins (because of the ?timeout parameter) and the system will run
						// again the command again for the remaining 30 minutes:
						//  ?timeout=1800000000 (30 mins) and -t 180

						int maxPushTimeout = 180; // 30 mins
						int64_t listenTimeoutInMicroSeconds;
						if (pushListenTimeout > maxPushTimeout)
							listenTimeoutInMicroSeconds = maxPushTimeout;
						else
							listenTimeoutInMicroSeconds = pushListenTimeout;
						listenTimeoutInMicroSeconds *= 1000000;

						if (url.find("?") == string::npos)
							url += ("?timeout=" + to_string(listenTimeoutInMicroSeconds));
						else
							url += ("&timeout=" + to_string(listenTimeoutInMicroSeconds));
					}

					// In case of udp:
					// overrun_nonfatal=1 prevents ffmpeg from exiting,
					//		it can recover in most circumstances.
					// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
					if (url.find("?") == string::npos)
						url += "?overrun_nonfatal=1&fifo_size=50000000";
					else
						url += "&overrun_nonfatal=1&fifo_size=50000000";
				}
				else if (url.find("srt://") != string::npos)
				{
				}
				else
				{
					_logger->error(
						__FILEREF__ + "listen/timeout not managed yet for the current protocol" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", encodingJobKey: " + to_string(encodingJobKey) + ", url: " + url
					);
				}

				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);
			}
			else if (streamSourceType == "IP_PULL")
			{
				// this is a streaming
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);

				if (useVideoTrackFromPhysicalPathName != "" && !externalEncoder)
				{
					// this is a file

					ffmpegInputArgumentList.push_back("-stream_loop");
					// number of times input stream shall be looped. Loop 0 means no loop, loop -1 means infinite loop
					ffmpegInputArgumentList.push_back("-1");

					ffmpegInputArgumentList.push_back("-i");
					ffmpegInputArgumentList.push_back(useVideoTrackFromPhysicalPathName);

					ffmpegInputArgumentList.push_back("-map");
					ffmpegInputArgumentList.push_back("0:a");
					ffmpegInputArgumentList.push_back("-map");
					ffmpegInputArgumentList.push_back("1:v");
				}
				else if (useVideoTrackFromPhysicalDeliveryURL != "" && externalEncoder)
				{
					// this is a file

					ffmpegInputArgumentList.push_back("-stream_loop");
					// number of times input stream shall be looped. Loop 0 means no loop, loop -1 means infinite loop
					ffmpegInputArgumentList.push_back("-1");

					ffmpegInputArgumentList.push_back("-i");
					ffmpegInputArgumentList.push_back(useVideoTrackFromPhysicalDeliveryURL);

					ffmpegInputArgumentList.push_back("-map");
					ffmpegInputArgumentList.push_back("0:a");
					ffmpegInputArgumentList.push_back("-map");
					ffmpegInputArgumentList.push_back("1:v");
				}
			}
			else if (streamSourceType == "TV")
			{
				if (url.find("udp://") != string::npos)
				{
					// In case of udp:
					// overrun_nonfatal=1 prevents ffmpeg from exiting,
					//		it can recover in most circumstances.
					// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
					if (url.find("?") == string::npos)
						url += "?overrun_nonfatal=1&fifo_size=50000000";
					else
						url += "&overrun_nonfatal=1&fifo_size=50000000";
				}

				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);
			}
			else if (streamSourceType == "CaptureLive")
			{
				// video
				{
					// -f v4l2 -framerate 25 -video_size 640x480 -i /dev/video0
					ffmpegInputArgumentList.push_back("-f");
					ffmpegInputArgumentList.push_back("v4l2");

					ffmpegInputArgumentList.push_back("-thread_queue_size");
					ffmpegInputArgumentList.push_back("4096");

					if (captureLive_videoInputFormat != "")
					{
						ffmpegInputArgumentList.push_back("-input_format");
						ffmpegInputArgumentList.push_back(captureLive_videoInputFormat);
					}

					if (captureLive_frameRate != -1)
					{
						ffmpegInputArgumentList.push_back("-framerate");
						ffmpegInputArgumentList.push_back(to_string(captureLive_frameRate));
					}

					if (captureLive_width != -1 && captureLive_height != -1)
					{
						ffmpegInputArgumentList.push_back("-video_size");
						ffmpegInputArgumentList.push_back(to_string(captureLive_width) + "x" + to_string(captureLive_height));
					}

					ffmpegInputArgumentList.push_back("-i");
					ffmpegInputArgumentList.push_back(string("/dev/video") + to_string(captureLive_videoDeviceNumber));
				}

				// audio
				{
					ffmpegInputArgumentList.push_back("-f");
					ffmpegInputArgumentList.push_back("alsa");

					ffmpegInputArgumentList.push_back("-thread_queue_size");
					ffmpegInputArgumentList.push_back("2048");

					if (captureLive_channelsNumber != -1)
					{
						ffmpegInputArgumentList.push_back("-ac");
						ffmpegInputArgumentList.push_back(to_string(captureLive_channelsNumber));
					}

					ffmpegInputArgumentList.push_back("-i");
					ffmpegInputArgumentList.push_back(string("hw:") + to_string(captureLive_audioDeviceNumber));
				}
			}

			// se viene usato l'imageoverlay filter, bisogna aggiungere il riferimento alla image
			{
				if (JSONUtils::isMetadataPresent(streamInputRoot, "filters"))
				{
					json filtersRoot = streamInputRoot["filters"];

					// se viene usato il filtro imageoverlay, è necessario recuperare sourcePhysicalPathName e sourcePhysicalDeliveryURL
					if (JSONUtils::isMetadataPresent(filtersRoot, "complex"))
					{
						json complexFiltersRoot = filtersRoot["complex"];
						for (int complexFilterIndex = 0; complexFilterIndex < complexFiltersRoot.size(); complexFilterIndex++)
						{
							json complexFilterRoot = complexFiltersRoot[complexFilterIndex];
							if (JSONUtils::isMetadataPresent(complexFilterRoot, "type") && complexFilterRoot["type"] == "imageoverlay")
							{
								if (externalEncoder)
								{
									if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalDeliveryURL"))
									{
										string errorMessage = fmt::format(
											"imageoverlay filter without imagePhysicalDeliveryURL"
											", ingestionJobKey: {}"
											", imageoverlay filter: {}",
											ingestionJobKey, JSONUtils::toString(complexFilterRoot)
										);
										SPDLOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
									ffmpegInputArgumentList.push_back("-i");
									ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalDeliveryURL"]);
								}
								else
								{
									if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalPathName"))
									{
										string errorMessage = fmt::format(
											"imageoverlay filter without imagePhysicalDeliveryURL"
											", ingestionJobKey: {}"
											", imageoverlay filter: {}",
											ingestionJobKey, JSONUtils::toString(complexFilterRoot)
										);
										SPDLOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
									ffmpegInputArgumentList.push_back("-i");
									ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalPathName"]);
								}
							}
						}
					}
				}
			}

			if (timePeriod)
			{
				ffmpegInputArgumentList.push_back("-t");
				ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));
			}
		}

		if (JSONUtils::isMetadataPresent(streamInputRoot, "filters"))
			inputFiltersRoot = streamInputRoot["filters"];
	}
	//	"directURLInput": { "url": "" },
	else if (JSONUtils::isMetadataPresent(inputRoot, "directURLInput"))
	{
		field = "directURLInput";
		json directURLInputRoot = inputRoot[field];

		string url;
		field = "url";
		url = JSONUtils::asString(directURLInputRoot, field, "");

		_logger->info(
			__FILEREF__ + "liveProxy, url" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
			", timePeriod: " + to_string(timePeriod) + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) +
			", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) + ", url: " + url
		);

		/* 2023-03-26: vedi commento sopra in questo metodo
		{
			try
			{
				int timeoutInSeconds = 20;
				getMediaInfo(ingestionJobKey, false, timeoutInSeconds, url, videoTracks, audioTracks);
			}
			catch(runtime_error& e)
			{
				string errorMessage = __FILEREF__ + "ffmpeg: getMediaInfo failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
				;
				_logger->error(errorMessage);

				// throw e;
			}
		}
		*/

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		{
			if (timePeriod)
			{
				streamingDurationInSeconds = utcProxyPeriodEnd - utcNow;

				_logger->info(
					__FILEREF__ + "LiveProxy timing. " + "Streaming duration" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", utcNow: " + to_string(utcNow) +
					", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) +
					", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
				);
			}

			// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

			// -re (input) Read input at native frame rate. By default ffmpeg attempts to read the input(s)
			//		as fast as possible. This option will slow down the reading of the input(s)
			//		to the native frame rate of the input(s). It is useful for real-time output
			//		(e.g. live streaming).
			// -hls_flags append_list: Append new segments into the end of old segment list
			//		and remove the #EXT-X-ENDLIST from the old segment list
			// -hls_time seconds: Set the target segment length in seconds. Segment will be cut on the next key frame
			//		after this time has passed.
			// -hls_list_size size: Set the maximum number of playlist entries. If set to 0 the list file
			//		will contain all the segments. Default value is 5.
			//	-nostdin: Disabling interaction on standard input, it is useful, for example, if ffmpeg is
			//		in the background process group
			ffmpegInputArgumentList.push_back("-nostdin");
			ffmpegInputArgumentList.push_back("-re");
			{
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(url);
			}

			// se viene usato l'imageoverlay filter, bisogna aggiungere il riferimento alla image
			{
				if (JSONUtils::isMetadataPresent(directURLInputRoot, "filters"))
				{
					json filtersRoot = directURLInputRoot["filters"];

					// se viene usato il filtro imageoverlay, è necessario recuperare sourcePhysicalPathName e sourcePhysicalDeliveryURL
					if (JSONUtils::isMetadataPresent(filtersRoot, "complex"))
					{
						json complexFiltersRoot = filtersRoot["complex"];
						for (int complexFilterIndex = 0; complexFilterIndex < complexFiltersRoot.size(); complexFilterIndex++)
						{
							json complexFilterRoot = complexFiltersRoot[complexFilterIndex];
							if (JSONUtils::isMetadataPresent(complexFilterRoot, "type") && complexFilterRoot["type"] == "imageoverlay")
							{
								if (externalEncoder)
								{
									if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalDeliveryURL"))
									{
										string errorMessage = fmt::format(
											"imageoverlay filter without imagePhysicalDeliveryURL"
											", ingestionJobKey: {}"
											", imageoverlay filter: {}",
											ingestionJobKey, JSONUtils::toString(complexFilterRoot)
										);
										SPDLOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
									ffmpegInputArgumentList.push_back("-i");
									ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalDeliveryURL"]);
								}
								else
								{
									if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalPathName"))
									{
										string errorMessage = fmt::format(
											"imageoverlay filter without imagePhysicalDeliveryURL"
											", ingestionJobKey: {}"
											", imageoverlay filter: {}",
											ingestionJobKey, JSONUtils::toString(complexFilterRoot)
										);
										SPDLOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
									ffmpegInputArgumentList.push_back("-i");
									ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalPathName"]);
								}
							}
						}
					}
				}
			}

			if (timePeriod)
			{
				ffmpegInputArgumentList.push_back("-t");
				ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));
			}
		}

		if (JSONUtils::isMetadataPresent(directURLInputRoot, "filters"))
			inputFiltersRoot = directURLInputRoot["filters"];
	}
	//	"vodInput": { "vodContentType": "", "sources": [{"sourcePhysicalPathName": "..."}],
	//		"otherInputOptions": "" },
	else if (JSONUtils::isMetadataPresent(inputRoot, "vodInput"))
	{
		string field = "vodInput";
		json vodInputRoot = inputRoot[field];

		field = "vodContentType";
		if (!JSONUtils::isMetadataPresent(vodInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string vodContentType = JSONUtils::asString(vodInputRoot, field, "Video");

		vector<string> sources;
		// int64_t durationOfInputsInMilliSeconds = 0;
		{
			field = "sources";
			if (!JSONUtils::isMetadataPresent(vodInputRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			json sourcesRoot = vodInputRoot[field];

			for (int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
			{
				json sourceRoot = sourcesRoot[sourceIndex];

				if (externalEncoder)
					field = "sourcePhysicalDeliveryURL";
				else
					field = "sourcePhysicalPathName";
				if (!JSONUtils::isMetadataPresent(sourceRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", encodingJobKey: " + to_string(encodingJobKey) + ", Field: " + field +
										  ", externalEncoder: " + to_string(externalEncoder) + ", sourceRoot: " + JSONUtils::toString(sourceRoot);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sourcePhysicalReference = JSONUtils::asString(sourceRoot, field, "");
				sources.push_back(sourcePhysicalReference);

				// field = "durationInMilliSeconds";
				// if (JSONUtils::isMetadataPresent(sourceRoot, field))
				// 	durationOfInputsInMilliSeconds += JSONUtils::asInt64(sourceRoot, field, 0);
			}
		}

		string otherInputOptions;
		field = "otherInputOptions";
		otherInputOptions = JSONUtils::asString(vodInputRoot, field, "");

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		{
			if (timePeriod)
			{
				streamingDurationInSeconds = utcProxyPeriodEnd - utcNow;

				_logger->info(
					__FILEREF__ + "VODProxy timing. " + "Streaming duration" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", utcNow: " + to_string(utcNow) +
					", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) +
					", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds)
				);
			}

			// ffmpeg <global-options> <input-options> -i <input> <output-options> <output>

			// -re (input) Read input at native frame rate. By default ffmpeg attempts to read the input(s)
			//		as fast as possible. This option will slow down the reading of the input(s)
			//		to the native frame rate of the input(s). It is useful for real-time output
			//		(e.g. live streaming).
			// -hls_flags append_list: Append new segments into the end of old segment list
			//		and remove the #EXT-X-ENDLIST from the old segment list
			// -hls_time seconds: Set the target segment length in seconds. Segment will be cut on the next key frame
			//		after this time has passed.
			// -hls_list_size size: Set the maximum number of playlist entries. If set to 0 the list file
			//		will contain all the segments. Default value is 5.
			//	-nostdin: Disabling interaction on standard input, it is useful, for example, if ffmpeg is
			//		in the background process group
			ffmpegInputArgumentList.push_back("-nostdin");

			ffmpegInputArgumentList.push_back("-re");
			FFMpegEncodingParameters::addToArguments(otherInputOptions, ffmpegInputArgumentList);

			if (vodContentType == "Image")
			{
				ffmpegInputArgumentList.push_back("-r");
				ffmpegInputArgumentList.push_back("25");

				ffmpegInputArgumentList.push_back("-loop");
				ffmpegInputArgumentList.push_back("1");
			}
			else
			{
				/*
					2022-10-27: -stream_loop works only in case of ONE input.
						In case of multiple VODs we will use the '-f concat' option implementing
						an endless recursive playlist
						see https://video.stackexchange.com/questions/18982/is-it-possible-to-create-an-endless-loop-using-concat
				*/
				if (sources.size() == 1)
				{
					ffmpegInputArgumentList.push_back("-stream_loop");
					ffmpegInputArgumentList.push_back("-1");
				}
			}

			if (sources.size() == 1)
			{
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(sources[0]);
			}
			else // if (sources.size() > 1)
			{
				// ffmpeg concat demuxer supports nested scripts with the header "ffconcat version 1.0".
				// Build the endless recursive playlist file like (i.e:
				//	ffconcat version 1.0
				//	file 'storage/MMSRepository/MMS_0003/1/000/004/016/2030954_97080_24.mp4'
				//	file 'storage/MMSRepository/MMS_0003/1/000/004/235/2143253_99028_24.mp4'
				//	...
				//	file 'XXX_YYY_endlessPlaylist.txt'

				string endlessPlaylistListFileName = fmt::format("{}_{}_endlessPlaylist.txt", ingestionJobKey, encodingJobKey);
				endlessPlaylistListPathName = _ffmpegEndlessRecursivePlaylistDir + "/" + endlessPlaylistListFileName;
				;

				SPDLOG_INFO(
					"Creating ffmpegEndlessRecursivePlaylist file"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", _ffmpegEndlessRecursivePlaylist: {}",
					ingestionJobKey, encodingJobKey, endlessPlaylistListPathName
				);
				ofstream playlistListFile(endlessPlaylistListPathName.c_str(), ofstream::trunc);
				if (!playlistListFile)
				{
					string errorMessage = fmt::format(
						"Error creating ffmpegEndlessRecursivePlaylist file"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", _ffmpegEndlessRecursivePlaylist: {}"
						", errno: {} ({})",
						ingestionJobKey, encodingJobKey, endlessPlaylistListPathName, errno, strerror(errno)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				playlistListFile << "ffconcat version 1.0" << endl;
				for (string sourcePhysicalReference : sources)
				{
					// sourcePhysicalReference will be:
					//	a URL in case of externalEncoder
					//	a storage path name in case of a local encoder

					if (externalEncoder)
					{
						bool isStreaming = false;

						string destBinaryPathName;
						string destBinaryFileName;
						{
							size_t fileNameIndex = sourcePhysicalReference.find_last_of("/");
							if (fileNameIndex == string::npos)
							{
								SPDLOG_ERROR(
									"physical path has a wrong path"
									", ingestionJobKey: {}"
									", sourcePhysicalReference: {}",
									ingestionJobKey, sourcePhysicalReference
								);

								continue;
							}
							// 2023-06-10: nel destBinaryFileName è necessario aggiungere
							//	ingestionJobKey and encodingJobKey come per il nome della playlist.
							//	Infatti, nel caso in cui avessimo due IngestionJob (due VOD Proxy)
							//	che usano lo stesso source file, entrambi farebbero il download dello stesso file,
							//	con lo stesso nome, nella stessa directory (_ffmpegEndlessRecursivePlaylistDir).
							//	Per questo motivo aggiungiamo, come prefisso al source file name,
							//	ingestionJobKey and encodingJobKey
							destBinaryFileName =
								fmt::format("{}_{}_{}", ingestionJobKey, encodingJobKey, sourcePhysicalReference.substr(fileNameIndex + 1));

							size_t extensionIndex = destBinaryFileName.find_last_of(".");
							if (extensionIndex != string::npos)
							{
								if (destBinaryFileName.substr(extensionIndex + 1) == "m3u8")
								{
									isStreaming = true;
									destBinaryFileName = destBinaryFileName.substr(0, extensionIndex) + ".mp4";
								}
							}

							destBinaryPathName = _ffmpegEndlessRecursivePlaylistDir + "/" + destBinaryFileName;
						}

						// sourcePhysicalReference is like
						// https://mms-delivery-path.catramms-cloud.com/token_mDEs0rZTXRyMkOCngnG87w==,1666987919/MMS_0000/1/000/229/507/1429406_231284_changeFileFormat.mp4
						if (isStreaming)
						{
							// regenerateTimestamps: see docs/TASK_01_Add_Content_JSON_Format.txt
							bool regenerateTimestamps = false;

							streamingToFile(ingestionJobKey, regenerateTimestamps, sourcePhysicalReference, destBinaryPathName);
						}
						else
						{
							chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
							double lastPercentageUpdated = -1.0;
							curlpp::types::ProgressFunctionFunctor functor = bind(
								&FFMpeg::progressDownloadCallback, this, ingestionJobKey, lastProgressUpdate, lastPercentageUpdated, placeholders::_1,
								placeholders::_2, placeholders::_3, placeholders::_4
							);
							MMSCURL::downloadFile(_logger, ingestionJobKey, sourcePhysicalReference, destBinaryPathName, functor);
						}
						// playlist and dowloaded files will be removed by the calling FFMpeg::liveProxy2 method
						playlistListFile << "file '" << destBinaryFileName << "'" << endl;

						SPDLOG_INFO(
							"ffmpeg: adding physical path"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _ffmpegEndlessRecursivePlaylist: {}"
							", sourcePhysicalReference: {}"
							", content: file '{}'",
							ingestionJobKey, encodingJobKey, endlessPlaylistListPathName, sourcePhysicalReference, destBinaryFileName
						);
					}
					else
					{
						size_t storageIndex = sourcePhysicalReference.find("/storage/");
						if (storageIndex == string::npos)
						{
							SPDLOG_ERROR(
								"physical path has a wrong path"
								", ingestionJobKey: {}"
								", sourcePhysicalReference: {}",
								ingestionJobKey, sourcePhysicalReference
							);

							continue;
						}

						playlistListFile << "file '" << sourcePhysicalReference.substr(storageIndex + 1) << "'" << endl;

						SPDLOG_INFO(
							"ffmpeg: adding physical path"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", _ffmpegEndlessRecursivePlaylist: {}"
							", sourcePhysicalReference: {}"
							", content: file '{}'",
							ingestionJobKey, encodingJobKey, endlessPlaylistListPathName, sourcePhysicalReference,
							sourcePhysicalReference.substr(storageIndex + 1)
						);
					}
				}
				playlistListFile << "file '" << endlessPlaylistListFileName << "'" << endl;
				playlistListFile.close();

				ffmpegInputArgumentList.push_back("-f");
				ffmpegInputArgumentList.push_back("concat");
				ffmpegInputArgumentList.push_back("-i");
				ffmpegInputArgumentList.push_back(endlessPlaylistListPathName);
			}

			// se viene usato l'imageoverlay filter, bisogna aggiungere il riferimento alla image
			{
				if (JSONUtils::isMetadataPresent(vodInputRoot, "filters"))
				{
					json filtersRoot = vodInputRoot["filters"];

					// se viene usato il filtro imageoverlay, è necessario recuperare sourcePhysicalPathName e sourcePhysicalDeliveryURL
					if (JSONUtils::isMetadataPresent(filtersRoot, "complex"))
					{
						json complexFiltersRoot = filtersRoot["complex"];
						for (int complexFilterIndex = 0; complexFilterIndex < complexFiltersRoot.size(); complexFilterIndex++)
						{
							json complexFilterRoot = complexFiltersRoot[complexFilterIndex];
							if (JSONUtils::isMetadataPresent(complexFilterRoot, "type") && complexFilterRoot["type"] == "imageoverlay")
							{
								if (externalEncoder)
								{
									if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalDeliveryURL"))
									{
										string errorMessage = fmt::format(
											"imageoverlay filter without imagePhysicalDeliveryURL"
											", ingestionJobKey: {}"
											", imageoverlay filter: {}",
											ingestionJobKey, JSONUtils::toString(complexFilterRoot)
										);
										SPDLOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
									ffmpegInputArgumentList.push_back("-i");
									ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalDeliveryURL"]);
								}
								else
								{
									if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalPathName"))
									{
										string errorMessage = fmt::format(
											"imageoverlay filter without imagePhysicalDeliveryURL"
											", ingestionJobKey: {}"
											", imageoverlay filter: {}",
											ingestionJobKey, JSONUtils::toString(complexFilterRoot)
										);
										SPDLOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
									ffmpegInputArgumentList.push_back("-i");
									ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalPathName"]);
								}
							}
						}
					}
				}
			}

			if (timePeriod)
			{
				ffmpegInputArgumentList.push_back("-t");
				ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));
			}
		}

		if (JSONUtils::isMetadataPresent(vodInputRoot, "filters"))
			inputFiltersRoot = vodInputRoot["filters"];
	}
	//	"countdownInput": { "mmsSourceVideoAssetPathName": "", "videoDurationInMilliSeconds": 123, "text": "", "textPosition_X_InPixel": "",
	//"textPosition_Y_InPixel": "", "fontType": "", "fontSize": 22, "fontColor": "", "textPercentageOpacity": -1, "boxEnable": false, "boxColor":
	//"", "boxPercentageOpacity": 20 },
	else if (JSONUtils::isMetadataPresent(inputRoot, "countdownInput"))
	{
		string field = "countdownInput";
		json countdownInputRoot = inputRoot[field];

		if (externalEncoder)
			field = "mmsSourceVideoAssetDeliveryURL";
		else
			field = "mmsSourceVideoAssetPathName";
		if (!JSONUtils::isMetadataPresent(countdownInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string mmsSourceVideoAssetPathName = JSONUtils::asString(countdownInputRoot, field, "");

		field = "videoDurationInMilliSeconds";
		if (!JSONUtils::isMetadataPresent(countdownInputRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(countdownInputRoot, field, -1);

		if (!externalEncoder && !fs::exists(mmsSourceVideoAssetPathName))
		{
			string errorMessage = string("Source video asset path name not existing") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", mmsSourceVideoAssetPathName: " + mmsSourceVideoAssetPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t utcCountDownEnd = utcProxyPeriodEnd;

		time_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		if (utcCountDownEnd <= utcNow)
		{
			time_t tooLateTime = utcNow - utcCountDownEnd;

			string errorMessage = __FILEREF__ + "Countdown timing. " + "Too late to start" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", utcNow: " + to_string(utcNow) +
								  ", utcCountDownEnd: " + to_string(utcCountDownEnd) + ", tooLateTime: " + to_string(tooLateTime);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		int streamLoopNumber;
		{
			streamingDurationInSeconds = utcCountDownEnd - utcNow;

			_logger->info(
				__FILEREF__ + "Countdown timing. " + "Streaming duration" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", utcNow: " + to_string(utcNow) +
				", utcCountDownEnd: " + to_string(utcCountDownEnd) + ", streamingDurationInSeconds: " + to_string(streamingDurationInSeconds) +
				", videoDurationInMilliSeconds: " + to_string(videoDurationInMilliSeconds)
			);

			float fVideoDurationInMilliSeconds = videoDurationInMilliSeconds;
			fVideoDurationInMilliSeconds /= 1000;

			streamLoopNumber = streamingDurationInSeconds / fVideoDurationInMilliSeconds;
			streamLoopNumber += 2;
		}

		{
			// global options
			// input options
			ffmpegInputArgumentList.push_back("-re");
			ffmpegInputArgumentList.push_back("-stream_loop");
			ffmpegInputArgumentList.push_back(to_string(streamLoopNumber));
			ffmpegInputArgumentList.push_back("-i");
			ffmpegInputArgumentList.push_back(mmsSourceVideoAssetPathName);
		}

		// se viene usato l'imageoverlay filter, bisogna aggiungere il riferimento alla image
		{
			if (JSONUtils::isMetadataPresent(countdownInputRoot, "filters"))
			{
				json filtersRoot = countdownInputRoot["filters"];

				// se viene usato il filtro imageoverlay, è necessario recuperare sourcePhysicalPathName e sourcePhysicalDeliveryURL
				if (JSONUtils::isMetadataPresent(filtersRoot, "complex"))
				{
					json complexFiltersRoot = filtersRoot["complex"];
					for (int complexFilterIndex = 0; complexFilterIndex < complexFiltersRoot.size(); complexFilterIndex++)
					{
						json complexFilterRoot = complexFiltersRoot[complexFilterIndex];
						if (JSONUtils::isMetadataPresent(complexFilterRoot, "type") && complexFilterRoot["type"] == "imageoverlay")
						{
							if (externalEncoder)
							{
								if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalDeliveryURL"))
								{
									string errorMessage = fmt::format(
										"imageoverlay filter without imagePhysicalDeliveryURL"
										", ingestionJobKey: {}"
										", imageoverlay filter: {}",
										ingestionJobKey, JSONUtils::toString(complexFilterRoot)
									);
									SPDLOG_ERROR(errorMessage);

									throw runtime_error(errorMessage);
								}
								ffmpegInputArgumentList.push_back("-i");
								ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalDeliveryURL"]);
							}
							else
							{
								if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalPathName"))
								{
									string errorMessage = fmt::format(
										"imageoverlay filter without imagePhysicalDeliveryURL"
										", ingestionJobKey: {}"
										", imageoverlay filter: {}",
										ingestionJobKey, JSONUtils::toString(complexFilterRoot)
									);
									SPDLOG_ERROR(errorMessage);

									throw runtime_error(errorMessage);
								}
								ffmpegInputArgumentList.push_back("-i");
								ffmpegInputArgumentList.push_back(complexFilterRoot["imagePhysicalPathName"]);
							}
						}
					}
				}
			}
		}

		{
			ffmpegInputArgumentList.push_back("-t");
			ffmpegInputArgumentList.push_back(to_string(streamingDurationInSeconds));
		}

		// inizializza filtersRoot e verifica se drawtext is present
		bool isDrawTextFilterPresent = false;
		if (JSONUtils::isMetadataPresent(countdownInputRoot, "filters"))
		{
			inputFiltersRoot = countdownInputRoot["filters"];
			if (JSONUtils::isMetadataPresent(inputFiltersRoot, "video"))
			{
				json videoFiltersRoot = inputFiltersRoot["video"];
				for (int videoFilterIndex = 0; videoFilterIndex < videoFiltersRoot.size(); videoFilterIndex++)
				{
					json videoFilterRoot = videoFiltersRoot[videoFilterIndex];
					if (JSONUtils::isMetadataPresent(videoFilterRoot, "type") && videoFilterRoot["type"] == "drawtext")
						isDrawTextFilterPresent = true;
				}
			}
		}
		if (!isDrawTextFilterPresent)
		{
			string errorMessage = __FILEREF__ + "Countdown has to have the drawText filter" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) +
								  ", countdownInputRoot: " + JSONUtils::toString(countdownInputRoot);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else
	{
		string errorMessage = __FILEREF__ + "streamInput or vodInput or countdownInput is not present" +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey);
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	return make_tuple(
		streamingDurationInSeconds, otherOutputOptionsBecauseOfMaxWidth, endlessPlaylistListPathName, pushListenTimeout, utcProxyPeriodStart,
		inputFiltersRoot
	); // , videoTracks, audioTracks);
}

// il metodo outputsRootToFfmpeg_clean pulisce eventuali directory/files creati da outputsRootToFfmpeg
void FFMpeg::outputsRootToFfmpeg(
	int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder, string otherOutputOptionsBecauseOfMaxWidth, json inputFiltersRoot,
	long streamingDurationInSeconds, json outputsRoot,

	/*
	// vengono usati i due vector seguenti nel caso abbiamo una lista di maps (video and audio)
	// a cui applicare i parametri di encoding
	// Esempio nel caso del liveGrid abbiamo qualcosa tipo:
	// -map "[0r+1r]" -codec:v libx264 -b:v 800k -preset veryfast -hls_time 10 -hls_list_size 4....
	// -map 0:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 ...
	// -map 1:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 ...
	// -map 2:a -acodec aac -b:a 92k -ac 2 -hls_time 10 -hls_list_size 4 ...
	// ...
	vector<string> videoMaps,
	vector<string> audioMaps,
	*/

	vector<string> &ffmpegOutputArgumentList
)
{

	SPDLOG_INFO(
		"Received outputsRootToFfmpeg"
		", ingestionJobKey: {}"
		", encodingJobKey: {}"
		", outputsRoot: {}",
		ingestionJobKey, encodingJobKey, JSONUtils::toString(outputsRoot)
	);

	FFMpegFilters ffmpegFilters(_ffmpegTtfFontDir);

	// 2023-01-01:
	//		In genere i parametri del 'draw text' vengono inizializzati all'interno di outputRoot.
	//		Nel caso di un Broadcast (Live Channel), all'interno del json dell'encodingJob, inputsRoot
	//		rappresenta l'intera playlist del live channel
	//		(dalle ore A alle ora B contenuto 1, dalle ora C alle ore D contentuto 2, ...).
	//		mentre  outputRoot è comune a tutta la playlist,
	//		Nello scenario in cui serve un drawTextDetails solamente per un inputRoot, non è possibile
	//		utilizzare outputRoot altrimenti avremmo il draw text anche per gli altri item della playlist.
	//		In particolare, il parametro inputDrawTextDetailsRoot arriva inizializzato solamente se
	//		siamo nello scenario di un solo inputRoot che richiede il suo drawtext.
	//		Per questo motivo, il prossimo if, gestisce il caso di drawTextDetails solo per un input root,
	// 2024-05-17: non serve piu, la regola ora è che se abbiamo un inputFilters questo ha la precedenza sull'outputFilters
	/*
	string ffmpegDrawTextFilter;
	if (inputFiltersRoot != nullptr)
	{
		{
			string text = JSONUtils::asString(inputFiltersRoot, "text", "");

			string textTemporaryFileName = getDrawTextTemporaryPathName(ingestionJobKey, encodingJobKey);
			{
				ofstream of(textTemporaryFileName, ofstream::trunc);
				of << text;
				of.flush();
			}

			_logger->info(
				__FILEREF__ + "outputsRootToFfmpeg (inputRoot): added text into a temporary file" + ", ingestionJobKey: " +
				to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) + ", textTemporaryFileName: " +
	textTemporaryFileName
			);

			json filterRoot = inputDrawTextDetailsRoot;
			filterRoot["type"] = "drawtext";
			filterRoot["textFilePathName"] = textTemporaryFileName;
			ffmpegDrawTextFilter = ffmpegFilters.getFilter(filterRoot, streamingDurationInSeconds);
		}
	}
	*/

	for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		json outputRoot = outputsRoot[outputIndex];

		string outputType = JSONUtils::asString(outputRoot, "outputType", "");

		string inputVideoMap = JSONUtils::asString(outputRoot, "inputVideoMap", "");
		string inputAudioMap = JSONUtils::asString(outputRoot, "inputAudioMap", "");

		// 2024-05-17: inputFiltersRoot se presente si aggiunge al filtersRoot dell'output,
		// 	Scenario di un Broadcast (Live Channel).
		// 	All'interno del json dell'encodingJob, inputsRoot rappresenta l'intera playlist del live channel
		//    (dalle ore A alle ora B contenuto 1, dalle ora C alle ore D contentuto 2, ...).
		//    mentre  outputRoot è comune a tutta la playlist,
		//    Nello scenario in cui serve un drawTextDetails solamente per un inputRoot, non è possibile
		//    utilizzare outputRoot altrimenti avremmo il draw text anche per gli altri item della playlist.
		//    In particolare, il parametro inputFiltersRoot arriva inizializzato solamente se
		//    siamo nello scenario di un solo inputRoot che richiede il suo drawtext.
		//    Per questo motivo, il prossimo if, gestisce il caso di drawTextDetails solo per un input root
		json filtersRoot = ffmpegFilters.mergeFilters(outputRoot["filters"], inputFiltersRoot);
		SPDLOG_INFO(
			"mergeFilters"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", outputRoot[filters]: {}"
			", inputFiltersRoot: {}"
			", filtersRoot: {}",
			ingestionJobKey, encodingJobKey, JSONUtils::toString(outputRoot["filters"]), JSONUtils::toString(inputFiltersRoot),
			JSONUtils::toString(filtersRoot)
		);

		// in caso di drawtext filter, set textFilePathName sicuramente se è presente reloadAtFrameInterval
		// Inoltre in caso di caratteri speciali come ', bisogna usare il file
		{
			if (filtersRoot != nullptr)
			{
				if (JSONUtils::isMetadataPresent(filtersRoot, "video"))
				{
					json videoFiltersRoot = filtersRoot["video"];
					for (int filterIndex = 0; filterIndex < videoFiltersRoot.size(); filterIndex++)
					{
						json videoFilterRoot = videoFiltersRoot[filterIndex];
						if (JSONUtils::isMetadataPresent(videoFilterRoot, "type") && videoFilterRoot["type"] == "drawtext")
						{
							int reloadAtFrameInterval = JSONUtils::asInt(videoFilterRoot, "reloadAtFrameInterval", -1);
							string overlayText = JSONUtils::asString(videoFilterRoot, "text", "");
							if (reloadAtFrameInterval > 0 ||
								// caratteri dove non si puo usare escape
								overlayText.find("'") != string::npos)
							{
								string textTemporaryFileName = getDrawTextTemporaryPathName(ingestionJobKey, encodingJobKey, outputIndex);
								{
									ofstream of(textTemporaryFileName, ofstream::trunc);
									of << overlayText;
									of.flush();
								}

								videoFilterRoot["textFilePathName"] = textTemporaryFileName;
								videoFiltersRoot[filterIndex] = videoFilterRoot;
								filtersRoot["video"] = videoFiltersRoot;
							}
						}
					}
				}
			}
		}

		json encodingProfileDetailsRoot = nullptr;
		if (JSONUtils::isMetadataPresent(outputRoot, "encodingProfileDetails"))
			encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];

		string otherOutputOptions = JSONUtils::asString(outputRoot, "otherOutputOptions", "");

		string encodingProfileContentType = JSONUtils::asString(outputRoot, "encodingProfileContentType", "Video");
		bool isVideo = encodingProfileContentType == "Video" ? true : false;

		/*
		if (ffmpegDrawTextFilter == "" && JSONUtils::isMetadataPresent(outputRoot, "drawTextDetails"))
		{
			string field = "drawTextDetails";
			json drawTextDetailsRoot = outputRoot[field];

			string text = JSONUtils::asString(drawTextDetailsRoot, "text", "");

			string textTemporaryFileName = getDrawTextTemporaryPathName(ingestionJobKey, encodingJobKey, outputIndex);
			{
				ofstream of(textTemporaryFileName, ofstream::trunc);
				of << text;
				of.flush();
			}

			// string ffmpegDrawTextFilter;
			{
				json filterRoot = drawTextDetailsRoot;
				filterRoot["type"] = "drawtext";
				filterRoot["textFilePathName"] = textTemporaryFileName;
				ffmpegDrawTextFilter = ffmpegFilters.getFilter(filterRoot, streamingDurationInSeconds);
			}
		}
		*/

		string httpStreamingFileFormat;
		string ffmpegHttpStreamingParameter = "";

		string ffmpegFileFormatParameter = "";

		string ffmpegVideoCodecParameter = "";
		string ffmpegVideoProfileParameter = "";
		string ffmpegVideoResolutionParameter = "";
		int videoBitRateInKbps = -1;
		string ffmpegVideoBitRateParameter = "";
		string ffmpegVideoOtherParameters = "";
		string ffmpegVideoMaxRateParameter = "";
		string ffmpegVideoBufSizeParameter = "";
		string ffmpegVideoFrameRateParameter = "";
		string ffmpegVideoKeyFramesRateParameter = "";
		bool twoPasses;
		vector<tuple<string, int, int, int, string, string, string>> videoBitRatesInfo;

		string ffmpegAudioCodecParameter = "";
		string ffmpegAudioBitRateParameter = "";
		string ffmpegAudioOtherParameters = "";
		string ffmpegAudioChannelsParameter = "";
		string ffmpegAudioSampleRateParameter = "";
		vector<string> audioBitRatesInfo;

		if (encodingProfileDetailsRoot != nullptr)
		{
			try
			{
				FFMpegEncodingParameters::settingFfmpegParameters(
					_logger, encodingProfileDetailsRoot, isVideo,

					httpStreamingFileFormat, ffmpegHttpStreamingParameter,

					ffmpegFileFormatParameter,

					ffmpegVideoCodecParameter, ffmpegVideoProfileParameter, ffmpegVideoOtherParameters, twoPasses, ffmpegVideoFrameRateParameter,
					ffmpegVideoKeyFramesRateParameter, videoBitRatesInfo,

					ffmpegAudioCodecParameter, ffmpegAudioOtherParameters, ffmpegAudioChannelsParameter, ffmpegAudioSampleRateParameter,
					audioBitRatesInfo
				);

				tuple<string, int, int, int, string, string, string> videoBitRateInfo = videoBitRatesInfo[0];
				tie(ffmpegVideoResolutionParameter, videoBitRateInKbps, ignore, ignore, ffmpegVideoBitRateParameter, ffmpegVideoMaxRateParameter,
					ffmpegVideoBufSizeParameter) = videoBitRateInfo;

				ffmpegAudioBitRateParameter = audioBitRatesInfo[0];

				/*
				if (httpStreamingFileFormat != "")
				{
					string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have an httpStreaming encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else */
				if (twoPasses)
				{
					/*
					string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
					*/
					twoPasses = false;

					string errorMessage = __FILEREF__ + "in case of proxy it is not possible to have a two passes encoding. Change it to false" +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
										  ", twoPasses: " + to_string(twoPasses);
					_logger->warn(errorMessage);
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = __FILEREF__ + "encodingProfileParameter retrieving failed" +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", e.what(): " + e.what();
				_logger->error(errorMessage);

				throw e;
			}
		}

		// se abbiamo overlay, necessariamente serve un profilo di encoding
		// questo controllo sarebbe in generale, cioé se abbiamo alcuni filtri in particolare dovremmo avere un profilo di encoding
		/*
		if (ffmpegDrawTextFilter != "" && encodingProfileDetailsRoot == nullptr)
		{
			string errorMessage = fmt::format(
				"text-overlay requires an encoding profile"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", outputIndex: {}",
				ingestionJobKey, encodingJobKey, outputIndex
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		string videoFilters;
		string audioFilters;
		string complexFilters;
		if (filtersRoot != nullptr)
		{
			tuple<string, string, string> allFilters =
				ffmpegFilters.addFilters(filtersRoot, ffmpegVideoResolutionParameter, "", streamingDurationInSeconds);
			tie(videoFilters, audioFilters, complexFilters) = allFilters;
		}

		bool threadsParameterToBeAdded = false;

		// video (parametri di encoding)
		if (inputVideoMap != "" && inputVideoMap != "default")
		{
			ffmpegOutputArgumentList.push_back("-map");
			if (inputVideoMap == "all video tracks")
				ffmpegOutputArgumentList.push_back("0:v");
			else if (inputVideoMap == "first video track")
				ffmpegOutputArgumentList.push_back("0:v:0");
			else if (inputVideoMap == "second video track")
				ffmpegOutputArgumentList.push_back("0:v:1");
			else if (inputVideoMap == "third video track")
				ffmpegOutputArgumentList.push_back("0:v:2");
			else
				ffmpegOutputArgumentList.push_back(inputVideoMap);
		}
		if (encodingProfileDetailsRoot != nullptr)
		{
			threadsParameterToBeAdded = true;

			{
				FFMpegEncodingParameters::addToArguments(ffmpegVideoCodecParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoProfileParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBitRateParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoOtherParameters, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoMaxRateParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoBufSizeParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoFrameRateParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegVideoKeyFramesRateParameter, ffmpegOutputArgumentList);
				// ffmpegVideoResolutionParameter is -vf scale=w=1280:h=720
				// Since we cannot have more than one -vf (otherwise ffmpeg will use
				// only the last one), in case we have ffmpegDrawTextFilter,
				// we will append it here

				if (videoFilters != "")
				{
					ffmpegOutputArgumentList.push_back("-filter:v");
					ffmpegOutputArgumentList.push_back(videoFilters);
				}

				if (complexFilters != "")
				{
					ffmpegOutputArgumentList.push_back("-filter_complex");
					ffmpegOutputArgumentList.push_back(complexFilters);
				}
			}
		}
		else
		{
			if (videoFilters != "")
			{
				threadsParameterToBeAdded = true;

				ffmpegOutputArgumentList.push_back("-filter:v");
				ffmpegOutputArgumentList.push_back(videoFilters);
			}
			else if (otherOutputOptions.find("-filter:v") == string::npos)
			{
				// it is not possible to have -c:v copy and -filter:v toghether
				ffmpegOutputArgumentList.push_back("-c:v");
				ffmpegOutputArgumentList.push_back("copy");
			}

			if (complexFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter_complex");
				ffmpegOutputArgumentList.push_back(complexFilters);
			}
		}

		// audio (parametri di encoding)
		if (inputAudioMap != "" && inputAudioMap != "default")
		{
			ffmpegOutputArgumentList.push_back("-map");
			if (inputAudioMap == "all audio tracks")
				ffmpegOutputArgumentList.push_back("0:a");
			else if (inputAudioMap == "first audio track")
				ffmpegOutputArgumentList.push_back("0:a:0");
			else if (inputAudioMap == "second audio track")
				ffmpegOutputArgumentList.push_back("0:a:1");
			else if (inputAudioMap == "third audio track")
				ffmpegOutputArgumentList.push_back("0:a:2");
			else
				ffmpegOutputArgumentList.push_back(inputAudioMap);
		}
		if (encodingProfileDetailsRoot != nullptr)
		{
			threadsParameterToBeAdded = true;

			{
				FFMpegEncodingParameters::addToArguments(ffmpegAudioCodecParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioBitRateParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioOtherParameters, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioChannelsParameter, ffmpegOutputArgumentList);
				FFMpegEncodingParameters::addToArguments(ffmpegAudioSampleRateParameter, ffmpegOutputArgumentList);

				if (audioFilters != "")
				{
					ffmpegOutputArgumentList.push_back("-filter:a");
					ffmpegOutputArgumentList.push_back(audioFilters);
				}
			}
		}
		else
		{
			if (audioFilters != "")
			{
				threadsParameterToBeAdded = true;

				ffmpegOutputArgumentList.push_back("-filter:a");
				ffmpegOutputArgumentList.push_back(audioFilters);
			}
			else if (otherOutputOptions.find("-filter:a") == string::npos)
			{
				// it is not possible to have -c:a copy and -filter:a toghether
				ffmpegOutputArgumentList.push_back("-c:a");
				ffmpegOutputArgumentList.push_back("copy");
			}
		}

		if (threadsParameterToBeAdded)
		{
			ffmpegOutputArgumentList.push_back("-threads");
			ffmpegOutputArgumentList.push_back("0");
		}

		// output file
		if (outputType == "CDN_AWS" || outputType == "CDN_CDN77" || outputType == "RTMP_Channel")
		{
			string rtmpUrl = JSONUtils::asString(outputRoot, "rtmpUrl", "");
			if (rtmpUrl == "")
			{
				string errorMessage = __FILEREF__ + "rtmpUrl cannot be empty" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rtmpUrl: " + rtmpUrl;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			// otherOutputOptions
			{
				if (otherOutputOptions.find("-map") == string::npos && otherOutputOptionsBecauseOfMaxWidth != "")
					FFMpegEncodingParameters::addToArguments(otherOutputOptions + otherOutputOptionsBecauseOfMaxWidth, ffmpegOutputArgumentList);
				else
					FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegOutputArgumentList);
			}

			// 2023-01-14
			// the aac_adtstoasc filter is needed only in case of an AAC input, otherwise
			// it will generate an error
			// Questo filtro è sempre stato aggiunto. Ora abbiamo il caso di un codec audio mp2 che
			// genera un errore.
			// Per essere conservativo ed evitare problemi, il controllo sotto viene fatto in 'logica negata'.
			// cioè invece di abilitare il filtro per i codec aac, lo disabilitiamo per il codec mp2
			// 2023-01-15
			// Il problema 'sopra' che, a seguito del codec mp2 veniva generato un errore,
			// è stato risolto aggiungendo un encoding in uscita.
			// Per questo motivo sotto viene commentato

			// 2023-04-06: La logica negata è sbagliata, perchè il filtro aac_adtstoasc funziona solamente
			//	con aac. Infatti ora ho trovato un caso di mp3 che non funziona con aac_adtstoasc
			{
				/*
				bool aacFilterToBeAdded = true;
				for(tuple<int, int64_t, string, long, int, long, string> inputAudioTrack: inputAudioTracks)
				{
					// trackIndex, audioDurationInMilliSeconds, audioCodecName,
					// audioSampleRate, audioChannels, audioBitRate, language));
					string audioCodecName;

					tie(ignore, ignore, audioCodecName, ignore, ignore, ignore, ignore) = inputAudioTrack;

					string audioCodecNameLowerCase;
					audioCodecNameLowerCase.resize(audioCodecName.size());
					transform(audioCodecName.begin(), audioCodecName.end(), audioCodecNameLowerCase.begin(),
						[](unsigned char c){return tolower(c); } );

					if (audioCodecNameLowerCase.find("mp2") != string::npos
					)
						aacFilterToBeAdded = false;

					_logger->info(__FILEREF__ + "aac check"
						+ ", audioCodecName: " + audioCodecName
						+ ", aacFilterToBeAdded: " + to_string(aacFilterToBeAdded)
					);
				}

				if (aacFilterToBeAdded)
				*/
				{
					ffmpegOutputArgumentList.push_back("-bsf:a");
					ffmpegOutputArgumentList.push_back("aac_adtstoasc");
				}
			}

			// 2020-08-13: commented bacause -c:v copy is already present
			// ffmpegArgumentList.push_back("-vcodec");
			// ffmpegArgumentList.push_back("copy");

			// right now it is fixed flv, it means cdnURL will be like "rtmp://...."
			ffmpegOutputArgumentList.push_back("-f");
			ffmpegOutputArgumentList.push_back("flv");
			ffmpegOutputArgumentList.push_back(rtmpUrl);
		}
		else if (outputType == "HLS_Channel")
		{
			string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
			string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");
			int segmentDurationInSeconds = JSONUtils::asInt(outputRoot, "segmentDurationInSeconds", 10);
			int playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);

			string manifestFilePathName = manifestDirectoryPath + "/" + manifestFileName;

			_logger->info(
				__FILEREF__ + "Checking manifestDirectoryPath directory" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", encodingJobKey: " + to_string(encodingJobKey) + ", manifestDirectoryPath: " + manifestDirectoryPath
			);

			// directory is created by EncoderVideoAudioProxy using MMSStorage::getStagingAssetPathName
			// I saw just once that the directory was not created and the liveencoder remains in the loop
			// where:
			//	1. the encoder returns an error because of the missing directory
			//	2. EncoderVideoAudioProxy calls again the encoder
			// So, for this reason, the below check is done
			if (!fs::exists(manifestDirectoryPath))
			{
				_logger->warn(
					__FILEREF__ + "manifestDirectoryPath does not exist!!! It will be created" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", manifestDirectoryPath: " + manifestDirectoryPath
				);

				_logger->info(__FILEREF__ + "Create directory" + ", manifestDirectoryPath: " + manifestDirectoryPath);
				fs::create_directories(manifestDirectoryPath);
				fs::permissions(
					manifestDirectoryPath,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
						fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace
				);
			}

			if (externalEncoder && manifestDirectoryPath != "")
				addToIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

			// if (outputType == "HLS")
			{
				ffmpegOutputArgumentList.push_back("-hls_flags");
				ffmpegOutputArgumentList.push_back("append_list");
				ffmpegOutputArgumentList.push_back("-hls_time");
				ffmpegOutputArgumentList.push_back(to_string(segmentDurationInSeconds));
				ffmpegOutputArgumentList.push_back("-hls_list_size");
				ffmpegOutputArgumentList.push_back(to_string(playlistEntriesNumber));

				// Segment files removed from the playlist are deleted after a period of time
				// equal to the duration of the segment plus the duration of the playlist
				ffmpegOutputArgumentList.push_back("-hls_flags");
				ffmpegOutputArgumentList.push_back("delete_segments");

				// Set the number of unreferenced segments to keep on disk
				// before 'hls_flags delete_segments' deletes them. Increase this to allow continue clients
				// to download segments which were recently referenced in the playlist.
				// Default value is 1, meaning segments older than hls_list_size+1 will be deleted.
				ffmpegOutputArgumentList.push_back("-hls_delete_threshold");
				ffmpegOutputArgumentList.push_back(to_string(1));

				// Start the playlist sequence number (#EXT-X-MEDIA-SEQUENCE) based on the current
				// date/time as YYYYmmddHHMMSS. e.g. 20161231235759
				// 2020-07-11: For the Live-Grid task, without -hls_start_number_source we have video-audio out of sync
				// 2020-07-19: commented, if it is needed just test it
				// ffmpegArgumentList.push_back("-hls_start_number_source");
				// ffmpegArgumentList.push_back("datetime");

				// 2020-07-19: commented, if it is needed just test it
				// ffmpegArgumentList.push_back("-start_number");
				// ffmpegArgumentList.push_back(to_string(10));
			}
			/*
			else if (outputType == "DASH")
			{
				ffmpegOutputArgumentList.push_back("-seg_duration");
				ffmpegOutputArgumentList.push_back(to_string(segmentDurationInSeconds));
				ffmpegOutputArgumentList.push_back("-window_size");
				ffmpegOutputArgumentList.push_back(to_string(playlistEntriesNumber));

				// it is important to specify -init_seg_name because those files
				// will not be removed in EncoderVideoAudioProxy.cpp
				ffmpegOutputArgumentList.push_back("-init_seg_name");
				ffmpegOutputArgumentList.push_back("init-stream$RepresentationID$.$ext$");

				// the only difference with the ffmpeg default is that default is $Number%05d$
				// We had to change it to $Number%01d$ because otherwise the generated file containing
				// 00001 00002 ... but the videojs player generates file name like 1 2 ...
				// and the streaming was not working
				ffmpegOutputArgumentList.push_back("-media_seg_name");
				ffmpegOutputArgumentList.push_back("chunk-stream$RepresentationID$-$Number%01d$.$ext$");
			}
			*/

			// otherOutputOptions
			// 2023-12-06: ho dovuto spostare otherOutputOptions qui perchè, nel caso del monitorHLS del LiveRecording,
			// viene aggiunto "-hls_flags program_date_time", per avere #EXT-X-PROGRAM-DATE-TIME: nell'm3u8 e,
			// se aggiunto prima, non funziona (EXT-X-PROGRAM-DATE-TIME non viene aggiunto nell'm3u8).
			// EXT-X-PROGRAM-DATE-TIME è importante per avere i campi utcEndTimeInMilliSecs e utcStartTimeInMilliSecs
			// inizializzati correttamente nello userData del media item virtual VOD generato
			{
				if (otherOutputOptions.find("-map") == string::npos && otherOutputOptionsBecauseOfMaxWidth != "")
					FFMpegEncodingParameters::addToArguments(otherOutputOptions + otherOutputOptionsBecauseOfMaxWidth, ffmpegOutputArgumentList);
				else
					FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegOutputArgumentList);
			}

			ffmpegOutputArgumentList.push_back("-f");
			ffmpegOutputArgumentList.push_back("hls");
			ffmpegOutputArgumentList.push_back(manifestFilePathName);
		}
		else if (outputType == "UDP_Stream")
		{
			string udpUrl = JSONUtils::asString(outputRoot, "udpUrl", "");

			if (udpUrl == "")
			{
				string errorMessage = __FILEREF__ + "udpUrl cannot be empty" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", udpUrl: " + udpUrl;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			// otherOutputOptions
			{
				if (otherOutputOptions.find("-map") == string::npos && otherOutputOptionsBecauseOfMaxWidth != "")
					FFMpegEncodingParameters::addToArguments(otherOutputOptions + otherOutputOptionsBecauseOfMaxWidth, ffmpegOutputArgumentList);
				else
					FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegOutputArgumentList);
			}

			ffmpegOutputArgumentList.push_back("-f");
			ffmpegOutputArgumentList.push_back("mpegts");
			ffmpegOutputArgumentList.push_back(udpUrl);
		}
		else if (outputType == "NONE")
		{
			// otherOutputOptions
			{
				if (otherOutputOptions.find("-map") == string::npos && otherOutputOptionsBecauseOfMaxWidth != "")
					FFMpegEncodingParameters::addToArguments(otherOutputOptions + otherOutputOptionsBecauseOfMaxWidth, ffmpegOutputArgumentList);
				else
					FFMpegEncodingParameters::addToArguments(otherOutputOptions, ffmpegOutputArgumentList);
			}

			tuple<string, string, string> allFilters = ffmpegFilters.addFilters(filtersRoot, "", "", -1);

			string videoFilters;
			string audioFilters;
			string complexFilters;
			tie(videoFilters, audioFilters, complexFilters) = allFilters;

			if (videoFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter:v");
				ffmpegOutputArgumentList.push_back(videoFilters);
			}

			if (audioFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter:a");
				ffmpegOutputArgumentList.push_back(audioFilters);
			}

			if (complexFilters != "")
			{
				ffmpegOutputArgumentList.push_back("-filter_complex");
				ffmpegOutputArgumentList.push_back(complexFilters);
			}

			ffmpegOutputArgumentList.push_back("-f");
			ffmpegOutputArgumentList.push_back("null");
			ffmpegOutputArgumentList.push_back("-");
		}
		else
		{
			string errorMessage = __FILEREF__ + "liveProxy. Wrong output type" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", outputType: " + outputType;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
}

void FFMpeg::outputsRootToFfmpeg_clean(int64_t ingestionJobKey, int64_t encodingJobKey, json outputsRoot, bool externalEncoder)
{

	for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		json outputRoot = outputsRoot[outputIndex];

		string outputType = JSONUtils::asString(outputRoot, "outputType", "");

		// if (outputType == "HLS" || outputType == "DASH")
		if (outputType == "HLS_Channel")
		{
			string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");

			if (externalEncoder && manifestDirectoryPath != "")
				removeFromIncrontab(ingestionJobKey, encodingJobKey, manifestDirectoryPath);

			if (manifestDirectoryPath != "")
			{
				if (fs::exists(manifestDirectoryPath))
				{
					try
					{
						_logger->info(__FILEREF__ + "removeDirectory" + ", manifestDirectoryPath: " + manifestDirectoryPath);
						fs::remove_all(manifestDirectoryPath);
					}
					catch (runtime_error &e)
					{
						string errorMessage = __FILEREF__ + "remove directory failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", outputIndex: " + to_string(outputIndex) +
											  ", manifestDirectoryPath: " + manifestDirectoryPath + ", e.what(): " + e.what();
						_logger->error(errorMessage);

						throw e;
					}
					catch (exception &e)
					{
						string errorMessage = __FILEREF__ + "remove directory failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", outputIndex: " + to_string(outputIndex) +
											  ", manifestDirectoryPath: " + manifestDirectoryPath + ", e.what(): " + e.what();
						_logger->error(errorMessage);

						throw e;
					}
				}
			}
		}

		if (JSONUtils::isMetadataPresent(outputRoot, "drawTextDetails"))
		{
			string textTemporaryFileName;
			{
				textTemporaryFileName = _ffmpegTempDir + "/" + to_string(ingestionJobKey) + "_" + to_string(encodingJobKey) + "_" +
										to_string(outputIndex) + ".overlayText";
			}

			if (fs::exists(textTemporaryFileName))
			{
				_logger->info(__FILEREF__ + "Remove" + ", textTemporaryFileName: " + textTemporaryFileName);
				fs::remove_all(textTemporaryFileName);
			}
		}

		if (JSONUtils::isMetadataPresent(outputRoot, "filters"))
		{
			json filtersRoot = outputRoot["filters"];

			if (JSONUtils::isMetadataPresent(filtersRoot, "video"))
			{
				json videoFiltersRoot = filtersRoot["video"];
				for (int filterIndex = 0; filterIndex < videoFiltersRoot.size(); filterIndex++)
				{
					json videoFilterRoot = videoFiltersRoot[filterIndex];
					if (JSONUtils::isMetadataPresent(videoFilterRoot, "type") && videoFilterRoot["type"] == "drawtext")
					{
						string textTemporaryFileName =
							fmt::format("{}/{}_{}_{}.overlayText", _ffmpegTempDir, ingestionJobKey, encodingJobKey, outputIndex);
						if (fs::exists(textTemporaryFileName))
						{
							_logger->info(__FILEREF__ + "Remove" + ", textTemporaryFileName: " + textTemporaryFileName);
							fs::remove_all(textTemporaryFileName);
						}
					}
				}
			}
		}
	}
}
