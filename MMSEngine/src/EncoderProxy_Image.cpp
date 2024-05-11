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
/*
#include "AWSSigner.h"
#include "FFMpeg.h"
#include "LocalAssetIngestionEvent.h"
#include "MMSCURL.h"
#include "MMSDeliveryAuthorization.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include "opencv2/face.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect.hpp"
#include <fstream>
#include <regex>

#include <aws/core/Aws.h>
#include <aws/medialive/MediaLiveClient.h>
#include <aws/medialive/model/DescribeChannelRequest.h>
#include <aws/medialive/model/DescribeChannelResult.h>
#include <aws/medialive/model/StartChannelRequest.h>
#include <aws/medialive/model/StopChannelRequest.h>
*/

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

			string newImageFormat;
			int newWidth;
			int newHeight;
			bool newAspectRatio;
			string sNewInterlaceType;
			Magick::InterlaceType newInterlaceType;
			{
				// added the check of the file size is zero because in this case
				// the magick library cause the crash of the xmms engine
				{
					unsigned long ulFileSize = fs::file_size(mmsSourceAssetPathName);
					if (ulFileSize == 0)
					{
						string errorMessage = __FILEREF__ + "source image file size is zero" + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				readingImageProfile(
					encodingProfileDetailsRoot, newImageFormat, newWidth, newHeight, newAspectRatio, sNewInterlaceType, newInterlaceType
				);
			}

			Magick::Image imageToEncode;

			imageToEncode.read(mmsSourceAssetPathName.c_str());

			string currentImageFormat = imageToEncode.magick();

			if (currentImageFormat == "jpeg")
				currentImageFormat = "JPG";

			int currentWidth = imageToEncode.columns();
			int currentHeight = imageToEncode.rows();

			_logger->info(
				__FILEREF__ + "Image processing" + ", encodingProfileKey: " + to_string(encodingProfileKey) + ", mmsSourceAssetPathName: " +
				mmsSourceAssetPathName + ", currentImageFormat: " + currentImageFormat + ", currentWidth: " + to_string(currentWidth) +
				", currentHeight: " + to_string(currentHeight) + ", newImageFormat: " + newImageFormat + ", newWidth: " + to_string(newWidth) +
				", newHeight: " + to_string(newHeight) + ", newAspectRatio: " + to_string(newAspectRatio) + ", sNewInterlace: " + sNewInterlaceType
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
				// the width is fixed and the height will be calculated

				// also 'scale' could be used
				imageToEncode.scale(newGeometry);
				imageToEncode.interlaceType(newInterlaceType);

				imageToEncode.write(stagingEncodedAssetPathName);
			}

			sourceToBeEncodedRoot["out_stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
			sourcesToBeEncodedRoot[sourceIndex] = sourceToBeEncodedRoot;
			_encodingItem->_encodingParametersRoot["sourcesToBeEncoded"] = sourcesToBeEncodedRoot;
		}
		catch (Magick::Error &e)
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

void EncoderProxy::readingImageProfile(
	json encodingProfileRoot, string &newFormat, int &newWidth, int &newHeight, bool &newAspectRatio, string &sNewInterlaceType,
	Magick::InterlaceType &newInterlaceType
)
{
	string field;

	// FileFormat
	{
		field = "fileFormat";
		if (!JSONUtils::isMetadataPresent(encodingProfileRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		newFormat = JSONUtils::asString(encodingProfileRoot, field, "");

		encodingImageFormatValidation(newFormat);
	}

	json encodingProfileImageRoot;
	{
		field = "Image";
		if (!JSONUtils::isMetadataPresent(encodingProfileRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		encodingProfileImageRoot = encodingProfileRoot[field];
	}

	// Width
	{
		field = "width";
		if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		newWidth = JSONUtils::asInt(encodingProfileImageRoot, field, 0);
	}

	// Height
	{
		field = "height";
		if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		newHeight = JSONUtils::asInt(encodingProfileImageRoot, field, 0);
	}

	// Aspect
	{
		field = "AspectRatio";
		if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		newAspectRatio = JSONUtils::asBool(encodingProfileImageRoot, field, false);
	}

	// Interlace
	{
		field = "InterlaceType";
		if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		sNewInterlaceType = JSONUtils::asString(encodingProfileImageRoot, field, "");

		newInterlaceType = encodingImageInterlaceTypeValidation(sNewInterlaceType);
	}
}
