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
