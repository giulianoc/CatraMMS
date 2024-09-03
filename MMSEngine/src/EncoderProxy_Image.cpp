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
#include "JSONUtils.h"
#include <tuple>

void EncoderProxy::encodeContentImage()
{
	int64_t encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, "encodingProfileKey", 0);

	json sourcesToBeEncodedRoot = _encodingItem->_encodingParametersRoot["sourcesToBeEncoded"];

	for (int sourceIndex = 0; sourceIndex < sourcesToBeEncodedRoot.size(); sourceIndex++)
	{
		json sourceToBeEncodedRoot = sourcesToBeEncodedRoot[sourceIndex];

		bool stopIfReferenceProcessingError = JSONUtils::asBool(sourceToBeEncodedRoot, "stopIfReferenceProcessingError", false);

		string stagingEncodedAssetPathName;

		try
		{
			string sourceFileName = JSONUtils::asString(sourceToBeEncodedRoot, "sourceFileName", "");
			string sourceRelativePath = JSONUtils::asString(sourceToBeEncodedRoot, "sourceRelativePath", "");
			string sourceFileExtension = JSONUtils::asString(sourceToBeEncodedRoot, "sourceFileExtension", "");
			string mmsSourceAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, "mmsSourceAssetPathName", "");
			json encodingProfileDetailsRoot = _encodingItem->_encodingParametersRoot["encodingProfileDetails"];

			string encodedFileName;
			{
				size_t extensionIndex = sourceFileName.find_last_of(".");
				if (extensionIndex == string::npos)
				{
					string errorMessage = __FILEREF__ + "No extension find in the asset file name" + ", sourceFileName: " + sourceFileName;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				encodedFileName = sourceFileName.substr(0, extensionIndex) + "_" + to_string(encodingProfileKey);
			}

			// added the check of the file size is zero because in this case
			// the magick library cause the crash of the xmms engine
			{
				unsigned long ulFileSize = fs::file_size(mmsSourceAssetPathName);
				if (ulFileSize == 0)
				{
					string errorMessage = fmt::format(
						"source image file size is zero"
						", mmsSourceAssetPathName: {}",
						mmsSourceAssetPathName
					);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			auto [newImageFormat, newWidth, newHeight, newAspectRatio, newMaxWidth, newMaxHeight, newInterlaceType] =
				readingImageProfile(encodingProfileDetailsRoot);

			Magick::Image imageToEncode;

			imageToEncode.read(mmsSourceAssetPathName.c_str());

			string currentImageFormat = imageToEncode.magick();

			if (currentImageFormat == "jpeg")
				currentImageFormat = "JPG";

			int currentWidth = imageToEncode.columns();
			int currentHeight = imageToEncode.rows();

			SPDLOG_INFO(
				"Image processing"
				", encodingProfileKey: {}"
				", mmsSourceAssetPathName: {}"
				", currentImageFormat: {}"
				", currentWidth: {}"
				", currentHeight: {}"
				", newImageFormat: {}"
				", newWidth: {}"
				", newHeight: {}"
				", newAspectRatio: {}",
				encodingProfileKey, mmsSourceAssetPathName, currentImageFormat, currentWidth, currentHeight, newImageFormat, newWidth, newHeight,
				newAspectRatio
			);

			if (currentImageFormat == newImageFormat && currentWidth == newWidth && currentHeight == newHeight)
			{
				// same as the ingested content. Just copy the content

				encodedFileName.append(sourceFileExtension);

				bool removeLinuxPathIfExist = true;
				bool neededForTranscoder = false;
				stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder, _encodingItem->_workspace->_directoryName, to_string(_encodingItem->_encodingJobKey), sourceRelativePath,
					encodedFileName,
					-1, // _encodingItem->_mediaItemKey, not used because
						// encodedFileName is not ""
					-1, // _encodingItem->_physicalPathKey, not used because
						// encodedFileName is not ""
					removeLinuxPathIfExist
				);

				fs::copy(mmsSourceAssetPathName, stagingEncodedAssetPathName);
			}
			else
			{
				if (newImageFormat == "JPG")
				{
					imageToEncode.magick("JPEG");
					encodedFileName.append(".jpg");
				}
				else if (newImageFormat == "GIF")
				{
					imageToEncode.magick("GIF");
					encodedFileName.append(".gif");
				}
				else if (newImageFormat == "PNG")
				{
					imageToEncode.magick("PNG");
					imageToEncode.depth(8);
					encodedFileName.append(".png");
				}

				bool removeLinuxPathIfExist = true;
				bool neededForTranscoder = false;
				stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder, _encodingItem->_workspace->_directoryName, to_string(_encodingItem->_encodingJobKey),
					"/", // encodingItem->_encodeData->_relativePath,
					encodedFileName,
					-1, // _encodingItem->_mediaItemKey, not used because
						// encodedFileName is not ""
					-1, // _encodingItem->_physicalPathKey, not used because
						// encodedFileName is not ""
					removeLinuxPathIfExist
				);

				Magick::Geometry newGeometry(newWidth, newHeight);

				// if Aspect is true the proportion are not mantained
				// if Aspect is false the proportion are mantained
				newGeometry.aspect(newAspectRatio);

				// if ulAspect is false, it means the aspect is preserved,
				// the width is fixed and the height will be calculated or viceversa

				// imageToEncode_2 create a new Image in memory but the image data is not copied
				// (see https://www.imagemagick.org/Magick++/Image++.html)
				Magick::Image imageToEncode_2 = imageToEncode;

				// also 'scale' could be used
				imageToEncode.scale(newGeometry);
				imageToEncode.interlaceType(newInterlaceType);

				imageToEncode.write(stagingEncodedAssetPathName);

				// 2024-09-03: se height è calcolata e abbiamo un max height che viene superato, ridimensioniamo in base al max height
				if (!newAspectRatio) // aspect is preserved
				{
					if (newWidth != 0 && newHeight == 0 && newMaxHeight > 0) // height is calculated and we have a max height
					{
						// Magick::Image newImageToEncode;

						// newImageToEncode.read(stagingEncodedAssetPathName.c_str());

						// rows(): height
						if (imageToEncode.rows() > newMaxHeight)
						{
							Magick::Geometry newGeometry_2(0, newMaxHeight);

							newGeometry_2.aspect(newAspectRatio);

							// also 'scale' could be used
							imageToEncode_2.scale(newGeometry_2);
							imageToEncode_2.interlaceType(newInterlaceType);

							imageToEncode_2.write(stagingEncodedAssetPathName);
						}
					}
					else if (newWidth == 0 && newHeight != 0 && newMaxWidth > 0) // width is calculated and we have a max width
					{
						// Magick::Image newImageToEncode;

						// newImageToEncode.read(stagingEncodedAssetPathName.c_str());

						// columns(): width
						if (imageToEncode.columns() > newMaxWidth)
						{
							Magick::Geometry newGeometry_2(newMaxWidth, 0);

							newGeometry_2.aspect(newAspectRatio);

							// also 'scale' could be used
							imageToEncode_2.scale(newGeometry_2);
							imageToEncode_2.interlaceType(newInterlaceType);

							imageToEncode_2.write(stagingEncodedAssetPathName);
						}
					}
				}
			}

			sourceToBeEncodedRoot["out_stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
			sourcesToBeEncodedRoot[sourceIndex] = sourceToBeEncodedRoot;
			_encodingItem->_encodingParametersRoot["sourcesToBeEncoded"] = sourcesToBeEncodedRoot;
		}
		catch (Magick::Error &e)
		{
			SPDLOG_INFO(
				"ImageMagick exception"
				", e.what(): {}"
				", encodingJobKey: {}"
				", ingestionJobKey: {}",
				e.what(), _encodingItem->_encodingJobKey, _encodingItem->_ingestionJobKey
			);

			if (sourcesToBeEncodedRoot.size() == 1 || stopIfReferenceProcessingError)
			{
				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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

				throw runtime_error(e.what());
			}
		}
		catch (exception e)
		{
			_logger->info(
				__FILEREF__ + "ImageMagick exception" + ", e.what(): " + e.what() + ", encodingItem->_encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			);

			if (sourcesToBeEncodedRoot.size() == 1 || stopIfReferenceProcessingError)
			{
				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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
		}
	}
}

void EncoderProxy::processEncodedImage()
{
	int64_t encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, "encodingProfileKey", 0);

	int64_t physicalItemRetentionInMinutes = -1;
	{
		string field = "physicalItemRetention";
		if (JSONUtils::isMetadataPresent(_encodingItem->_ingestedParametersRoot, field))
		{
			string retention = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, field, "1d");
			physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
		}
	}

	json sourcesToBeEncodedRoot = _encodingItem->_encodingParametersRoot["sourcesToBeEncoded"];

	for (int sourceIndex = 0; sourceIndex < sourcesToBeEncodedRoot.size(); sourceIndex++)
	{
		json sourceToBeEncodedRoot = sourcesToBeEncodedRoot[sourceIndex];

		// bool stopIfReferenceProcessingError =
		// JSONUtils::asBool(sourceToBeEncodedRoot,
		// 	"stopIfReferenceProcessingError", false);

		try
		{
			string stagingEncodedAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, "out_stagingEncodedAssetPathName", "");

			string sourceRelativePath = JSONUtils::asString(sourceToBeEncodedRoot, "sourceRelativePath", "");

			int64_t sourceMediaItemKey = JSONUtils::asInt64(sourceToBeEncodedRoot, "sourceMediaItemKey", 0);

			if (stagingEncodedAssetPathName == "")
				continue;

			tuple<int64_t, long, json> mediaInfoDetails;
			vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
			vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
			/*
			int64_t durationInMilliSeconds = -1;
			long bitRate = -1;
			string videoCodecName;
			string videoProfile;
			int videoWidth = -1;
			int videoHeight = -1;
			string videoAvgFrameRate;
			long videoBitRate = -1;
			string audioCodecName;
			long audioSampleRate = -1;
			int audioChannels = -1;
			long audioBitRate = -1;
			*/

			int imageWidth = -1;
			int imageHeight = -1;
			string imageFormat;
			int imageQuality = -1;
			try
			{
				_logger->info(
					__FILEREF__ + "Processing through Magick" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);
				Magick::Image imageToEncode;

				imageToEncode.read(stagingEncodedAssetPathName.c_str());

				imageWidth = imageToEncode.columns();
				imageHeight = imageToEncode.rows();
				imageFormat = imageToEncode.magick();
				imageQuality = imageToEncode.quality();
			}
			catch (Magick::WarningCoder &e)
			{
				// Process coder warning while loading file (e.g. TIFF warning)
				// Maybe the user will be interested in these warnings (or not).
				// If a warning is produced while loading an image, the image
				// can normally still be used (but not if the warning was about
				// something important!)
				_logger->error(
					__FILEREF__ + "ImageMagick failed to retrieve width and height" + ", encodingItem->_encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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

				throw runtime_error(e.what());
			}
			catch (Magick::Warning &e)
			{
				_logger->error(
					__FILEREF__ + "ImageMagick failed to retrieve width and height" + ", encodingItem->_encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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

				throw runtime_error(e.what());
			}
			catch (Magick::ErrorFileOpen &e)
			{
				_logger->error(
					__FILEREF__ + "ImageMagick failed to retrieve width and height" + ", encodingItem->_encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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

				throw runtime_error(e.what());
			}
			catch (Magick::Error &e)
			{
				_logger->error(
					__FILEREF__ + "ImageMagick failed to retrieve width and height" + ", encodingItem->_encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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

				throw runtime_error(e.what());
			}
			catch (exception &e)
			{
				_logger->error(
					__FILEREF__ + "ImageMagick failed to retrieve width and height" + ", encodingItem->_encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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

			string encodedFileName;
			string mmsAssetPathName;
			unsigned long mmsPartitionIndexUsed;
			try
			{
				size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (fileNameIndex == string::npos)
				{
					string errorMessage =
						__FILEREF__ + "No fileName find in the asset path name" + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

				bool deliveryRepositoriesToo = true;

				mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
					_encodingItem->_ingestionJobKey, stagingEncodedAssetPathName, _encodingItem->_workspace->_directoryName, encodedFileName,
					sourceRelativePath,

					&mmsPartitionIndexUsed, // OUT
					// &sourceFileType,

					deliveryRepositoriesToo, _encodingItem->_workspace->_territories
				);
			}
			catch (runtime_error &e)
			{
				_logger->error(
					__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed" + ", encodingItem->_encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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
					__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed" + ", encodingItem->_encodingJobKey: " +
					to_string(_encodingItem->_encodingJobKey) + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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
					size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "removeDirectory" + ", directoryPathName: " + directoryPathName);
						fs::remove_all(directoryPathName);
					}
				}
				catch (runtime_error &e)
				{
					_logger->error(
						__FILEREF__ + "removeDirectory failed" + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
						", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", stagingEncodedAssetPathName: " +
						stagingEncodedAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
					);
				}
			}

			try
			{
				unsigned long long mmsAssetSizeInBytes;
				{
					mmsAssetSizeInBytes = fs::file_size(mmsAssetPathName);
				}

				bool externalReadOnlyStorage = false;
				string externalDeliveryTechnology;
				string externalDeliveryURL;
				int64_t liveRecordingIngestionJobKey = -1;
				int64_t encodedPhysicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
					_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, liveRecordingIngestionJobKey, sourceMediaItemKey,
					externalReadOnlyStorage, externalDeliveryTechnology, externalDeliveryURL, encodedFileName, sourceRelativePath,
					mmsPartitionIndexUsed, mmsAssetSizeInBytes, encodingProfileKey, physicalItemRetentionInMinutes,

					mediaInfoDetails, videoTracks, audioTracks,
					/*
					durationInMilliSeconds,
					bitRate,
					videoCodecName,
					videoProfile,
					videoWidth,
					videoHeight,
					videoAvgFrameRate,
					videoBitRate,
					audioCodecName,
					audioSampleRate,
					audioChannels,
					audioBitRate,
					*/

					imageWidth, imageHeight, imageFormat, imageQuality
				);

				sourceToBeEncodedRoot["out_encodedPhysicalPathKey"] = encodedPhysicalPathKey;
				sourcesToBeEncodedRoot[sourceIndex] = sourceToBeEncodedRoot;
				_encodingItem->_encodingParametersRoot["sourcesToBeEncoded"] = sourcesToBeEncodedRoot;

				_logger->info(
					__FILEREF__ + "Saved the Encoded content" + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
				);
			}
			catch (exception &e)
			{
				_logger->error(
					__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed" + ", encodingItem->_ingestionJobKey: " +
					to_string(_encodingItem->_ingestionJobKey) + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);

				_logger->info(__FILEREF__ + "Remove" + ", mmsAssetPathName: " + mmsAssetPathName);
				fs::remove_all(mmsAssetPathName);

				throw e;
			}
		}
		catch (runtime_error &e)
		{
			_logger->error(
				__FILEREF__ + "process media input failed" + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", exception: " + e.what()
			);

			if (sourcesToBeEncodedRoot.size() == 1)
				throw e;
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "process media input failed" + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", exception: " + e.what()
			);

			if (sourcesToBeEncodedRoot.size() == 1)
				throw e;
		}
	}
}

tuple<string, int, int, bool, int, int, Magick::InterlaceType> EncoderProxy::readingImageProfile(json encodingProfileRoot)
{
	string newFormat;
	int newWidth;
	int newHeight;
	bool newAspectRatio;
	int newMaxWidth;
	int newMaxHeight;
	Magick::InterlaceType newInterlaceType;

	// FileFormat
	{
		newFormat = JSONUtils::asString(encodingProfileRoot, "fileFormat", "", true);
		encodingImageFormatValidation(newFormat);
	}

	json encodingProfileImageRoot;
	{
		if (!JSONUtils::isMetadataPresent(encodingProfileRoot, "Image"))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: Image";
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		encodingProfileImageRoot = encodingProfileRoot["Image"];
	}

	newWidth = JSONUtils::asInt(encodingProfileImageRoot, "width", 0, true);
	newHeight = JSONUtils::asInt(encodingProfileImageRoot, "height", 0, true);
	newAspectRatio = JSONUtils::asBool(encodingProfileImageRoot, "aspectRatio", false, true);
	newMaxWidth = JSONUtils::asInt(encodingProfileImageRoot, "maxWidth", 0, false);
	newMaxHeight = JSONUtils::asInt(encodingProfileImageRoot, "maxHeight", 0, false);

	// Interlace
	{
		string sNewInterlaceType = JSONUtils::asString(encodingProfileImageRoot, "interlaceType", "", true);
		newInterlaceType = encodingImageInterlaceTypeValidation(sNewInterlaceType);
	}

	return make_tuple(newFormat, newWidth, newHeight, newAspectRatio, newMaxWidth, newMaxHeight, newInterlaceType);
}
