
#include "PictureInPicture.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/DateTime.h"
#include "spdlog/spdlog.h"

void PictureInPicture::encodeContent(json metadataRoot)
{
	string api = "pictureInPicture";

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

		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		string mainSourceFileExtension;
		{
			string field = "mainSourceFileExtension";
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
			mainSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string overlaySourceFileExtension;
		{
			string field = "overlaySourceFileExtension";
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
			overlaySourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string mainSourceAssetPathName;
		string overlaySourceAssetPathName;
		string encodedStagingAssetPathName;

		if (externalEncoder)
		{
			{
				string field = "mainSourceTranscoderStagingAssetPathName";
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
				mainSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = mainSourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = mainSourceAssetPathName.substr(0, endOfDirectoryIndex);

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

				field = "mainSourcePhysicalDeliveryURL";
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
				string mainSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				mainSourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey, _encodingJobKey, _encoding->_ffmpeg, mainSourceFileExtension, mainSourcePhysicalDeliveryURL,
					mainSourceAssetPathName
				);
			}

			{
				string field = "overlaySourceTranscoderStagingAssetPathName";
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
				overlaySourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = overlaySourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = overlaySourceAssetPathName.substr(0, endOfDirectoryIndex);

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

				field = "overlaySourcePhysicalDeliveryURL";
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
				string overlaySourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				overlaySourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey, _encodingJobKey, _encoding->_ffmpeg, overlaySourceFileExtension, overlaySourcePhysicalDeliveryURL,
					overlaySourceAssetPathName
				);
			}

			string field = "encodedTranscoderStagingAssetPathName";
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
		}
		else
		{
			string field = "mainSourceAssetPathName";
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
			mainSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "overlaySourceAssetPathName";
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
			overlaySourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

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
		}

		int64_t mainSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot, "mainSourceDurationInMilliSeconds", 0);
		int64_t overlaySourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot, "overlaySourceDurationInMilliSeconds", 0);
		bool soundOfMain = JSONUtils::asBool(encodingParametersRoot, "soundOfMain", false);

		string overlayPosition_X_InPixel = JSONUtils::asString(ingestedParametersRoot, "overlayPosition_X_InPixel", "0");
		string overlayPosition_Y_InPixel = JSONUtils::asString(ingestedParametersRoot, "overlayPosition_Y_InPixel", "0");
		string overlay_Width_InPixel = JSONUtils::asString(ingestedParametersRoot, "overlay_Width_InPixel", "100");
		string overlay_Height_InPixel = JSONUtils::asString(ingestedParametersRoot, "overlay_Height_InPixel", "100");

		_encoding->_ffmpeg->pictureInPicture(
			mainSourceAssetPathName, mainSourceDurationInMilliSeconds,

			overlaySourceAssetPathName, overlaySourceDurationInMilliSeconds,

			soundOfMain,

			overlayPosition_X_InPixel, overlayPosition_Y_InPixel, overlay_Width_InPixel, overlay_Height_InPixel,

			encodingProfileDetailsRoot,

			encodedStagingAssetPathName, _encodingJobKey, _ingestionJobKey, &(_encoding->_childPid)
		);

		_encoding->_ffmpegTerminatedSuccessful = true;

		SPDLOG_INFO(
			"PictureInPicture encoding content finished"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", encodedStagingAssetPathName: {}",
			_ingestionJobKey, _encodingJobKey, encodedStagingAssetPathName
		);

		if (externalEncoder)
		{
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", mainSourceAssetPathName: {}",
					_ingestionJobKey, _encodingJobKey, mainSourceAssetPathName
				);
				fs::remove_all(mainSourceAssetPathName);
			}

			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", overlaySourceAssetPathName: {}",
					_ingestionJobKey, _encodingJobKey, overlaySourceAssetPathName
				);
				fs::remove_all(overlaySourceAssetPathName);
			}

			string workflowLabel = JSONUtils::asString(ingestedParametersRoot, "title", "") + " (add pictureInPicture from external transcoder)";

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot, "encodingProfileKey", -1);

			uploadLocalMediaToMMS(
				_ingestionJobKey, _encodingJobKey, ingestedParametersRoot, encodingProfileDetailsRoot, encodingParametersRoot,
				mainSourceFileExtension, encodedStagingAssetPathName, workflowLabel,
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
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
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
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
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
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
}
