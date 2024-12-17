
#include "SlideShow.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/DateTime.h"
#include "spdlog/spdlog.h"

void SlideShow::encodeContent(json metadataRoot)
{
	string api = "slideShow";

	_logger->info(
		__FILEREF__ + "Received " + api + ", _ingestionJobKey: " + to_string(_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingJobKey)
		// + ", requestBody: " + requestBody already logged
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
						string errorMessage = __FILEREF__ + "Field is not present or it is null" +
											  ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceFileExtension = JSONUtils::asString(imageRoot, field, "");

					field = "sourcePhysicalDeliveryURL";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" +
											  ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourcePhysicalDeliveryURL = JSONUtils::asString(imageRoot, field, "");

					field = "sourceTranscoderStagingAssetPathName";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" +
											  ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceTranscoderStagingAssetPathName = JSONUtils::asString(imageRoot, field, "");

					{
						size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							string directoryPathName = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);

							_logger->info(
								__FILEREF__ + "Creating directory" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingJobKey) + ", directoryPathName: " + directoryPathName
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
						string errorMessage = __FILEREF__ + "Field is not present or it is null" +
											  ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceFileExtension = JSONUtils::asString(audioRoot, field, "");

					field = "sourcePhysicalDeliveryURL";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" +
											  ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourcePhysicalDeliveryURL = JSONUtils::asString(audioRoot, field, "");

					field = "sourceTranscoderStagingAssetPathName";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" +
											  ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
											  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceTranscoderStagingAssetPathName = JSONUtils::asString(audioRoot, field, "");

					{
						size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							string directoryPathName = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);

							_logger->info(
								__FILEREF__ + "Creating directory" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
								", _encodingJobKey: " + to_string(_encodingJobKey) + ", directoryPathName: " + directoryPathName
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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(
						__FILEREF__ + "Creating directory" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingJobKey) + ", directoryPathName: " + directoryPathName
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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetailsRoot"];

		_encoding->_ffmpeg->slideShow(
			_ingestionJobKey, _encodingJobKey, durationOfEachSlideInSeconds, frameRateMode, encodingProfileDetailsRoot, imagesPathNames,
			audiosPathNames, shortestAudioDurationInSeconds, encodedStagingAssetPathName, &(_encoding->_childPid)
		);

		_encoding->_ffmpegTerminatedSuccessful = true;

		_logger->info(
			__FILEREF__ + "slideShow finished" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingJobKey)
		);

		if (externalEncoder)
		{
			for (string imagePathName : imagesPathNames)
			{
				_logger->info(
					__FILEREF__ + "Remove file" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingJobKey) + ", imagePathName: " + imagePathName
				);

				fs::remove_all(imagePathName);
			}

			for (string audioPathName : audiosPathNames)
			{
				_logger->info(
					__FILEREF__ + "Remove file" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingJobKey) + ", audioPathName: " + audioPathName
				);

				fs::remove_all(audioPathName);
			}

			field = "targetFileFormat";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

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
		string errorMessage = fmt::format(
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
		_encoding->_errorMessage = errorMessage;
		_completedWithError = true;

		throw e;
	}
	catch (exception &e)
	{
		string eWhat = e.what();
		string errorMessage = fmt::format(
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
		_encoding->_errorMessage = errorMessage;
		_completedWithError = true;

		throw e;
	}
}
