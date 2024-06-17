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
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "LocalAssetIngestionEvent.h"

#include "opencv2/face.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect.hpp"
/*
#include "AWSSigner.h"
#include "MMSCURL.h"
#include "MMSDeliveryAuthorization.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include <fstream>
#include <regex>

#include <aws/core/Aws.h>
#include <aws/medialive/MediaLiveClient.h>
#include <aws/medialive/model/DescribeChannelRequest.h>
#include <aws/medialive/model/DescribeChannelResult.h>
#include <aws/medialive/model/StartChannelRequest.h>
#include <aws/medialive/model/StopChannelRequest.h>
*/

string EncoderProxy::faceRecognition()
{
	{
		lock_guard<mutex> locker(*_mtEncodingJobs);

		*_status = EncodingJobStatus::Running;
	}

	_localEncodingProgress = 0.0;

	if (_faceRecognitionNumber.use_count() > _maxFaceRecognitionNumber)
	{
		string errorMessage = string("MaxConcurrentJobsReached") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
							  ", _faceRecognitionNumber.use_count: " + to_string(_faceRecognitionNumber.use_count()) +
							  ", _maxFaceRecognitionNumber: " + to_string(_maxFaceRecognitionNumber);
		_logger->warn(__FILEREF__ + errorMessage);

		throw MaxConcurrentJobsReached();
	}

	int64_t sourceMediaItemKey;
	string faceRecognitionCascadeName;
	string sourcePhysicalPath;
	string faceRecognitionOutput;
	long initialFramesNumberToBeSkipped;
	bool oneFramePerSecond;
	{
		string field = "sourceMediaItemKey";
		sourceMediaItemKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

		field = "faceRecognitionCascadeName";
		faceRecognitionCascadeName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

		field = "sourcePhysicalPath";
		sourcePhysicalPath = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

		// VideoWithHighlightedFaces, ImagesToBeUsedInDeepLearnedModel or
		// FrameContainingFace
		field = "faceRecognitionOutput";
		faceRecognitionOutput = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

		field = "initialFramesNumberToBeSkipped";
		initialFramesNumberToBeSkipped = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

		field = "oneFramePerSecond";
		oneFramePerSecond = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);
	}

	string cascadePathName = _computerVisionCascadePath + "/" + faceRecognitionCascadeName + ".xml";

	_logger->info(
		__FILEREF__ + "faceRecognition" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", cascadeName: " + faceRecognitionCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath
	);

	cv::CascadeClassifier cascade;
	if (!cascade.load(cascadePathName))
	{
		string errorMessage = __FILEREF__ + "cascadeName could not be loaded" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", cascadePathName: " + cascadePathName;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	// sometimes the file was created by another MMSEngine and it is not found
	// just because of nfs delay. For this reason we implemented a retry
	// mechanism
	bool fileExists = false;
	{
		chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
		do
		{
			if (fs::exists(sourcePhysicalPath))
			{
				fileExists = true;
				break;
			}

			this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
		} while (chrono::system_clock::now() < end);
	}

	if (!fileExists)
	{
		string errorMessage = __FILEREF__ + "Media Source file does not exist" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", sourcePhysicalPath: " + sourcePhysicalPath;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	cv::VideoCapture capture;
	capture.open(sourcePhysicalPath, cv::CAP_FFMPEG);
	if (!capture.isOpened())
	{
		string errorMessage = __FILEREF__ + "Capture could not be opened" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", sourcePhysicalPath: " + sourcePhysicalPath;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	string faceRecognitionMediaPathName;
	string fileFormat;
	{
		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(_encodingItem->_workspace);
		if (faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel" || faceRecognitionOutput == "FrameContainingFace")
		{
			fileFormat = "jpg";

			faceRecognitionMediaPathName = workspaceIngestionRepository + "/"; // sourceFileName is added later
		}
		else // if (faceRecognitionOutput == "VideoWithHighlightedFaces")
		{
			// opencv does not have issues with avi and mov (it seems has issues
			// with mp4)
			fileFormat = "avi";

			faceRecognitionMediaPathName = workspaceIngestionRepository + "/" + to_string(_encodingItem->_ingestionJobKey) + "." + fileFormat;
		}
	}

	_logger->info(
		__FILEREF__ + "faceRecognition started" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", cascadeName: " + faceRecognitionCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath +
		", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName
	);

	cv::VideoWriter writer;
	long totalFramesNumber;
	double fps;
	{
		totalFramesNumber = (long)capture.get(cv::CAP_PROP_FRAME_COUNT);
		fps = capture.get(cv::CAP_PROP_FPS);
		cv::Size size((int)capture.get(cv::CAP_PROP_FRAME_WIDTH), (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT));

		if (faceRecognitionOutput == "VideoWithHighlightedFaces")
		{
			writer.open(faceRecognitionMediaPathName, cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, size);
		}
	}

	_logger->info(
		__FILEREF__ + "generating Face Recognition start" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", cascadeName: " + faceRecognitionCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath + ", faceRecognitionMediaPathName: " +
		faceRecognitionMediaPathName + ", totalFramesNumber: " + to_string(totalFramesNumber) + ", fps: " + to_string(fps)
	);

	cv::Mat bgrFrame;
	cv::Mat grayFrame;
	cv::Mat smallFrame;

	// this is used only in case of faceRecognitionOutput ==
	// "FacesImagesToBeUsedInDeepLearnedModel" Essentially the last image source
	// file name will be ingested when we will go out of the loop (while(true))
	// in order to set the IngestionRowToBeUpdatedAsSuccess flag a true for this
	// last ingestion
	string lastImageSourceFileName;

	int progressNotificationPeriodInSeconds = 2;
	chrono::system_clock::time_point lastProgressNotification =
		chrono::system_clock::now() - chrono::seconds(progressNotificationPeriodInSeconds + 1);

	long currentFrameIndex = 0;
	long framesContainingFaces = 0;

	bool bgrFrameEmpty = false;
	if (faceRecognitionOutput == "FrameContainingFace")
	{
		long initialFrameIndex = 0;
		while (initialFrameIndex++ < initialFramesNumberToBeSkipped)
		{
			if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastProgressNotification).count() >
				progressNotificationPeriodInSeconds)
			{
				lastProgressNotification = chrono::system_clock::now();

				_localEncodingProgress = 100 * currentFrameIndex / totalFramesNumber;

				try
				{
					_logger->info(
						__FILEREF__ + "updateEncodingJobProgress" + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
						", encodingProgress: " + to_string(_localEncodingProgress)
					);
					_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, _localEncodingProgress);
				}
				catch (runtime_error &e)
				{
					_logger->error(
						__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
					);
				}
				catch (exception &e)
				{
					_logger->error(
						__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}

				_logger->info(
					__FILEREF__ + "generating Face Recognition progress" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", cascadeName: " + faceRecognitionCascadeName +
					", sourcePhysicalPath: " + sourcePhysicalPath + ", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName +
					", currentFrameIndex: " + to_string(currentFrameIndex) + ", totalFramesNumber: " + to_string(totalFramesNumber) +
					", _localEncodingProgress: " + to_string(_localEncodingProgress)
				);
			}

			capture >> bgrFrame;
			currentFrameIndex++;
			if (bgrFrame.empty())
			{
				bgrFrameEmpty = true;

				break;
			}
		}
	}

	bool frameContainingFaceFound = false;
	while (!bgrFrameEmpty)
	{
		if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastProgressNotification).count() >
			progressNotificationPeriodInSeconds)
		{
			lastProgressNotification = chrono::system_clock::now();

			/*
			double progress = (currentFrameIndex / totalFramesNumber) * 100;
			// this is to have one decimal in the percentage
			double faceRecognitionPercentage = ((double) ((int) (progress *
			10))) / 10;
			*/
			_localEncodingProgress = 100 * currentFrameIndex / totalFramesNumber;

			try
			{
				_logger->info(
					__FILEREF__ + "updateEncodingJobProgress" + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", encodingProgress: " + to_string(_localEncodingProgress)
				);
				_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, _localEncodingProgress);
			}
			catch (runtime_error &e)
			{
				_logger->error(
					__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
				);
			}
			catch (exception &e)
			{
				_logger->error(
					__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				);
			}

			_logger->info(
				__FILEREF__ + "generating Face Recognition progress" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", cascadeName: " + faceRecognitionCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath +
				", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName + ", currentFrameIndex: " + to_string(currentFrameIndex) +
				", totalFramesNumber: " + to_string(totalFramesNumber) + ", _localEncodingProgress: " + to_string(_localEncodingProgress)
			);
		}

		if (faceRecognitionOutput == "FrameContainingFace" && oneFramePerSecond)
		{
			int frameIndex = fps - 1;
			while (--frameIndex >= 0)
			{
				capture >> bgrFrame;
				currentFrameIndex++;
				if (bgrFrame.empty())
				{
					bgrFrameEmpty = true;

					break;
				}
			}

			if (bgrFrameEmpty)
				continue;
		}

		capture >> bgrFrame;
		if (bgrFrame.empty())
		{
			bgrFrameEmpty = true;

			continue;
		}

		currentFrameIndex++;

		cv::cvtColor(bgrFrame, grayFrame, cv::COLOR_BGR2GRAY);
		double xAndYScaleFactor = 1 / _computerVisionDefaultScale;
		cv::resize(grayFrame, smallFrame, cv::Size(), xAndYScaleFactor, xAndYScaleFactor, cv::INTER_LINEAR_EXACT);
		cv::equalizeHist(smallFrame, smallFrame);

		vector<cv::Rect> faces;
		cascade.detectMultiScale(
			smallFrame, faces, _computerVisionDefaultScale, _computerVisionDefaultMinNeighbors, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30)
		);

		if (_computerVisionDefaultTryFlip)
		{
			// 1: flip (mirror) horizontally
			cv::flip(smallFrame, smallFrame, 1);
			vector<cv::Rect> faces2;
			cascade.detectMultiScale(
				smallFrame, faces2, _computerVisionDefaultScale, _computerVisionDefaultMinNeighbors, 0 | cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30)
			);
			for (vector<cv::Rect>::const_iterator r = faces2.begin(); r != faces2.end(); ++r)
				faces.push_back(cv::Rect(smallFrame.cols - r->x - r->width, r->y, r->width, r->height));
		}

		if (faceRecognitionOutput == "VideoWithHighlightedFaces" || faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel")
		{
			if (faces.size() > 0)
				framesContainingFaces++;

			for (size_t i = 0; i < faces.size(); i++)
			{
				cv::Rect roiRectScaled = faces[i];
				// cv::Mat smallROI;

				if (faceRecognitionOutput == "VideoWithHighlightedFaces")
				{
					cv::Scalar color = cv::Scalar(255, 0, 0);
					double aspectRatio = (double)roiRectScaled.width / roiRectScaled.height;
					int thickness = 3;
					int lineType = 8;
					int shift = 0;
					if (0.75 < aspectRatio && aspectRatio < 1.3)
					{
						cv::Point center;
						int radius;

						center.x = cvRound((roiRectScaled.x + roiRectScaled.width * 0.5) * _computerVisionDefaultScale);
						center.y = cvRound((roiRectScaled.y + roiRectScaled.height * 0.5) * _computerVisionDefaultScale);
						radius = cvRound((roiRectScaled.width + roiRectScaled.height) * 0.25 * _computerVisionDefaultScale);
						cv::circle(bgrFrame, center, radius, color, thickness, lineType, shift);
					}
					else
					{
						cv::rectangle(
							bgrFrame,
							cv::Point(cvRound(roiRectScaled.x * _computerVisionDefaultScale), cvRound(roiRectScaled.y * _computerVisionDefaultScale)),
							cv::Point(
								cvRound((roiRectScaled.x + roiRectScaled.width - 1) * _computerVisionDefaultScale),
								cvRound((roiRectScaled.y + roiRectScaled.height - 1) * _computerVisionDefaultScale)
							),
							color, thickness, lineType, shift
						);
					}
				}
				else
				{
					// Crop the full image to that image contained by the
					// rectangle myROI Note that this doesn't copy the data
					cv::Rect roiRect(
						roiRectScaled.x * _computerVisionDefaultScale, roiRectScaled.y * _computerVisionDefaultScale,
						roiRectScaled.width * _computerVisionDefaultScale, roiRectScaled.height * _computerVisionDefaultScale
					);
					cv::Mat grayFrameCropped(grayFrame, roiRect);

					/*
					cv::Mat cropped;
					// Copy the data into new matrix
					grayFrameCropped.copyTo(cropped);
					*/

					string sourceFileName = to_string(_encodingItem->_ingestionJobKey) + "_" + to_string(currentFrameIndex) + "." + fileFormat;

					string faceRecognitionImagePathName = faceRecognitionMediaPathName + sourceFileName;

					cv::imwrite(faceRecognitionImagePathName, grayFrameCropped);
					// cv::imwrite(faceRecognitionImagePathName, cropped);

					if (lastImageSourceFileName == "")
						lastImageSourceFileName = sourceFileName;
					else
					{
						// ingest the face
						int64_t faceOfVideoMediaItemKey = sourceMediaItemKey;
						string mediaMetaDataContent = generateMediaMetadataToIngest(
							_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1,
							-1, // cutOfVideoMediaItemKey
							vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
						);

						shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
							_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
								MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
							);

						localAssetIngestionEvent->setSource(ENCODERPROXY);
						localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

						localAssetIngestionEvent->setExternalReadOnlyStorage(false);
						localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
						localAssetIngestionEvent->setIngestionSourceFileName(lastImageSourceFileName);
						localAssetIngestionEvent->setMMSSourceFileName(lastImageSourceFileName);
						localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
						localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
						localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(false);
						// localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate)
						// + "/1");

						localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

						shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
						_multiEventsSet->addEvent(event);

						_logger->info(
							__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", sourceFileName: " + lastImageSourceFileName +
							", getEventKey().first: " + to_string(event->getEventKey().first) +
							", getEventKey().second: " + to_string(event->getEventKey().second)
						);

						lastImageSourceFileName = sourceFileName;
					}
				}
			}

			if (faceRecognitionOutput == "VideoWithHighlightedFaces")
			{
				writer << bgrFrame;
			}
		}
		else // if (faceRecognitionOutput == "FrameContainingFace")
		{
			if (faces.size() > 0)
			{
				framesContainingFaces++;

				// ingest the frame
				string sourceFileName =
					to_string(_encodingItem->_ingestionJobKey) + "_frameContainingFace" + "_" + to_string(currentFrameIndex) + "." + fileFormat;

				string faceRecognitionImagePathName = faceRecognitionMediaPathName + sourceFileName;

				cv::imwrite(faceRecognitionImagePathName, bgrFrame);

				int64_t faceOfVideoMediaItemKey = sourceMediaItemKey;
				string mediaMetaDataContent = generateMediaMetadataToIngest(
					_encodingItem->_ingestionJobKey, fileFormat, faceOfVideoMediaItemKey, -1, -1,
					-1, // cutOfVideoMediaItemKey
					vector<int64_t>(), vector<int64_t>(), _encodingItem->_ingestedParametersRoot
				);

				shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
					_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
					);

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
				// localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate)
				// + "/1");

				localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

				shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
				_multiEventsSet->addEvent(event);

				frameContainingFaceFound = true;

				_logger->info(
					__FILEREF__ +
					"addEvent: EVENT_TYPE (INGESTASSETEVENT) - "
					"FrameContainingFace" +
					", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
					", sourceFileName: " + sourceFileName + ", getEventKey().first: " + to_string(event->getEventKey().first) +
					", getEventKey().second: " + to_string(event->getEventKey().second)
				);

				break;
			}
		}
	}

	if (faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel")
	{
		if (lastImageSourceFileName != "")
		{
			// ingest the face
			int64_t faceOfVideoMediaItemKey = sourceMediaItemKey;
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
			localAssetIngestionEvent->setIngestionSourceFileName(lastImageSourceFileName);
			localAssetIngestionEvent->setMMSSourceFileName(lastImageSourceFileName);
			localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
			localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
			localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
			// localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate)
			// + "/1");

			localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

			shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
			_multiEventsSet->addEvent(event);

			_logger->info(
				__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", sourceFileName: " + lastImageSourceFileName +
				", getEventKey().first: " + to_string(event->getEventKey().first) +
				", getEventKey().second: " + to_string(event->getEventKey().second)
			);
		}
		else
		{
			// no faces were met, let's update ingestion status
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_IngestionFailure;

			string errorMessage = "No faces recognized";
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", IngestionStatus: " +
				MMSEngineDBFacade::toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			_mmsEngineDBFacade->updateIngestionJob(_encodingItem->_ingestionJobKey, newIngestionStatus, errorMessage);
		}
	}
	else if (faceRecognitionOutput == "FrameContainingFace")
	{
		// in case the frame containing a face was not found
		if (!frameContainingFaceFound)
		{
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_IngestionFailure;

			string errorMessage = "No face recognized";
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", IngestionStatus: " +
				MMSEngineDBFacade::toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			_mmsEngineDBFacade->updateIngestionJob(_encodingItem->_ingestionJobKey, newIngestionStatus, errorMessage);
		}
		else
		{
			_logger->info(
				__FILEREF__ + "faceRecognition media done" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
				to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", cascadeName: " + faceRecognitionCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath +
				", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName + ", currentFrameIndex: " + to_string(currentFrameIndex) +
				", framesContainingFaces: " + to_string(framesContainingFaces) + ", frameContainingFaceFound: " + to_string(frameContainingFaceFound)
			);
		}
	}

	capture.release();

	_logger->info(
		__FILEREF__ + "faceRecognition media done" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", cascadeName: " + faceRecognitionCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath +
		", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName + ", currentFrameIndex: " + to_string(currentFrameIndex) +
		", framesContainingFaces: " + to_string(framesContainingFaces)
	);

	return faceRecognitionMediaPathName;
}

void EncoderProxy::processFaceRecognition(string stagingEncodedAssetPathName)
{
	try
	{
		string faceRecognitionOutput;
		{
			// VideoWithHighlightedFaces or ImagesToBeUsedInDeepLearnedModel
			string field = "faceRecognitionOutput";
			faceRecognitionOutput = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");
		}

		if (faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel" || faceRecognitionOutput == "FrameContainingFace")
		{
			// nothing to do, all the faces (images) were already ingested

			return;
		}

		// faceRecognitionOutput is "VideoWithHighlightedFaces"

		size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "No extention find in the asset file name" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

		size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
		if (fileNameIndex == string::npos)
		{
			string errorMessage = __FILEREF__ + "No fileName find in the asset path name" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
			_logger->error(errorMessage);

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
		// localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate)
		// + "/1");

		localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

		shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		_logger->info(
			__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", sourceFileName: " + sourceFileName +
			", getEventKey().first: " + to_string(event->getEventKey().first) + ", getEventKey().second: " + to_string(event->getEventKey().second)
		);
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + "processFaceRecognition failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName + ", e.what(): " + e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "processFaceRecognition failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
		);

		throw e;
	}
}
