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
// #include "FFMpeg.h"
#include "JSONUtils.h"
#include "LocalAssetIngestionEvent.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "spdlog/spdlog.h"

void EncoderProxy::processLiveGrid(bool killedByUser)
{
	try
	{
		// This method is never called because in both the scenarios below an
		// exception by EncoderProxy::liveGrid is raised:
		// - transcoding killed by the user
		// - The max number of calls to the URL were all done and all failed
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processLiveGrid failed"
			", _proxyIdentifier: {}"
			", _encodingJobKey: {}"
			", _ingestionJobKey: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey, _encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processLiveGrid failed"
			", _proxyIdentifier: {}"
			", _encodingJobKey: {}"
			", _ingestionJobKey: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey, _encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processSlideShow()
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processSlideShow has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = std::format(
				"No extention found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = std::format(
				"No fileName found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

		vector<int64_t> slideShowOfImageMediaItemKeys;
		vector<int64_t> slideShowOfAudioMediaItemKeys;
		{
			for (const auto &imageRoot : _encodingItem->_encodingParametersRoot["imagesRoot"])
				slideShowOfImageMediaItemKeys.push_back(imageRoot["sourceMediaItemKey"]);
			for (const auto &audioRoot : _encodingItem->_encodingParametersRoot["audiosRoot"])
				slideShowOfAudioMediaItemKeys.push_back(audioRoot["sourceMediaItemKey"]);
		}

		string mediaMetaDataContent = generateMediaMetadataToIngest(
			_encodingItem->_ingestionJobKey, fileFormat, -1, -1, -1, -1, // cutOfVideoMediaItemKey
			slideShowOfImageMediaItemKeys, slideShowOfAudioMediaItemKeys, _encodingItem->_ingestedParametersRoot
		);

		int outputFrameRate;
		string field = "outputFrameRate";
		outputFrameRate = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 25);

		shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

		localAssetIngestionEvent->setSource(ENCODERPROXY);
		localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
		localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
		localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
		localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
		localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
		localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", sourceFileName: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processOverlayedImageOnVideo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processOverlayedImageOnVideo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processIntroOutroOverlay()
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processIntroOutroOverlay has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = std::format(
				"No extention found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = std::format(
				"No fileName found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

		int64_t faceOfVideoMediaItemKey = -1;
		string mediaMetaDataContent = generateMediaMetadataToIngest(
			_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1, -1, // cutOfVideoMediaItemKey
			vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
		);

		shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

		localAssetIngestionEvent->setSource(ENCODERPROXY);
		localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
		localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
		localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
		localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
		localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", sourceFileName: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processIntroOutroOverlay failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processIntroOutroOverlay failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processCutFrameAccurate()
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processCutFrameAccurate has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = std::format(
				"No extention found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = std::format(
				"No fileName found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

		string field = "sourceVideoMediaItemKey";
		int64_t sourceVideoMediaItemKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		field = "newUtcStartTimeInMilliSecs";
		int64_t newUtcStartTimeInMilliSecs = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		field = "newUtcEndTimeInMilliSecs";
		int64_t newUtcEndTimeInMilliSecs = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		if (newUtcStartTimeInMilliSecs != -1 && newUtcEndTimeInMilliSecs != -1)
		{
			json destUserDataRoot;

			field = "userData";
			if (JSONUtils::isMetadataPresent(_encodingItem->_ingestedParametersRoot, field))
				destUserDataRoot = _encodingItem->_ingestedParametersRoot[field];

			json destMmsDataRoot;

			field = "mmsData";
			if (JSONUtils::isMetadataPresent(destUserDataRoot, field))
				destMmsDataRoot = destUserDataRoot[field];

			field = "utcStartTimeInMilliSecs";
			if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
				destMmsDataRoot.erase(field);
			destMmsDataRoot[field] = newUtcStartTimeInMilliSecs;

			field = "utcEndTimeInMilliSecs";
			if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
				destMmsDataRoot.erase(field);
			destMmsDataRoot[field] = newUtcEndTimeInMilliSecs;

			field = "mmsData";
			destUserDataRoot[field] = destMmsDataRoot;

			field = "userData";
			_encodingItem->_ingestedParametersRoot[field] = destUserDataRoot;
		}

		string mediaMetaDataContent = generateMediaMetadataToIngest(
			_encodingItem->_ingestionJobKey, fileFormat, -1, sourceVideoMediaItemKey, newUtcStartTimeInMilliSecs / 1000,
			newUtcEndTimeInMilliSecs / 1000, vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
		);

		shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

		localAssetIngestionEvent->setSource(ENCODERPROXY);
		localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
		localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
		localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
		localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
		localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", sourceFileName: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processIntroOutroOverlay failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processIntroOutroOverlay failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processGeneratedFrames(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		// Status will be success if at least one Chunk was generated, otherwise
		// it will be failed
		{
			string errorMessage;
			string processorMMS;
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

			SPDLOG_INFO(
				"Update IngestionJob"
				", ingestionJobKey: {}"
				", IngestionStatus: {}"
				", errorMessage: {}"
				", processorMMS: {}",
				_encodingItem->_ingestionJobKey, MMSEngineDBFacade::toString(newIngestionStatus), errorMessage, processorMMS
			);
			_mmsEngineDBFacade->updateIngestionJob(_encodingItem->_ingestionJobKey, newIngestionStatus, errorMessage);
		}
	}
	else
	{
		// here we do not have just a profile to be added into MMS but we have
		// one or more MediaItemKeys that have to be ingested
		// One MIK in case of a .mjpeg
		// One or more MIKs in case of .jpg

		shared_ptr<MultiLocalAssetIngestionEvent> multiLocalAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<MultiLocalAssetIngestionEvent>(
				MMSENGINE_EVENTTYPEIDENTIFIER_MULTILOCALASSETINGESTIONEVENT
			);

		multiLocalAssetIngestionEvent->setSource(ENCODERPROXY);
		multiLocalAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		multiLocalAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		multiLocalAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		multiLocalAssetIngestionEvent->setEncodingJobKey(_encodingItem->_encodingJobKey);
		multiLocalAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		multiLocalAssetIngestionEvent->setParametersRoot(_encodingItem->_ingestedParametersRoot);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(multiLocalAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (MULTIINGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, event->getEventKey().first, event->getEventKey().second
		);
	}
}

void EncoderProxy::processAddSilentAudio(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processVideoSpeed has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		json sourcesRoot = _encodingItem->_encodingParametersRoot["sources"];

		for (int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
		{
			json sourceRoot = sourcesRoot[sourceIndex];

			stagingEncodedAssetPathName = JSONUtils::asString(sourceRoot, "encodedNFSStagingAssetPathName", "");
			if (stagingEncodedAssetPathName == "")
			{
				string errorMessage = std::format(
					"encodedNFSStagingAssetPathName cannot be empty"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", stagingEncodedAssetPathName: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
			if (extensionIndex == string::npos)
			{
				string errorMessage = std::format(
					"No extention found in the asset file name"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", stagingEncodedAssetPathName: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

			size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
			if (fileNameIndex == string::npos)
			{
				string errorMessage = std::format(
					"No fileName found in the asset file name"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", stagingEncodedAssetPathName: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

			if (!fs::exists(stagingEncodedAssetPathName))
			{
				string errorMessage = std::format(
					"stagingEncodedAssetPathName is not found"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", stagingEncodedAssetPathName: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
				);
				SPDLOG_ERROR(errorMessage);

				continue;
			}

			int64_t faceOfVideoMediaItemKey = -1;
			string mediaMetaDataContent = generateMediaMetadataToIngest(
				_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1, -1, // cutOfVideoMediaItemKey
				vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
			);

			shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
				_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

			localAssetIngestionEvent->setSource(ENCODERPROXY);
			localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
			localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

			localAssetIngestionEvent->setExternalReadOnlyStorage(false);
			localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
			localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
			localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
			localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
			localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
			localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

			localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

			shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
			_multiEventsSet->addEvent(event);

			SPDLOG_INFO(
				"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
				", _proxyIdentifier: {}"
				", ingestionJobKey: {}"
				", sourceFileName: {}"
				", getEventKey().first: {}"
				", getEventKey().second: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
			);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processVideoSpeed failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processVideoSpeed failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processPictureInPicture(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processPictureInPicture has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = std::format(
				"No extention found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = std::format(
				"No fileName found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

		int64_t faceOfVideoMediaItemKey = -1;
		string mediaMetaDataContent = generateMediaMetadataToIngest(
			_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1, -1, // cutOfVideoMediaItemKey
			vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
		);

		shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

		localAssetIngestionEvent->setSource(ENCODERPROXY);
		localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
		localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
		localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
		localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
		localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", sourceFileName: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processPictureInPicture failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processPictureInPicture failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processOverlayedTextOnVideo(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processOverlayedTextOnVideo has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = std::format(
				"No extention found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = std::format(
				"No fileName found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

		int64_t faceOfVideoMediaItemKey = -1;
		string mediaMetaDataContent = generateMediaMetadataToIngest(
			_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1, -1, // cutOfVideoMediaItemKey
			vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
		);

		shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

		localAssetIngestionEvent->setSource(ENCODERPROXY);
		localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
		localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
		localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
		localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
		localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", sourceFileName: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processOverlayedImageOnVideo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processOverlayedImageOnVideo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processVideoSpeed(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processVideoSpeed has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = std::format(
				"No extention found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = std::format(
				"No fileName found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

		int64_t faceOfVideoMediaItemKey = -1;
		string mediaMetaDataContent = generateMediaMetadataToIngest(
			_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1, -1, // cutOfVideoMediaItemKey
			vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
		);

		shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

		localAssetIngestionEvent->setSource(ENCODERPROXY);
		localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
		localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
		localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
		localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
		localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", sourceFileName: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processVideoSpeed failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processVideoSpeed failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}

void EncoderProxy::processOverlayedImageOnVideo(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
		SPDLOG_INFO(
			"The encoder selected is external, processOverlayedImageOnVideo has nothing to do"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _currentUsedFFMpegExternalEncoder: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegExternalEncoder
		);

		return;
	}

	string stagingEncodedAssetPathName;
	try
	{
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, "encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = std::format(
				"encodedNFSStagingAssetPathName cannot be empty"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = std::format(
				"No extention found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = std::format(
				"No fileName found in the asset file name"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", stagingEncodedAssetPathName: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

		int64_t faceOfVideoMediaItemKey = -1;
		string mediaMetaDataContent = generateMediaMetadataToIngest(
			_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1, -1, // cutOfVideoMediaItemKey
			vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
		);

		shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

		localAssetIngestionEvent->setSource(ENCODERPROXY);
		localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
		localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
		localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
		localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
		localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
		localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
		localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
		localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		SPDLOG_INFO(
			"addEvent: EVENT_TYPE (INGESTASSETEVENT)"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", sourceFileName: {}"
			", getEventKey().first: {}"
			", getEventKey().second: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, sourceFileName, event->getEventKey().first, event->getEventKey().second
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processOverlayedImageOnVideo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processOverlayedImageOnVideo failed"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", stagingEncodedAssetPathName: {}"
			", _workspace->_directoryName: {}"
			", e.what(): {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, stagingEncodedAssetPathName,
			_encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}
