/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   EnodingsManager.cpp
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#include "EncoderProxy.h"
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
/*
#include "AWSSigner.h"
#include "LocalAssetIngestionEvent.h"
#include "MMSDeliveryAuthorization.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include "opencv2/face.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect.hpp"
#include <fstream>
#include <regex>

#include <aws/core/Aws.h>
#include <aws/medialive/MediaLiveClient.h>
#include <aws/medialive/model/DescribeChannelRequest.h>
#include <aws/medialive/model/DescribeChannelResult.h>
#include <aws/medialive/model/StartChannelRequest.h>
#include <aws/medialive/model/StopChannelRequest.h>
*/

void EncoderProxy::encodeContentVideoAudio(string ffmpegURI, int maxConsecutiveEncodingStatusFailures)
{
	_logger->info(
		__FILEREF__ + "Creating encoderVideoAudioProxy thread" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", ffmpegURI: " + ffmpegURI +
		", _mp4Encoder: " + _mp4Encoder
	);

	{
		bool killedByUser = encodeContent_VideoAudio_through_ffmpeg(ffmpegURI, maxConsecutiveEncodingStatusFailures);
		if (killedByUser)
		{
			string errorMessage = __FILEREF__ + "Encoding killed by the User" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
			_logger->error(errorMessage);

			throw EncodingKilledByUser();
		}
	}
}

