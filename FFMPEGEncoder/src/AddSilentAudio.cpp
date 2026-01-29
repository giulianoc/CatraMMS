
#include "AddSilentAudio.h"

#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

void AddSilentAudio::encodeContent(json metadataRoot)
{
	string api = "addSilentAudio";

	LOG_INFO(
		"Received {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", requestBody: {}",
		api, _encoding->_ingestionJobKey, _encoding->_encodingJobKey, JSONUtils::toString(metadataRoot)
	);

	try
	{
		// json metadataRoot = JSONUtils::toJson<json>(
		// 	-1, _encodingJobKey, requestBody);

		// int64_t ingestionJobKey = JSONUtils::as<int64_t>(metadataRoot, "ingestionJobKey", -1);
		bool externalEncoder = JSONUtils::as<bool>(metadataRoot, "externalEncoder", false);
		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];

		string addType = JSONUtils::as<string>(ingestedParametersRoot, "addType", "entireTrack");
		int silentDurationInSeconds = JSONUtils::as<int32_t>(ingestedParametersRoot, "silentDurationInSeconds", 1);

		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		json sourcesRoot = encodingParametersRoot["sources"];

		for (int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
		{
			const json& sourceRoot = sourcesRoot[sourceIndex];

			bool stopIfReferenceProcessingError = JSONUtils::as<bool>(sourceRoot, "stopIfReferenceProcessingError", false);
			int64_t sourceDurationInMilliSeconds = JSONUtils::as<int64_t>(sourceRoot, "sourceDurationInMilliSeconds", 0);
			string sourceFileExtension = JSONUtils::as<string>(sourceRoot, "sourceFileExtension", "");

			string sourceAssetPathName;
			string encodedStagingAssetPathName;

			_encoding->_ffmpegTerminatedSuccessful = false;

			if (externalEncoder)
			{
				sourceAssetPathName = JSONUtils::as<string>(sourceRoot, "sourceTranscoderStagingAssetPathName", "");

				{
					size_t endOfDirectoryIndex = sourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = sourceAssetPathName.substr(0, endOfDirectoryIndex);

						LOG_INFO(
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

				encodedStagingAssetPathName = JSONUtils::as<string>(sourceRoot, "encodedTranscoderStagingAssetPathName", "");

				{
					size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

						LOG_INFO(
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

				string sourcePhysicalDeliveryURL = JSONUtils::as<string>(sourceRoot, "sourcePhysicalDeliveryURL", "");

				sourceAssetPathName = downloadMediaFromMMS(
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, _encoding->_ffmpeg, sourceFileExtension, sourcePhysicalDeliveryURL, sourceAssetPathName
				);
			}
			else
			{
				sourceAssetPathName = JSONUtils::as<string>(sourceRoot, "sourceAssetPathName", "");
				encodedStagingAssetPathName = JSONUtils::as<string>(sourceRoot, "encodedNFSStagingAssetPathName", "");
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

				LOG_INFO(
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
					LOG_INFO(
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
					LOG_INFO(
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
					LOG_INFO(
						"Remove file"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", sourceAssetPathName: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, sourceAssetPathName
					);
					fs::remove_all(sourceAssetPathName);
				}

				string workflowLabel = JSONUtils::as<string>(ingestedParametersRoot, "title", "") + " (add " + api + " from external transcoder)";

				int64_t encodingProfileKey = JSONUtils::as<int64_t>(encodingParametersRoot, "encodingProfileKey", -1);

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
		LOG_ERROR(
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
		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (runtime_error)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		LOG_ERROR(errorMessage);

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
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		LOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
}
