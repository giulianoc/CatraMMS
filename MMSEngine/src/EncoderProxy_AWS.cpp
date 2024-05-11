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

#include "AWSSigner.h"
#include "EncoderProxy.h"

#include <aws/core/Aws.h>
#include <aws/medialive/MediaLiveClient.h>
#include <aws/medialive/model/DescribeChannelRequest.h>
#include <aws/medialive/model/DescribeChannelResult.h>
#include <aws/medialive/model/StartChannelRequest.h>
#include <aws/medialive/model/StopChannelRequest.h>
/*
#include "FFMpeg.h"
#include "JSONUtils.h"
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

*/

void EncoderProxy::awsStartChannel(int64_t ingestionJobKey, string awsChannelIdToBeStarted)
{
	Aws::MediaLive::MediaLiveClient mediaLiveClient;

	Aws::MediaLive::Model::StartChannelRequest startChannelRequest;
	startChannelRequest.SetChannelId(awsChannelIdToBeStarted);

	_logger->info(
		__FILEREF__ + "mediaLive.StartChannel" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
	);

	chrono::system_clock::time_point commandTime = chrono::system_clock::now();

	auto startChannelOutcome = mediaLiveClient.StartChannel(startChannelRequest);
	if (!startChannelOutcome.IsSuccess())
	{
		string errorMessage = __FILEREF__ + "AWS Start Channel failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted +
							  ", errorType: " + to_string((long)startChannelOutcome.GetError().GetErrorType()) +
							  ", errorMessage: " + startChannelOutcome.GetError().GetMessage();
		_logger->error(errorMessage);

		// liveproxy is not stopped in case of error
		// throw runtime_error(errorMessage);
	}

	bool commandFinished = false;
	int maxCommandDuration = 120;
	Aws::MediaLive::Model::ChannelState lastChannelState = Aws::MediaLive::Model::ChannelState::IDLE;
	int sleepInSecondsBetweenChecks = 15;
	while (!commandFinished && chrono::system_clock::now() - commandTime < chrono::seconds(maxCommandDuration))
	{
		Aws::MediaLive::Model::DescribeChannelRequest describeChannelRequest;
		describeChannelRequest.SetChannelId(awsChannelIdToBeStarted);

		_logger->info(
			__FILEREF__ + "mediaLive.DescribeChannel" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
		);

		auto describeChannelOutcome = mediaLiveClient.DescribeChannel(describeChannelRequest);
		if (!describeChannelOutcome.IsSuccess())
		{
			string errorMessage = __FILEREF__ + "AWS Describe Channel failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted +
								  ", errorType: " + to_string((long)describeChannelOutcome.GetError().GetErrorType()) +
								  ", errorMessage: " + describeChannelOutcome.GetError().GetMessage();
			_logger->error(errorMessage);

			this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
		else
		{
			Aws::MediaLive::Model::DescribeChannelResult describeChannelResult = describeChannelOutcome.GetResult();
			lastChannelState = describeChannelResult.GetState();
			if (lastChannelState == Aws::MediaLive::Model::ChannelState::RUNNING)
				commandFinished = true;
			else
				this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
	}

	_logger->info(
		__FILEREF__ + "mediaLive.StartChannel finished" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted + ", lastChannelState: " + to_string((long)lastChannelState) +
		", maxCommandDuration: " + to_string(maxCommandDuration) +
		", elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - commandTime).count())
	);
}

void EncoderProxy::awsStopChannel(int64_t ingestionJobKey, string awsChannelIdToBeStarted)
{
	chrono::system_clock::time_point start = chrono::system_clock::now();

	Aws::MediaLive::MediaLiveClient mediaLiveClient;

	Aws::MediaLive::Model::StopChannelRequest stopChannelRequest;
	stopChannelRequest.SetChannelId(awsChannelIdToBeStarted);

	_logger->info(
		__FILEREF__ + "mediaLive.StopChannel" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
	);

	chrono::system_clock::time_point commandTime = chrono::system_clock::now();

	auto stopChannelOutcome = mediaLiveClient.StopChannel(stopChannelRequest);
	if (!stopChannelOutcome.IsSuccess())
	{
		string errorMessage = __FILEREF__ + "AWS Stop Channel failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted +
							  ", errorType: " + to_string((long)stopChannelOutcome.GetError().GetErrorType()) +
							  ", errorMessage: " + stopChannelOutcome.GetError().GetMessage();
		_logger->error(errorMessage);

		// liveproxy is not stopped in case of error
		// throw runtime_error(errorMessage);
	}

	bool commandFinished = false;
	int maxCommandDuration = 120;
	Aws::MediaLive::Model::ChannelState lastChannelState = Aws::MediaLive::Model::ChannelState::RUNNING;
	int sleepInSecondsBetweenChecks = 15;
	while (!commandFinished && chrono::system_clock::now() - commandTime < chrono::seconds(maxCommandDuration))
	{
		Aws::MediaLive::Model::DescribeChannelRequest describeChannelRequest;
		describeChannelRequest.SetChannelId(awsChannelIdToBeStarted);

		_logger->info(
			__FILEREF__ + "mediaLive.DescribeChannel" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
		);

		auto describeChannelOutcome = mediaLiveClient.DescribeChannel(describeChannelRequest);
		if (!describeChannelOutcome.IsSuccess())
		{
			string errorMessage = __FILEREF__ + "AWS Describe Channel failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted +
								  ", errorType: " + to_string((long)describeChannelOutcome.GetError().GetErrorType()) +
								  ", errorMessage: " + describeChannelOutcome.GetError().GetMessage();
			_logger->error(errorMessage);

			this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
		else
		{
			Aws::MediaLive::Model::DescribeChannelResult describeChannelResult = describeChannelOutcome.GetResult();
			lastChannelState = describeChannelResult.GetState();
			if (lastChannelState == Aws::MediaLive::Model::ChannelState::IDLE)
				commandFinished = true;
			else
				this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
	}

	_logger->info(
		__FILEREF__ + "mediaLive.StopChannel finished" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
		", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted + ", lastChannelState: " + to_string((long)lastChannelState) +
		", maxCommandDuration: " + to_string(maxCommandDuration) +
		", elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - commandTime).count())
	);
}

string EncoderProxy::getAWSSignedURL(string playURL, int expirationInMinutes)
{
	string signedPlayURL;

	// string mmsGUIURL;
	// ostringstream response;
	// bool responseInitialized = false;
	try
	{
		// playURL is like:
		// https://d1nue3l1x0sz90.cloudfront.net/out/v1/ca8fd629f9204ca38daf18f04187c694/index.m3u8
		string prefix("https://");
		if (!(playURL.size() >= prefix.size() && 0 == playURL.compare(0, prefix.size(), prefix) && playURL.find("/", prefix.size()) != string::npos))
		{
			string errorMessage =
				__FILEREF__ + "awsSignedURL. playURL wrong format" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", playURL: " + playURL;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		size_t uriStartIndex = playURL.find("/", prefix.size());
		string cloudFrontHostName = playURL.substr(prefix.size(), uriStartIndex - prefix.size());
		string uriPath = playURL.substr(uriStartIndex + 1);

		AWSSigner awsSigner(_logger);
		string signedPlayURL =
			awsSigner.calculateSignedURL(cloudFrontHostName, uriPath, _keyPairId, _privateKeyPEMPathName, expirationInMinutes * 60);

		if (signedPlayURL == "")
		{
			string errorMessage = __FILEREF__ + "awsSignedURL. no signedPlayURL found" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", signedPlayURL: " + signedPlayURL;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error e)
	{
		_logger->error(
			__FILEREF__ + "awsSigner failed (exception)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(
			__FILEREF__ + "awsSigner failed (exception)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", exception: " + e.what()
		);

		throw e;
	}

	return signedPlayURL;
}