bool EncoderProxy::encodeContent_VideoAudio_through_ffmpeg(string ffmpegURI, int maxConsecutiveEncodingStatusFailures)
{
	string encodersPool = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, "encodersPool", "");

	string ffmpegEncoderURL;
	// string ffmpegURI = _ffmpegEncodeURI;
	try
	{
		_currentUsedFFMpegExternalEncoder = false;

		if (_encodingItem->_encoderKey == -1)
		{
			int64_t encoderKeyToBeSkipped = -1;
			bool externalEncoderAllowed = true;
			tuple<int64_t, string, bool> encoderDetails = _encodersLoadBalancer->getEncoderURL(
				_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace, encoderKeyToBeSkipped, externalEncoderAllowed
			);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;

			_logger->info(
				__FILEREF__ + "getEncoderHost" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
				to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", encodersPool: " + encodersPool + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost +
				", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			ffmpegEncoderURL = _currentUsedFFMpegEncoderHost + ffmpegURI + "/" + to_string(_encodingItem->_ingestionJobKey) + "/" +
							   to_string(_encodingItem->_encodingJobKey);
			string body;
			{
				_logger->info(
					__FILEREF__ + "building body for encoder 1" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
					to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", _directoryName: " + _encodingItem->_workspace->_directoryName
				);

				json encodingMedatada;

				// 2023-03-21: rimuovere il parametro ingestionJobKey se il
				// trascoder deployed è > 1.0.5315
				encodingMedatada["ingestionJobKey"] = _encodingItem->_ingestionJobKey;
				encodingMedatada["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
				encodingMedatada["encodingParametersRoot"] = _encodingItem->_encodingParametersRoot;
				encodingMedatada["ingestedParametersRoot"] = _encodingItem->_ingestedParametersRoot;

				body = JSONUtils::toString(encodingMedatada);
			}

			vector<string> otherHeaders;
			json encodeContentResponse;
			try
			{
				encodeContentResponse = MMSCURL::httpPostStringAndGetJson(
					_logger, _encodingItem->_ingestionJobKey, ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, _ffmpegEncoderUser,
					_ffmpegEncoderPassword, body,
					"application/json", // contentType
					otherHeaders
				);
			}
			catch (runtime_error &e)
			{
				string error = e.what();
				if (error.find(EncodingIsAlreadyRunning().what()) != string::npos)
				{
					// 2023-03-26:
					// Questo scenario indica che per il DB "l'encoding è da
					// eseguire" mentre abbiamo un Encoder che lo sta già
					// eseguendo Si tratta di una inconsistenza che non dovrebbe
					// mai accadere. Oggi pero' ho visto questo scenario e l'ho
					// risolto facendo ripartire sia l'encoder che gli engines
					// Gestire questo scenario rende il sistema piu' robusto e
					// recupera facilmente una situazione che altrimenti
					// richiederebbe una gestione manuale Inoltre senza guardare
					// nel log, non si riuscirebbe a capire che siamo in questo
					// scenario.

					// La gestione di questo scenario consiste nell'ignorare
					// questa eccezione facendo andare avanti la procedura, come
					// se non avesse generato alcun errore
					_logger->error(
						__FILEREF__ +
						"inconsistency: DB says the encoding has to be "
						"executed but the Encoder is already executing it. We "
						"will manage it" +
						", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", body: " + body + ", e.what: " + e.what()
					);
				}
				else
					throw e;
			}

			/* 2023-03-26; non si verifica mai, se FFMPEGEncoder genera un
			errore, ritorna un HTTP status diverso da 200 e quindi MMSCURL
			genera un eccezione
			{
				string field = "error";
				if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
				{
					string error = JSONUtils::asString(encodeContentResponse,
			field, "");

					string errorMessage = string("FFMPEGEncoder error")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " +
			to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " +
			to_string(_encodingItem->_encodingJobKey)
						+ ", error: " + error
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			*/
		}
		else
		{
			_logger->info(
				__FILEREF__ +
				"Encode content. The transcoder is already saved, the encoding "
				"should be already running" +
				", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encoderKey: " + to_string(_encodingItem->_encoderKey) +
				", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			pair<string, bool> encoderDetails = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

			// we have to reset _encodingItem->_encoderKey because in case we
			// will come back in the above 'while' loop, we have to select
			// another encoder
			_encodingItem->_encoderKey = -1;

			ffmpegEncoderURL = _currentUsedFFMpegEncoderHost + ffmpegURI + "/" + to_string(_encodingItem->_encodingJobKey);
		}

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(
			__FILEREF__ + "Update EncodingJob" + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", transcoder: " + _currentUsedFFMpegEncoderHost + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(
			_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, ""
		); // stagingEncodedAssetPathName);

		// int maxConsecutiveEncodingStatusFailures = 1;
		bool killedByUser = waitingEncoding(maxConsecutiveEncodingStatusFailures);

		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "Encoded media file" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL +
			", @MMS statistics@ - encodingDuration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@" +
			", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return killedByUser;
	}
	catch (EncoderNotFound e)
	{
		_logger->error(
			__FILEREF__ + "Encoder not found" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", ffmpegEncoderURL: " + ffmpegEncoderURL + ", exception: " + e.what()
		);

		throw e;
	}
	catch (MaxConcurrentJobsReached &e)
	{
		string errorMessage = string("MaxConcurrentJobsReached") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", e.what(): " + e.what();
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (runtime_error e)
	{
		string error = e.what();
		if (error.find(NoEncodingAvailable().what()) != string::npos || error.find(MaxConcurrentJobsReached().what()) != string::npos)
		{
			string errorMessage = string("No Encodings available / MaxConcurrentJobsReached") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", error: " + error;
			_logger->warn(__FILEREF__ + errorMessage);

			throw MaxConcurrentJobsReached();
		}
		else
		{
			_logger->error(
				__FILEREF__ + "Encoding URL failed (runtime_error)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
				to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", ffmpegEncoderURL: " + ffmpegEncoderURL + ", exception: " + e.what()
			);

			throw e;
		}
	}
	catch (exception e)
	{
		_logger->error(
			__FILEREF__ + "Encoding URL failed (exception)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", ffmpegEncoderURL: " + ffmpegEncoderURL + ", exception: " + e.what()
		);

		throw e;
	}
}

void EncoderProxy::processEncodedContentVideoAudio()
{
	_logger->info(
		__FILEREF__ + "processEncodedContentVideoAudio" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
		", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
	);

	if (_currentUsedFFMpegExternalEncoder)
	{
		_logger->info(
			__FILEREF__ +
			"The encoder selected is external, processEncodedContentVideoAudio "
			"has nothing to do" +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
		);

		return;
	}

	json sourcesToBeEncodedRoot;
	json sourceToBeEncodedRoot;
	string encodedNFSStagingAssetPathName;
	json encodingProfileDetailsRoot;
	int64_t sourceMediaItemKey;
	int64_t encodingProfileKey;
	string sourceRelativePath;
	int64_t physicalItemRetentionInMinutes = -1;
	try
	{
		sourcesToBeEncodedRoot = _encodingItem->_encodingParametersRoot["sourcesToBeEncoded"];
		if (sourcesToBeEncodedRoot.size() == 0)
		{
			string errorMessage = __FILEREF__ + "No sourceToBeEncoded found" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		sourceToBeEncodedRoot = sourcesToBeEncodedRoot[0];

		encodedNFSStagingAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, "encodedNFSStagingAssetPathName", "");
		if (encodedNFSStagingAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty" +
								  ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		encodingProfileDetailsRoot = _encodingItem->_encodingParametersRoot["encodingProfileDetails"];

		string field = "sourceRelativePath";
		sourceRelativePath = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

		field = "sourceMediaItemKey";
		sourceMediaItemKey = JSONUtils::asInt64(sourceToBeEncodedRoot, field, 0);

		field = "encodingProfileKey";
		encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		field = "physicalItemRetention";
		if (JSONUtils::isMetadataPresent(_encodingItem->_ingestedParametersRoot, field))
		{
			string retention = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, field, "1d");
			physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
		}
	}
	catch (runtime_error e)
	{
		_logger->error(
			__FILEREF__ + "Initialization encoding variables error" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(
			__FILEREF__ + "Initialization encoding variables error" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", exception: " + e.what()
		);

		throw e;
	}

	string fileFormat = JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");
	string fileFormatLowerCase;
	fileFormatLowerCase.resize(fileFormat.size());
	transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(), [](unsigned char c) { return tolower(c); });

	// manifestFileName is used in case of
	// MMSEngineDBFacade::EncodingTechnology::MPEG2_TS the manifestFileName
	// naming convention is used also in FFMpeg.cpp
	string manifestFileName = to_string(_encodingItem->_ingestionJobKey) + "_" + to_string(_encodingItem->_encodingJobKey);
	if (fileFormatLowerCase == "hls")
		manifestFileName += ".m3u8";
	else if (fileFormatLowerCase == "dash")
		manifestFileName += ".mpd";

	tuple<int64_t, long, json> mediaInfoDetails;
	vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
	vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;

	int imageWidth = -1;
	int imageHeight = -1;
	string imageFormat;
	int imageQuality = -1;
	try
	{
		int timeoutInSeconds = 20;

		_logger->info(
			__FILEREF__ + "Calling getMediaInfo" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", fileFormatLowerCase: " + fileFormatLowerCase + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
		);
		bool isMMSAssetPathName = true;
		FFMpeg ffmpeg(_configuration, _logger);
		if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
		{
			mediaInfoDetails = ffmpeg.getMediaInfo(
				_encodingItem->_ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, encodedNFSStagingAssetPathName + "/" + manifestFileName,
				videoTracks, audioTracks
			);
		}
		else
		{
			mediaInfoDetails = ffmpeg.getMediaInfo(
				_encodingItem->_ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, encodedNFSStagingAssetPathName, videoTracks, audioTracks
			);
		}

		// tie(durationInMilliSeconds, bitRate,
		//     videoCodecName, videoProfile, videoWidth, videoHeight,
		//     videoAvgFrameRate, videoBitRate, audioCodecName, audioSampleRate,
		//     audioChannels, audioBitRate) = mediaInfo;
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "EncoderProxy::getMediaInfo failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName + ", e.what(): " + e.what()
		);

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory" + ", directoryPathName: " + directoryPathName);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				_logger->error(
					__FILEREF__ + "removeDirectory failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", directoryPathName: " + directoryPathName +
					", exception: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "EncoderProxy::getMediaInfo failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName + ", sourceRelativePath: " + sourceRelativePath
		);

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory" + ", directoryPathName: " + directoryPathName);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				_logger->error(
					__FILEREF__ + "removeDirectory failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", directoryPathName: " + directoryPathName +
					", exception: " + e.what()
				);
			}
		}

		throw e;
	}

	int64_t encodedPhysicalPathKey;
	string encodedFileName;
	string mmsAssetPathName;
	unsigned long mmsPartitionIndexUsed;
	try
	{
		size_t fileNameIndex = encodedNFSStagingAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "No fileName find in the asset path name" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		encodedFileName = encodedNFSStagingAssetPathName.substr(fileNameIndex + 1);

		bool deliveryRepositoriesToo = true;

		mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
			_encodingItem->_ingestionJobKey, encodedNFSStagingAssetPathName, _encodingItem->_workspace->_directoryName, encodedFileName,
			sourceRelativePath,

			&mmsPartitionIndexUsed, // OUT
									// &sourceFileType,

			deliveryRepositoriesToo, _encodingItem->_workspace->_territories
		);
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName + ", _workspace->_directoryName: " +
			_encodingItem->_workspace->_directoryName + ", sourceRelativePath: " + sourceRelativePath + ", e.what(): " + e.what()
		);

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory" + ", directoryPathName: " + directoryPathName);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				_logger->error(
					__FILEREF__ + "removeDirectory failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", directoryPathName: " + directoryPathName +
					", exception: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName + ", sourceRelativePath: " + sourceRelativePath
		);

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory" + ", directoryPathName: " + directoryPathName);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				_logger->error(
					__FILEREF__ + "removeDirectory failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", directoryPathName: " + directoryPathName +
					", exception: " + e.what()
				);
			}
		}

		throw e;
	}

	// remove staging directory
	{
		string directoryPathName;
		try
		{
			size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
			if (endOfDirectoryIndex != string::npos)
			{
				directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

				_logger->info(__FILEREF__ + "removeDirectory" + ", directoryPathName: " + directoryPathName);
				fs::remove_all(directoryPathName);
			}
		}
		catch (runtime_error &e)
		{
			_logger->error(
				__FILEREF__ + "removeDirectory failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
				to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName + ", directoryPathName: " + directoryPathName +
				", exception: " + e.what()
			);
		}
	}

	try
	{
		unsigned long long mmsAssetSizeInBytes;
		{
			if (fs::is_directory(mmsAssetPathName))
			{
				mmsAssetSizeInBytes = 0;
				// recursive_directory_iterator, by default, does not follow sym
				// links
				for (fs::directory_entry const &entry : fs::recursive_directory_iterator(mmsAssetPathName))
				{
					if (entry.is_regular_file())
						mmsAssetSizeInBytes += entry.file_size();
				}
			}
			else
			{
				mmsAssetSizeInBytes = fs::file_size(mmsAssetPathName);
			}
		}

		string newSourceRelativePath = sourceRelativePath;

		if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
		{
			size_t segmentsDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
			if (segmentsDirectoryIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "No segmentsDirectory find in the asset path name" +
									  ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
									  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									  ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			// in case of MPEG2_TS next 'stagingEncodedAssetPathName.substr'
			// extract the directory name containing manifest and ts files. So
			// relativePath has to be extended with this directory
			newSourceRelativePath += (encodedNFSStagingAssetPathName.substr(segmentsDirectoryIndex + 1) + "/");

			// in case of MPEG2_TS, encodedFileName is the manifestFileName
			encodedFileName = manifestFileName;
		}

		bool externalReadOnlyStorage = false;
		string externalDeliveryTechnology;
		string externalDeliveryURL;
		int64_t liveRecordingIngestionJobKey = -1;
		encodedPhysicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
			_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, liveRecordingIngestionJobKey, sourceMediaItemKey,
			externalReadOnlyStorage, externalDeliveryTechnology, externalDeliveryURL, encodedFileName, newSourceRelativePath, mmsPartitionIndexUsed,
			mmsAssetSizeInBytes, encodingProfileKey, physicalItemRetentionInMinutes,

			mediaInfoDetails, videoTracks, audioTracks,

			imageWidth, imageHeight, imageFormat, imageQuality
		);

		_logger->info(
			__FILEREF__ + "Saved the Encoded content" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey) + ", newSourceRelativePath: " + newSourceRelativePath +
			", encodedFileName: " + encodedFileName
		);
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName + ", e.what(): " + e.what()
		);

		_logger->info(__FILEREF__ + "remove" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", mmsAssetPathName: " + mmsAssetPathName);
		fs::remove_all(mmsAssetPathName);

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
		);

		// file in case of .3gp content OR directory in case of IPhone content
		if (fs::exists(mmsAssetPathName))
		{
			_logger->info(__FILEREF__ + "remove" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", mmsAssetPathName: " + mmsAssetPathName);
			fs::remove_all(mmsAssetPathName);
		}

		throw e;
	}
}
