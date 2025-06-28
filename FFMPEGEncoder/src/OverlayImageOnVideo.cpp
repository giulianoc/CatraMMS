
#include "OverlayImageOnVideo.h"

#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

void OverlayImageOnVideo::encodeContent(json metadataRoot)
{
	string api = "overlayImageOnVideo";

	SPDLOG_INFO(
		"Received {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", requestBody: {}",
		api, _ingestionJobKey, _encodingJobKey, JSONUtils::toString(metadataRoot)
	);

	try
	{
		// json metadataRoot = JSONUtils::toJson(
		// 	-1, _encodingJobKey, requestBody);

		// int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);
		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);
		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];

		string imagePosition_X_InPixel = JSONUtils::asString(ingestedParametersRoot, "imagePosition_X_InPixel", "0");
		string imagePosition_Y_InPixel = JSONUtils::asString(ingestedParametersRoot, "imagePosition_Y_InPixel", "0");

		int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot, "videoDurationInMilliSeconds", -1);

		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		string sourceVideoFileExtension;
		{
			string field = "sourceVideoFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceVideoFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string sourceVideoAssetPathName;
		string encodedStagingAssetPathName;
		string mmsSourceImageAssetPathName;

		if (externalEncoder)
		{
			string field = "sourceVideoTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceVideoAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			{
				size_t endOfDirectoryIndex = sourceVideoAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = sourceVideoAssetPathName.substr(0, endOfDirectoryIndex);

					SPDLOG_INFO(
						"Creating directory"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", directoryPathName: {}",
						_ingestionJobKey, _encodingJobKey, directoryPathName
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

			field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					SPDLOG_INFO(
						"Creating directory"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", directoryPathName: {}",
						_ingestionJobKey, _encodingJobKey, directoryPathName
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

			field = "sourceImagePhysicalDeliveryURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsSourceImageAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "sourceVideoPhysicalDeliveryURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sourceVideoPhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

			sourceVideoAssetPathName = downloadMediaFromMMS(
				_ingestionJobKey, _encodingJobKey, _encoding->_ffmpeg, sourceVideoFileExtension, sourceVideoPhysicalDeliveryURL,
				sourceVideoAssetPathName
			);
		}
		else
		{
			string field = "mmsSourceVideoAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceVideoAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "encodedNFSStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "mmsSourceImageAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_ingestionJobKey, _encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsSourceImageAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		_encoding->_ffmpeg->overlayImageOnVideo(
			externalEncoder, sourceVideoAssetPathName, videoDurationInMilliSeconds, mmsSourceImageAssetPathName, imagePosition_X_InPixel,
			imagePosition_Y_InPixel, encodedStagingAssetPathName, encodingProfileDetailsRoot, _encodingJobKey, _ingestionJobKey,
			&(_encoding->_childPid)
		);

		_encoding->_ffmpegTerminatedSuccessful = true;

		SPDLOG_INFO(
			"overlayImageOnVideo finished"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", sourceVideoAssetPathName: {}"
			", mmsSourceImageAssetPathName: {}"
			", encodedStagingAssetPathName: {}",
			_ingestionJobKey, _encodingJobKey, sourceVideoAssetPathName, mmsSourceImageAssetPathName, encodedStagingAssetPathName
		);

		if (externalEncoder)
		{
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", sourceVideoAssetPathName: {}",
					_ingestionJobKey, _encodingJobKey, sourceVideoAssetPathName
				);
				fs::remove_all(sourceVideoAssetPathName);
			}

			string workflowLabel = JSONUtils::asString(ingestedParametersRoot, "title", "") + " (add overlayImageOnVideo from external transcoder)";

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot, "encodingProfileKey", -1);

			uploadLocalMediaToMMS(
				_ingestionJobKey, _encodingJobKey, ingestedParametersRoot, encodingProfileDetailsRoot, encodingParametersRoot,
				sourceVideoFileExtension, encodedStagingAssetPathName, workflowLabel,
				"External Transcoder", // ingester
				encodingProfileKey
			);
		}
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		string eWhat = e.what();
		SPDLOG_ERROR(
			"{} API failed (EncodingKilledByUser)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);

		// used by FFMPEGEncoderTask
		_killedByUser = true;

		throw e;
	}
	catch (runtime_error &e)
	{
		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (runtime_error)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
	catch (exception &e)
	{
		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (exception)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
}
