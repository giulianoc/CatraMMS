
#include "SlideShow.h"

#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

void SlideShow::encodeContent(json metadataRoot)
{
	string api = "slideShow";

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

		float durationOfEachSlideInSeconds = 2.0;
		string field = "durationOfEachSlideInSeconds";
		durationOfEachSlideInSeconds = JSONUtils::asDouble(ingestedParametersRoot, field, 2.0);

		float shortestAudioDurationInSeconds = -1.0;
		field = "shortestAudioDurationInSeconds";
		shortestAudioDurationInSeconds = JSONUtils::asDouble(encodingParametersRoot, field, -1.0);

		string frameRateMode = "vfr";
		field = "frameRateMode";
		frameRateMode = JSONUtils::asString(ingestedParametersRoot, field, "vfr");

		vector<string> imagesPathNames;
		{
			json imagesRoot = encodingParametersRoot["imagesRoot"];
			for (int index = 0; index < imagesRoot.size(); index++)
			{
				json imageRoot = imagesRoot[index];

				if (externalEncoder)
				{
					field = "sourceFileExtension";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
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
					string sourceFileExtension = JSONUtils::asString(imageRoot, field, "");

					field = "sourcePhysicalDeliveryURL";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
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
					string sourcePhysicalDeliveryURL = JSONUtils::asString(imageRoot, field, "");

					field = "sourceTranscoderStagingAssetPathName";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
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
					string sourceTranscoderStagingAssetPathName = JSONUtils::asString(imageRoot, field, "");

					{
						size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							string directoryPathName = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);

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
								fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read |
									fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec,
								fs::perm_options::replace
							);
						}
					}

					imagesPathNames.push_back(downloadMediaFromMMS(
						_ingestionJobKey, _encodingJobKey, _encoding->_ffmpeg, sourceFileExtension, sourcePhysicalDeliveryURL,
						sourceTranscoderStagingAssetPathName
					));
				}
				else
				{
					imagesPathNames.push_back(JSONUtils::asString(imageRoot, "sourceAssetPathName", ""));
				}
			}
		}

		vector<string> audiosPathNames;
		{
			json audiosRoot = encodingParametersRoot["audiosRoot"];
			for (int index = 0; index < audiosRoot.size(); index++)
			{
				json audioRoot = audiosRoot[index];

				if (externalEncoder)
				{
					field = "sourceFileExtension";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
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
					string sourceFileExtension = JSONUtils::asString(audioRoot, field, "");

					field = "sourcePhysicalDeliveryURL";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
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
					string sourcePhysicalDeliveryURL = JSONUtils::asString(audioRoot, field, "");

					field = "sourceTranscoderStagingAssetPathName";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
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
					string sourceTranscoderStagingAssetPathName = JSONUtils::asString(audioRoot, field, "");

					{
						size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							string directoryPathName = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);

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
								fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read |
									fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec,
								fs::perm_options::replace
							);
						}
					}

					audiosPathNames.push_back(downloadMediaFromMMS(
						_ingestionJobKey, _encodingJobKey, _encoding->_ffmpeg, sourceFileExtension, sourcePhysicalDeliveryURL,
						sourceTranscoderStagingAssetPathName
					));
				}
				else
				{
					audiosPathNames.push_back(JSONUtils::asString(audioRoot, "sourceAssetPathName", ""));
				}
			}
		}

		string encodedStagingAssetPathName;
		if (externalEncoder)
		{
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
		}
		else
		{
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

		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetailsRoot"];

		_encoding->_ffmpeg->slideShow(
			_ingestionJobKey, _encodingJobKey, durationOfEachSlideInSeconds, frameRateMode, encodingProfileDetailsRoot, imagesPathNames,
			audiosPathNames, shortestAudioDurationInSeconds, encodedStagingAssetPathName, _encoding->_childProcessId
		);

		_encoding->_ffmpegTerminatedSuccessful = true;

		SPDLOG_INFO(
			"slideShow finished"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}",
			_ingestionJobKey, _encodingJobKey
		);

		if (externalEncoder)
		{
			for (string imagePathName : imagesPathNames)
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", imagePathName: {}",
					_ingestionJobKey, _encodingJobKey, imagePathName
				);
				fs::remove_all(imagePathName);
			}

			for (string audioPathName : audiosPathNames)
			{
				SPDLOG_INFO(
					"Remove file"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", audioPathName: {}",
					_ingestionJobKey, _encodingJobKey, audioPathName
				);
				fs::remove_all(audioPathName);
			}

			field = "targetFileFormat";
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
			string targetFileFormat = JSONUtils::asString(encodingParametersRoot, field, "");

			string workflowLabel = JSONUtils::asString(ingestedParametersRoot, "title", "") + " (add slideShow from external transcoder)";

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot, "encodingProfileKey", -1);

			uploadLocalMediaToMMS(
				_ingestionJobKey, _encodingJobKey, ingestedParametersRoot, encodingProfileDetailsRoot, encodingParametersRoot,
				targetFileFormat, // sourceFileExtension,
				encodedStagingAssetPathName, workflowLabel,
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
