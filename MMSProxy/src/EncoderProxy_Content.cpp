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

#include "CurlWrapper.h"
#include "EncoderProxy.h"
#include "JSONUtils.h"
#include "SafeFileSystem.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <FFMpegWrapper.h>

void EncoderProxy::encodeContentVideoAudio(string ffmpegURI, int maxConsecutiveEncodingStatusFailures)
{
	SPDLOG_INFO(
		"Creating encoderVideoAudioProxy thread"
		", _proxyIdentifier: {}"
		", ffmpegURI: {}"
		", _mp4Encoder: {}",
		_proxyIdentifier, ffmpegURI, _mp4Encoder
	);

	{
		FFMpegWrapper::KillType killTypeReceived = encodeContent_VideoAudio_through_ffmpeg(ffmpegURI, maxConsecutiveEncodingStatusFailures);
		if (killTypeReceived == FFMpegWrapper::KillType::Kill)
		{
			string errorMessage = std::format(
				"Encoding killed by the User"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
			);
			SPDLOG_ERROR(errorMessage);

			throw EncodingKilledByUser();
		}
	}
}

FFMpegWrapper::KillType EncoderProxy::encodeContent_VideoAudio_through_ffmpeg(string ffmpegURI, int maxConsecutiveEncodingStatusFailures)
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

			SPDLOG_INFO(
				"getEncoderHost"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", encodersPool: {}"
				", _currentUsedFFMpegEncoderHost: {}"
				", _currentUsedFFMpegEncoderKey: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodersPool, _currentUsedFFMpegEncoderHost,
				_currentUsedFFMpegEncoderKey
			);
			ffmpegEncoderURL = _currentUsedFFMpegEncoderHost + ffmpegURI + "/" + to_string(_encodingItem->_ingestionJobKey) + "/" +
							   to_string(_encodingItem->_encodingJobKey);
			string body;
			{
				SPDLOG_INFO(
					"building body for encoder 1"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", _directoryName: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_workspace->_directoryName
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
				encodeContentResponse = CurlWrapper::httpPostStringAndGetJson(
					ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword),
					body, "application/json", // contentType
					otherHeaders, std::format(", ingestionJobKey: {}", _encodingItem->_ingestionJobKey)
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
					SPDLOG_ERROR(
						"inconsistency: DB says the encoding has to be executed but the Encoder is already executing it. We will manage it"
						", _proxyIdentifier: {}"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", body: {}"
						", e.what: {}",
						_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, body, e.what()
					);
				}
				else
					throw e;
			}

			/* 2023-03-26; non si verifica mai, se FFMPEGEncoder genera un
			errore, ritorna un HTTP status diverso da 200 e quindi CurlWrapper
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
					error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			*/
		}
		else
		{
			SPDLOG_INFO(
				"Encode content. The transcoder is already saved, the encoding should be already running"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", encoderKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_encoderKey,
				_encodingItem->_stagingEncodedAssetPathName
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

		SPDLOG_INFO(
			"Update EncodingJob"
			", encodingJobKey: {}"
			", transcoder: {}"
			", _currentUsedFFMpegEncoderKey: {}",
			_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(
			_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, ""
		); // stagingEncodedAssetPathName);

		// int maxConsecutiveEncodingStatusFailures = 1;
		FFMpegWrapper::KillType killTypeReceived = waitingEncoding(maxConsecutiveEncodingStatusFailures);

		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

		SPDLOG_INFO(
			"Encoded media file"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", ffmpegEncoderURL: {}"
			", @MMS statistics@ - encodingDuration (secs): @{}@"
			", _intervalInSecondsToCheckEncodingFinished: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, ffmpegEncoderURL,
			chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count(), _intervalInSecondsToCheckEncodingFinished
		);

		return killTypeReceived;
	}
	catch (EncoderNotFound e)
	{
		SPDLOG_ERROR(
			"Encoder not found"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL, e.what()
		);

		throw e;
	}
	catch (MaxConcurrentJobsReached &e)
	{
		string errorMessage = std::format(
			"MaxConcurrentJobsReached"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, e.what()
		);
		SPDLOG_WARN(errorMessage);

		throw e;
	}
	catch (runtime_error e)
	{
		string error = e.what();
		if (error.find(NoEncodingAvailable().what()) != string::npos || error.find(MaxConcurrentJobsReached().what()) != string::npos)
		{
			string errorMessage = std::format(
				"No Encodings available / MaxConcurrentJobsReached"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", error: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, error
			);
			SPDLOG_WARN(errorMessage);

			throw MaxConcurrentJobsReached();
		}
		else
		{
			SPDLOG_ERROR(
				"Encoding URL failed"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", ffmpegEncoderURL: {}"
				", exception: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL, e.what()
			);

			throw e;
		}
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"Encoding URL failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processEncodedContentVideoAudio()
{
	SPDLOG_INFO(
		"processEncodedContentVideoAudio"
		", _proxyIdentifier: {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", _currentUsedFFMpegExternalEncoder: {}",
		_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
	);

	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processEncodedContentVideoAudio has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
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
			string errorMessage = std::format(
				"No sourceToBeEncoded found"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		sourceToBeEncodedRoot = sourcesToBeEncodedRoot[0];

		encodedNFSStagingAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, "encodedNFSStagingAssetPathName", "");
		if (encodedNFSStagingAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", encodedNFSStagingAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodedNFSStagingAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

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
		SPDLOG_ERROR(
			"Initialization encoding variables error"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", exception: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"Initialization encoding variables error"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", exception: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
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

		SPDLOG_INFO(
			"Calling getMediaInfo"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", fileFormatLowerCase: {}"
			", encodedNFSStagingAssetPathName: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, fileFormatLowerCase, encodedNFSStagingAssetPathName
		);
		bool isMMSAssetPathName = true;
		FFMpegWrapper ffmpeg(_configuration);
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
		SPDLOG_ERROR(
			"EncoderProxy::getMediaInfo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", encodedNFSStagingAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodedNFSStagingAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
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

					SPDLOG_INFO(
						"removeDirectory"
						", directoryPathName: {}",
						directoryPathName
					);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					"removeDirectory failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", directoryPathName: {}"
					", exception: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, directoryPathName, e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"EncoderProxy::getMediaInfo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", encodedNFSStagingAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodedNFSStagingAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
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

					SPDLOG_INFO(
						"removeDirectory"
						", directoryPathName: {}",
						directoryPathName
					);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					"removeDirectory failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", directoryPathName: {}"
					", exception: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, directoryPathName, e.what()
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
			string errorMessage = std::format(
				"No fileName find in the asset path name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", encodedNFSStagingAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodedNFSStagingAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

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
		SPDLOG_ERROR(
			"_mmsStorage->moveAssetInMMSRepository failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", encodedNFSStagingAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", sourceRelativePath: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodedNFSStagingAssetPathName,
			_encodingItem->_workspace->_directoryName, sourceRelativePath, e.what()
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

					SPDLOG_INFO(
						"removeDirectory"
						", directoryPathName: {}",
						directoryPathName
					);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					"removeDirectory failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", directoryPathName: {}"
					", exception: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, directoryPathName, e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"_mmsStorage->moveAssetInMMSRepository failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", encodedNFSStagingAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", sourceRelativePath: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodedNFSStagingAssetPathName,
			_encodingItem->_workspace->_directoryName, sourceRelativePath, e.what()
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

					SPDLOG_INFO(
						"removeDirectory"
						", directoryPathName: {}",
						directoryPathName
					);
					fs::remove_all(directoryPathName);
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					"removeDirectory failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", directoryPathName: {}"
					", exception: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, directoryPathName, e.what()
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

				SPDLOG_INFO(
					"removeDirectory"
					", directoryPathName: {}",
					directoryPathName
				);
				fs::remove_all(directoryPathName);
			}
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"removeDirectory failed"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", directoryPathName: {}"
				", exception: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, directoryPathName, e.what()
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
#ifdef SAFEFILESYSTEMTHREAD
				mmsAssetSizeInBytes =
					SafeFileSystem::fileSizeThread(mmsAssetPathName, 10, std::format(", ingestionJobKey: {}", _encodingItem->_ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
				mmsAssetSizeInBytes =
					SafeFileSystem::fileSizeProcess(mmsAssetPathName, 10, std::format(", ingestionJobKey: {}", _encodingItem->_ingestionJobKey));
#else
				mmsAssetSizeInBytes = fs::file_size(mmsAssetPathName);
#endif
			}
		}

		string newSourceRelativePath = sourceRelativePath;

		if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
		{
			size_t segmentsDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
			if (segmentsDirectoryIndex == string::npos)
			{
				string errorMessage = std::format(
					"No segmentsDirectory find in the asset path name"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", encodedNFSStagingAssetPathName: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodedNFSStagingAssetPathName
				);
				SPDLOG_ERROR(errorMessage);

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

		SPDLOG_INFO(
			"Saved the Encoded content"
			", _proxyIdentifier: {}"
			", _encodingJobKey: {}"
			", _ingestionJobKey: {}"
			", encodedPhysicalPathKey: {}"
			", newSourceRelativePath: {}"
			", encodedFileName: {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey, encodedPhysicalPathKey, newSourceRelativePath,
			encodedFileName
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"_mmsEngineDBFacade->saveVariantContentMetadata failed"
			", _proxyIdentifier: {}"
			", _encodingJobKey: {}"
			", _ingestionJobKey: {}"
			", encodedNFSStagingAssetPathName: {}"
			", e.what: {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey, encodedNFSStagingAssetPathName, e.what()
		);

		SPDLOG_INFO(
			"removeDirectory"
			", mmsAssetPathName: {}",
			mmsAssetPathName
		);
		fs::remove_all(mmsAssetPathName);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"_mmsEngineDBFacade->saveVariantContentMetadata failed"
			", _proxyIdentifier: {}"
			", _encodingJobKey: {}"
			", _ingestionJobKey: {}"
			", encodedNFSStagingAssetPathName: {}"
			", e.what: {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey, encodedNFSStagingAssetPathName, e.what()
		);

		// file in case of .3gp content OR directory in case of IPhone content
		if (fs::exists(mmsAssetPathName))
		{
			SPDLOG_INFO(
				"removeDirectory"
				", mmsAssetPathName: {}",
				mmsAssetPathName
			);
			fs::remove_all(mmsAssetPathName);
		}

		throw e;
	}
}
