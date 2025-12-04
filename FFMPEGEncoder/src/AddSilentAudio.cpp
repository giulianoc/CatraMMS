
#include "AddSilentAudio.h"

#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

void AddSilentAudio::encodeContent(json metadataRoot)
{
	string api = "addSilentAudio";

	SPDLOG_INFO(
		"Received {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", requestBody: {}",
		api, _encoding->_ingestionJobKey, _encoding->_encodingJobKey, JSONUtils::toString(metadataRoot)
	);

	try
	{
		// json metadataRoot = JSONUtils::toJson(
		// 	-1, _encodingJobKey, requestBody);

		// int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);
		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);
		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];

		string addType = JSONUtils::asString(ingestedParametersRoot, "addType", "entireTrack");
		int silentDurationInSeconds = JSONUtils::asInt(ingestedParametersRoot, "silentDurationInSeconds", 1);

		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		json sourcesRoot = encodingParametersRoot["sources"];

		for (int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
		{
			const json& sourceRoot = sourcesRoot[sourceIndex];

			bool stopIfReferenceProcessingError = JSONUtils::asBool(sourceRoot, "stopIfReferenceProcessingError", false);
			int64_t sourceDurationInMilliSeconds = JSONUtils::asInt64(sourceRoot, "sourceDurationInMilliSeconds", 0);
			string sourceFileExtension = JSONUtils::asString(sourceRoot, "sourceFileExtension", "");

			string sourceAssetPathName;
			string encodedStagingAssetPathName;

			_encoding->_ffmpegTerminatedSuccessful = false;

			if (externalEncoder)
			{
				sourceAssetPathName = JSONUtils::asString(sourceRoot, "sourceTranscoderStagingAssetPathName", "");

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

				encodedStagingAssetPathName = JSONUtils::asString(sourceRoot, "encodedTranscoderStagingAssetPathName", "");

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

				string sourcePhysicalDeliveryURL = JSONUtils::asString(sourceRoot, "sourcePhysicalDeliveryURL", "");

				sourceAssetPathName = downloadMediaFromMMS(
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, _encoding->_ffmpeg, sourceFileExtension, sourcePhysicalDeliveryURL, sourceAssetPathName
				);
			}
			else
			{
				sourceAssetPathName = JSONUtils::asString(sourceRoot, "sourceAssetPathName", "");
				encodedStagingAssetPathName = JSONUtils::asString(sourceRoot, "encodedNFSStagingAssetPathName", "");
			}

			try
			{
				_encoding->_encodingStart = chrono::system_clock::now();
				_encoding->_ffmpeg->silentAudio(
					sourceAssetPathName, sourceDurationInMilliSeconds,

					addType, silentDurationInSeconds,

					encodingProfileDetailsRoot,

					encodedStagingAssetPathName, _encoding->_encodingJobKey, _encoding->_ingestionJobKey,
					_encoding->_childProcessId, nullptr
				);

				_encoding->_ffmpegTerminatedSuccessful = true;

				SPDLOG_INFO(
					"Encode content finished"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", encodedStagingAssetPathName: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, encodedStagingAssetPathName
				);
			}
			catch (FFMpegEncodingKilledByUser &e)
			{
				throw e;
			}
			catch (runtime_error &e)
			{
				if (stopIfReferenceProcessingError || sourceIndex + 1 == sourcesRoot.size())
					throw e;
				else
				{
					SPDLOG_INFO(
						"ffmpeg failed but we will continue with the next one"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", stopIfReferenceProcessingError: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, stopIfReferenceProcessingError
					);

					continue;
				}
			}
			catch (exception &e)
			{
				if (stopIfReferenceProcessingError || sourceIndex + 1 == sourcesRoot.size())
					throw e;
				else
				{
					SPDLOG_INFO(
						"ffmpeg failed but we will continue with the next one"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", stopIfReferenceProcessingError: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, stopIfReferenceProcessingError
					);

					continue;
				}
			}

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

				string workflowLabel = JSONUtils::asString(ingestedParametersRoot, "title", "") + " (add " + api + " from external transcoder)";

				int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot, "encodingProfileKey", -1);

				uploadLocalMediaToMMS(
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, ingestedParametersRoot, encodingProfileDetailsRoot,
					encodingParametersRoot, sourceFileExtension, encodedStagingAssetPathName, workflowLabel,
					"External Transcoder", // ingester
					encodingProfileKey
				);
			}
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
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
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
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_callbackData->pushErrorMessage(errorMessage);
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
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
}
