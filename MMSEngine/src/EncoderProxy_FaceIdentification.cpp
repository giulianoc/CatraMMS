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
#include "LocalAssetIngestionEvent.h"

#include "opencv2/face.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect.hpp"

string EncoderProxy::faceIdentification()
{
	{
		lock_guard<mutex> locker(*_mtEncodingJobs);

		*_status = EncodingJobStatus::Running;
	}

	_localEncodingProgress = 0;

	// build the deep learned model
	vector<cv::Mat> images;
	vector<int> idImages;
	unordered_map<int, string> idTagMap;
	{
		vector<string> deepLearnedModelTags;

		string field = "deepLearnedModelTagsCommaSeparated";
		stringstream ssDeepLearnedModelTagsCommaSeparated(JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, ""));
		while (ssDeepLearnedModelTagsCommaSeparated.good())
		{
			string tag;
			getline(ssDeepLearnedModelTagsCommaSeparated, tag, ',');

			deepLearnedModelTags.push_back(tag);
		}

		int64_t mediaItemKey = -1;
		vector<int64_t> otherMediaItemsKey;
		string uniqueName;
		int64_t physicalPathKey = -1;
		bool contentTypePresent = true;
		MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::ContentType::Image;
		// bool startAndEndIngestionDatePresent = false;
		string startIngestionDate;
		string endIngestionDate;
		string title;
		int liveRecordingChunk = -1;
		int64 deliveryCode = -1;
		int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
		int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
		string jsonCondition;
		string orderBy;
		string jsonOrderBy;
		set<string> responseFields;
		bool admin = true;

		int start = 0;
		int rows = 200;
		int totalImagesNumber = -1;
		bool imagesFinished = false;

		int idImageCounter = 0;
		unordered_map<string, int> tagIdMap;
		vector<string> tagsNotIn;

		while (!imagesFinished)
		{
			json mediaItemsListRoot = _mmsEngineDBFacade->getMediaItemsList(
				_encodingItem->_workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey, start, rows,
				contentTypePresent, contentType,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate, title, liveRecordingChunk, deliveryCode, utcCutPeriodStartTimeInMilliSeconds,
				utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, deepLearnedModelTags, tagsNotIn, orderBy, jsonOrderBy, responseFields,
				admin,
				// 2022-12-18: MIKs dovrebbero essere stati aggiunti da un po
				false
			);

			field = "response";
			json responseRoot = mediaItemsListRoot[field];

			if (totalImagesNumber == -1)
			{
				field = "numFound";
				totalImagesNumber = JSONUtils::asInt(responseRoot, field, 0);
			}

			field = "mediaItems";
			json mediaItemsArrayRoot = responseRoot[field];
			if (mediaItemsArrayRoot.size() < rows)
				imagesFinished = true;
			else
				start += rows;

			_logger->info(
				__FILEREF__ + "Called getMediaItemsList" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", mediaItemsArrayRoot.size(): " + to_string(mediaItemsArrayRoot.size())
			);

			for (int imageIndex = 0; imageIndex < mediaItemsArrayRoot.size(); imageIndex++)
			{
				json mediaItemRoot = mediaItemsArrayRoot[imageIndex];

				int currentIdImage;
				unordered_map<string, int>::iterator tagIdIterator;

				field = "tags";
				string tags = JSONUtils::asString(mediaItemRoot, field, "");
				if (tags.front() == ',')
					tags = tags.substr(1);
				if (tags.back() == ',')
					tags.pop_back();

				tagIdIterator = tagIdMap.find(tags);
				if (tagIdIterator == tagIdMap.end())
				{
					currentIdImage = idImageCounter++;
					tagIdMap.insert(make_pair(tags, currentIdImage));
				}
				else
					currentIdImage = (*tagIdIterator).second;

				{
					unordered_map<int, string>::iterator idTagIterator;

					idTagIterator = idTagMap.find(currentIdImage);
					if (idTagIterator == idTagMap.end())
						idTagMap.insert(make_pair(currentIdImage, tags));
				}

				field = "physicalPaths";
				json physicalPathsArrayRoot = mediaItemRoot[field];
				if (physicalPathsArrayRoot.size() > 0)
				{
					json physicalPathRoot = physicalPathsArrayRoot[0];

					field = "physicalPathKey";
					int64_t physicalPathKey = JSONUtils::asInt64(physicalPathRoot, field, 0);

					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							physicalPathKey,
							// 2022-12-18: MIK dovrebbe essere aggiunto da
							// un po
							false
						);
					string mmsImagePathName;
					tie(mmsImagePathName, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;

					images.push_back(cv::imread(mmsImagePathName, 0));
					idImages.push_back(currentIdImage);
				}
			}
		}
	}

	_logger->info(
		__FILEREF__ + "Deep learned model built" + ", images.size: " + to_string(images.size()) + ", idImages.size: " + to_string(idImages.size()) +
		", idTagMap.size: " + to_string(idTagMap.size())
	);

	if (images.size() == 0)
	{
		string errorMessage =
			__FILEREF__ + "The Deep Learned Model is empty, no deepLearnedModelTags found" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	string faceIdentificationCascadeName;
	string sourcePhysicalPath;
	{
		string field = "faceIdentificationCascadeName";
		faceIdentificationCascadeName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, 0);

		field = "sourcePhysicalPath";
		sourcePhysicalPath = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");
	}

	string cascadePathName = _computerVisionCascadePath + "/" + faceIdentificationCascadeName + ".xml";

	cv::CascadeClassifier cascade;
	if (!cascade.load(cascadePathName))
	{
		string errorMessage = __FILEREF__ + "cascadeName could not be loaded" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
							  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
							  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", cascadePathName: " + cascadePathName;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	// The following lines create an LBPH model for
	// face recognition and train it with the images and
	// labels.
	//
	// The LBPHFaceRecognizer uses Extended Local Binary Patterns
	// (it's probably configurable with other operators at a later
	// point), and has the following default values
	//
	//      radius = 1
	//      neighbors = 8
	//      grid_x = 8
	//      grid_y = 8
	//
	// So if you want a LBPH FaceRecognizer using a radius of
	// 2 and 16 neighbors, call the factory method with:
	//
	//      cv::face::LBPHFaceRecognizer::create(2, 16);
	//
	// And if you want a threshold (e.g. 123.0) call it with its default values:
	//
	//      cv::face::LBPHFaceRecognizer::create(1,8,8,8,123.0)
	//
	cv::Ptr<cv::face::LBPHFaceRecognizer> recognizerModel = cv::face::LBPHFaceRecognizer::create();
	recognizerModel->train(images, idImages);

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

	string faceIdentificationMediaPathName;
	string fileFormat;
	{
		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(_encodingItem->_workspace);

		{
			// opencv does not have issues with avi and mov (it seems has issues
			// with mp4)
			fileFormat = "avi";

			faceIdentificationMediaPathName = workspaceIngestionRepository + "/" + to_string(_encodingItem->_ingestionJobKey) + "." + fileFormat;
		}
	}

	_logger->info(
		__FILEREF__ + "faceIdentification started" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", cascadeName: " + faceIdentificationCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath +
		", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName
	);

	cv::VideoWriter writer;
	long totalFramesNumber;
	{
		totalFramesNumber = (long)capture.get(cv::CAP_PROP_FRAME_COUNT);
		double fps = capture.get(cv::CAP_PROP_FPS);
		cv::Size size((int)capture.get(cv::CAP_PROP_FRAME_WIDTH), (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT));

		{
			writer.open(faceIdentificationMediaPathName, cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, size);
		}
	}

	_logger->info(
		__FILEREF__ + "generating Face Identification" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", cascadeName: " + faceIdentificationCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath +
		", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName
	);

	int progressNotificationPeriodInSeconds = 2;
	chrono::system_clock::time_point lastProgressNotification =
		chrono::system_clock::now() - chrono::seconds(progressNotificationPeriodInSeconds + 1);

	cv::Mat bgrFrame;
	cv::Mat grayFrame;
	cv::Mat smallFrame;

	long currentFrameIndex = 0;
	while (true)
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
				__FILEREF__ + "generating Face Recognition" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", cascadeName: " + faceIdentificationCascadeName +
				", sourcePhysicalPath: " + sourcePhysicalPath + ", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName +
				", currentFrameIndex: " + to_string(currentFrameIndex) + ", totalFramesNumber: " + to_string(totalFramesNumber)
			);
		}

		capture >> bgrFrame;
		if (bgrFrame.empty())
			break;

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

		for (size_t i = 0; i < faces.size(); i++)
		{
			cv::Rect roiRectScaled = faces[i];

			// Crop the full image to that image contained by the rectangle
			// myROI Note that this doesn't copy the data
			cv::Rect roiRect(
				roiRectScaled.x * _computerVisionDefaultScale, roiRectScaled.y * _computerVisionDefaultScale,
				roiRectScaled.width * _computerVisionDefaultScale, roiRectScaled.height * _computerVisionDefaultScale
			);
			cv::Mat grayFrameCropped(grayFrame, roiRect);

			string predictedTags;
			{
				// int predictedLabel =
				// recognizerModel->predict(grayFrameCropped); To get the
				// confidence of a prediction call the model with:
				int predictedIdImage = -1;
				double confidence = 0.0;
				recognizerModel->predict(grayFrameCropped, predictedIdImage, confidence);

				{
					unordered_map<int, string>::iterator idTagIterator;

					idTagIterator = idTagMap.find(predictedIdImage);
					if (idTagIterator != idTagMap.end())
						predictedTags = (*idTagIterator).second;
				}

				_logger->info(
					__FILEREF__ + "recognizerModel->predict" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
					", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", predictedIdImage: " + to_string(predictedIdImage) +
					", confidence: " + to_string(confidence) + ", predictedTags: " + predictedTags
				);
			}

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

				double fontScale = 2;
				cv::putText(
					bgrFrame, predictedTags,
					cv::Point(cvRound(roiRectScaled.x * _computerVisionDefaultScale), cvRound(roiRectScaled.y * _computerVisionDefaultScale)),
					cv::FONT_HERSHEY_PLAIN, fontScale, color, thickness
				);
			}
		}

		writer << bgrFrame;
	}

	capture.release();

	_logger->info(
		__FILEREF__ + "faceIdentification media done" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", cascadeName: " + faceIdentificationCascadeName + ", sourcePhysicalPath: " + sourcePhysicalPath +
		", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName
	);

	return faceIdentificationMediaPathName;
}

void EncoderProxy::processFaceIdentification(string stagingEncodedAssetPathName)
{
	try
	{
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
			__FILEREF__ + "processFaceIdentification failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName + ", e.what(): " + e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "processFaceIdentification failed" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
			", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName +
			", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
		);

		throw e;
	}
}
