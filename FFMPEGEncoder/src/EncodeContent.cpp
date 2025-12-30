
#include "EncodeContent.h"

#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "spdlog/spdlog.h"

void EncodeContent::encodeContent(json metadataRoot)
{
	string api = "encodeContent";

	SPDLOG_INFO(
		"Received {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", requestBody: {}",
		api, _encoding->_ingestionJobKey, _encoding->_encodingJobKey, JSONUtils::toString(metadataRoot)
	);

	bool externalEncoder = false;
	string sourceAssetPathName;
	string encodedStagingAssetPathName;
	// int64_t ingestionJobKey = 1;
	try
	{
		// json metadataRoot = JSONUtils::toJson<json>(
		// 	-1, _encodingJobKey, requestBody);

		// ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);

		externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);

		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];

		int videoTrackIndexToBeUsed = JSONUtils::asInt32(ingestedParametersRoot, "VideoTrackIndex", -1);
		int audioTrackIndexToBeUsed = JSONUtils::asInt32(ingestedParametersRoot, "AudioTrackIndex", -1);

		json filtersRoot = nullptr;
		if (JSONUtils::isPresent(ingestedParametersRoot, "filters"))
			filtersRoot = ingestedParametersRoot["filters"];

		json sourcesToBeEncodedRoot = encodingParametersRoot["sourcesToBeEncoded"];
		json sourceToBeEncodedRoot = sourcesToBeEncodedRoot[0];
		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		int64_t durationInMilliSeconds = JSONUtils::asInt64(sourceToBeEncodedRoot, "sourceDurationInMilliSecs", -1);
		MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(JSONUtils::asString(encodingParametersRoot, "contentType", ""));
		int64_t physicalPathKey = JSONUtils::asInt64(sourceToBeEncodedRoot, "sourcePhysicalPathKey", -1);

		json videoTracksRoot;
		string field = "videoTracks";
		if (JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			videoTracksRoot = sourceToBeEncodedRoot[field];
		json audioTracksRoot;
		field = "audioTracks";
		if (JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			audioTracksRoot = sourceToBeEncodedRoot[field];

		field = "sourceFileExtension";
		if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileExtension = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

		bool useOfLocalStorageForProcessingOutput = true;

		if (externalEncoder)
		{
			field = "sourceTranscoderStagingAssetPathName";
			if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

			{
				size_t endOfDirectoryIndex = sourceAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = sourceAssetPathName.substr(0, endOfDirectoryIndex);

					SPDLOG_INFO(
						"Creating directory"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", directoryPathName: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, directoryPathName
					);
					fs::create_directories(directoryPathName);
					fs::permissions(
						directoryPathName,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
							fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace
					);
				}
			}

			field = "sourcePhysicalDeliveryURL";
			if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sourcePhysicalDeliveryURL = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

			field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

			sourceAssetPathName = downloadMediaFromMMS(
				_encoding->_ingestionJobKey, _encoding->_encodingJobKey, _encoding->_ffmpeg, sourceFileExtension, sourcePhysicalDeliveryURL, sourceAssetPathName
			);
		}
		else
		{
			field = "mmsSourceAssetPathName";
			if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

			if (useOfLocalStorageForProcessingOutput)
				field = "encodedTranscoderStagingAssetPathName";
			else
				field = "encodedNFSStagingAssetPathName";
			if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");
		}

		SPDLOG_INFO(
			"encoding content..."
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", sourceAssetPathName: {}",
			_encoding->_ingestionJobKey, _encoding->_encodingJobKey, sourceAssetPathName
		);

		_encoding->_encodingStart = chrono::system_clock::now();
		_encoding->_ffmpeg->encodeContent(
			sourceAssetPathName, durationInMilliSeconds, encodedStagingAssetPathName, encodingProfileDetailsRoot,
			contentType == MMSEngineDBFacade::ContentType::Video, videoTracksRoot, audioTracksRoot, videoTrackIndexToBeUsed, audioTrackIndexToBeUsed,
			filtersRoot, physicalPathKey, _encoding->_encodingJobKey, _encoding->_ingestionJobKey,
			_encoding->_childProcessId, _encoding->_callbackData
		);

		_encoding->_ffmpegTerminatedSuccessful = true;

		SPDLOG_INFO(
			"encoded content"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", sourceAssetPathName: {}"
			", encodedStagingAssetPathName: {}",
			_encoding->_ingestionJobKey, _encoding->_encodingJobKey, sourceAssetPathName, encodedStagingAssetPathName
		);

		if (externalEncoder)
		{
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", sourceAssetPathName: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, sourceAssetPathName
				);
				fs::remove_all(sourceAssetPathName);
			}

			field = "sourceMediaItemKey";
			if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			int64_t sourceMediaItemKey = JSONUtils::asInt64(sourceToBeEncodedRoot, field, -1);

			field = "encodingProfileKey";
			if (!JSONUtils::isPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot, field, -1);

			string workflowLabel =
				"Add Variant " + to_string(sourceMediaItemKey) + " - " + to_string(encodingProfileKey) + " (encoding from external transcoder)";
			uploadLocalMediaToMMS(
				_encoding->_ingestionJobKey, _encoding->_encodingJobKey, ingestedParametersRoot, encodingProfileDetailsRoot, encodingParametersRoot,
				sourceFileExtension, encodedStagingAssetPathName, workflowLabel,
				"External Transcoder", // ingester
				encodingProfileKey, sourceMediaItemKey
			);
		}
		else
		{
			if (useOfLocalStorageForProcessingOutput)
			{
				field = "encodedNFSStagingAssetPathName";
				if (!JSONUtils::isPresent(sourceToBeEncodedRoot, field))
				{
					string errorMessage = std::format(
						"Field is not present or it is null"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", Field: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string encodedNFSStagingAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

				// move encodedStagingAssetPathName (encodedTranscoderStagingAssetPathName) in encodedNFSStagingAssetPathName
				SPDLOG_INFO(
					"moving file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", encodedStagingAssetPathName: {}"
					", encodedNFSStagingAssetPathName: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, encodedStagingAssetPathName, encodedNFSStagingAssetPathName
				);
				int64_t moveElapsedInSeconds = MMSStorage::move(_encoding->_ingestionJobKey, encodedStagingAssetPathName, encodedNFSStagingAssetPathName);
				SPDLOG_INFO(
					"moved file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", encodedStagingAssetPathName: {}"
					", encodedNFSStagingAssetPathName: {}"
					", moveElapsedInSeconds: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, encodedStagingAssetPathName, encodedNFSStagingAssetPathName, moveElapsedInSeconds
				);
			}
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && fs::exists(sourceAssetPathName))
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", sourceAssetPathName: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, sourceAssetPathName
				);
				fs::remove_all(sourceAssetPathName);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					SPDLOG_INFO(
						"removeDirectory"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", directoryPathName: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, directoryPathName
					);
					fs::remove_all(directoryPathName);
				}
			}
		}

		string eWhat = e.what();
		SPDLOG_ERROR(
			"{} API failed (EncodingKilledByUser)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);

		// used by FFMPEGEncoderTask
		_killedByUser = true;

		throw e;
	}
	catch (runtime_error &e)
	{
		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && fs::exists(sourceAssetPathName))
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", sourceAssetPathName: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, sourceAssetPathName
				);
				fs::remove_all(sourceAssetPathName);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					SPDLOG_INFO(
						"removeDirectory"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", directoryPathName: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, directoryPathName
					);
					fs::remove_all(directoryPathName);
				}
			}
		}

		string eWhat = e.what();
		SPDLOG_ERROR(
			"{} API failed (runtime_error)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);

		// used by FFMPEGEncoderTask
		_encoding->_callbackData->pushErrorMessage(e.what());
		_completedWithError = true;

		throw e;
	}
	catch (exception &e)
	{
		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && fs::exists(sourceAssetPathName))
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", sourceAssetPathName: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, sourceAssetPathName
				);
				fs::remove_all(sourceAssetPathName);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					SPDLOG_INFO(
						"removeDirectory"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", directoryPathName: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, directoryPathName
					);
					fs::remove_all(directoryPathName);
				}
			}
		}

		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (exception)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
}
