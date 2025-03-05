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
#include "spdlog/spdlog.h"

#include <aws/core/Aws.h>
#include <aws/medialive/MediaLiveClient.h>
#include <aws/medialive/model/DescribeChannelRequest.h>
#include <aws/medialive/model/DescribeChannelResult.h>
#include <aws/medialive/model/StartChannelRequest.h>
#include <aws/medialive/model/StopChannelRequest.h>

void EncoderProxy::awsStartChannel(int64_t ingestionJobKey, string awsChannelIdToBeStarted)
{
	Aws::MediaLive::MediaLiveClient mediaLiveClient;

	Aws::MediaLive::Model::StartChannelRequest startChannelRequest;
	startChannelRequest.SetChannelId(awsChannelIdToBeStarted);

	SPDLOG_INFO(
		"mediaLive.StartChannel"
		", ingestionJobKey: {}"
		", awsChannelIdToBeStarted: {}",
		ingestionJobKey, awsChannelIdToBeStarted
	);

	chrono::system_clock::time_point commandTime = chrono::system_clock::now();

	auto startChannelOutcome = mediaLiveClient.StartChannel(startChannelRequest);
	if (!startChannelOutcome.IsSuccess())
	{
		string errorMessage = std::format(
			"AWS Start Channel failed"
			", ingestionJobKey: {}"
			", awsChannelIdToBeStarted: {}"
			", errorType: {}"
			", errorMessage: {}",
			ingestionJobKey, awsChannelIdToBeStarted, static_cast<long>(startChannelOutcome.GetError().GetErrorType()),
			startChannelOutcome.GetError().GetMessage()
		);
		SPDLOG_ERROR(errorMessage);

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

		SPDLOG_INFO(
			"mediaLive.DescribeChannel"
			", ingestionJobKey: {}"
			", awsChannelIdToBeStarted: {}",
			ingestionJobKey, awsChannelIdToBeStarted
		);

		auto describeChannelOutcome = mediaLiveClient.DescribeChannel(describeChannelRequest);
		if (!describeChannelOutcome.IsSuccess())
		{
			string errorMessage = std::format(
				"AWS Describe Channel failed"
				", ingestionJobKey: {}"
				", awsChannelIdToBeStarted: {}"
				", errorType: {}"
				", errorMessage: {}",
				ingestionJobKey, awsChannelIdToBeStarted, static_cast<long>(describeChannelOutcome.GetError().GetErrorType()),
				describeChannelOutcome.GetError().GetMessage()
			);
			SPDLOG_ERROR(errorMessage);

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

	SPDLOG_INFO(
		"mediaLive.StartChannel finished"
		", ingestionJobKey: {}"
		", awsChannelIdToBeStarted: {}"
		", lastChannelState: {}"
		", maxCommandDuration: {}"
		", elapsed (secs): {}",
		ingestionJobKey, awsChannelIdToBeStarted, static_cast<long>(lastChannelState), maxCommandDuration,
		chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - commandTime).count()
	);
}

void EncoderProxy::awsStopChannel(int64_t ingestionJobKey, string awsChannelIdToBeStarted)
{
	chrono::system_clock::time_point start = chrono::system_clock::now();

	Aws::MediaLive::MediaLiveClient mediaLiveClient;

	Aws::MediaLive::Model::StopChannelRequest stopChannelRequest;
	stopChannelRequest.SetChannelId(awsChannelIdToBeStarted);

	SPDLOG_INFO(
		"mediaLive.StopChannel"
		", ingestionJobKey: {}"
		", awsChannelIdToBeStarted: {}",
		ingestionJobKey, awsChannelIdToBeStarted
	);

	chrono::system_clock::time_point commandTime = chrono::system_clock::now();

	auto stopChannelOutcome = mediaLiveClient.StopChannel(stopChannelRequest);
	if (!stopChannelOutcome.IsSuccess())
	{
		string errorMessage = std::format(
			"AWS Stop Channel failed"
			", ingestionJobKey: {}"
			", awsChannelIdToBeStarted: {}"
			", errorType: {}"
			", errorMessage: {}",
			ingestionJobKey, awsChannelIdToBeStarted, static_cast<long>(stopChannelOutcome.GetError().GetErrorType()),
			stopChannelOutcome.GetError().GetMessage()
		);
		SPDLOG_ERROR(errorMessage);

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

		SPDLOG_INFO(
			"mediaLive.DescribeChannel"
			", ingestionJobKey: {}"
			", awsChannelIdToBeStarted: {}",
			ingestionJobKey, awsChannelIdToBeStarted
		);

		auto describeChannelOutcome = mediaLiveClient.DescribeChannel(describeChannelRequest);
		if (!describeChannelOutcome.IsSuccess())
		{
			string errorMessage = std::format(
				"AWS Describe Channel failed"
				", ingestionJobKey: {}"
				", awsChannelIdToBeStarted: {}"
				", errorType: {}"
				", errorMessage: {}",
				ingestionJobKey, awsChannelIdToBeStarted, static_cast<long>(describeChannelOutcome.GetError().GetErrorType()),
				describeChannelOutcome.GetError().GetMessage()
			);
			SPDLOG_ERROR(errorMessage);

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

	SPDLOG_INFO(
		"mediaLive.StopChannel finished"
		", ingestionJobKey: {}"
		", awsChannelIdToBeStarted: {}"
		", lastChannelState: {}"
		", maxCommandDuration: {}"
		", elapsed (secs): {}",
		ingestionJobKey, awsChannelIdToBeStarted, static_cast<long>(lastChannelState), maxCommandDuration,
		chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - commandTime).count()
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
			string errorMessage = std::format(
				"awsSignedURL. playURL wrong format"
				", _proxyIdentifier: {}"
				", playURL: {}",
				_proxyIdentifier, playURL
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		size_t uriStartIndex = playURL.find("/", prefix.size());
		string cloudFrontHostName = playURL.substr(prefix.size(), uriStartIndex - prefix.size());
		string uriPath = playURL.substr(uriStartIndex + 1);

		AWSSigner awsSigner;
		string signedPlayURL =
			awsSigner.calculateSignedURL(cloudFrontHostName, uriPath, _keyPairId, _privateKeyPEMPathName, expirationInMinutes * 60);

		if (signedPlayURL == "")
		{
			string errorMessage = std::format(
				"awsSignedURL. no signedPlayURL found"
				", _proxyIdentifier: {}"
				", signedPlayURL: {}",
				_proxyIdentifier, signedPlayURL
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error e)
	{
		SPDLOG_ERROR(
			"awsSigner failed"
			", _proxyIdentifier: {}"
			", encodingJobKey: {}"
			", exception: {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"awsSigner failed"
			", _proxyIdentifier: {}"
			", encodingJobKey: {}"
			", exception: {}",
			_proxyIdentifier, _encodingItem->_encodingJobKey, e.what()
		);

		throw e;
	}

	return signedPlayURL;
}
