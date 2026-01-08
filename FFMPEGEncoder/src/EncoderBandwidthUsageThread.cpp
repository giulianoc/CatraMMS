/*
Copyright (C) Giuliano Catrambone (giulianocatrambone@gmail.com)

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 Commercial use other than under the terms of the GNU General Public
 License is allowed only after express negotiation of conditions
 with the authors.
*/


#include "EncoderBandwidthUsageThread.h"

#include "CurlWrapper.h"
#include "Encrypt.h"

EncoderBandwidthUsageThread::EncoderBandwidthUsageThread(const json & configurationRoot)
{
	_mmsAPIProtocol = JsonPath(&configurationRoot)["api"]["protocol"].as<std::string>();
	SPDLOG_INFO("Configuration item"
		", api->protocol: {}", _mmsAPIProtocol);
	_mmsAPIHostname = JsonPath(&configurationRoot)["api"]["hostname"].as<std::string>();
	SPDLOG_INFO("Configuration item"
		", api->hostname: {}", _mmsAPIHostname);
	_mmsAPIPort = JsonPath(&configurationRoot)["api"]["port"].as<int32_t>(0);
	SPDLOG_INFO("Configuration item"
		", api->port: {}", _mmsAPIPort);
	_mmsAPIVersion = JsonPath(&configurationRoot)["api"]["version"].as<std::string>();
	SPDLOG_INFO("Configuration item"
		", api->version: {}", _mmsAPIVersion);
	_mmsAPIUpdateBandwidthStatsURI = JsonPath(&configurationRoot)["api"]["updateBandwidthStatsURI"].as<std::string>();
	SPDLOG_INFO("Configuration item"
		", api->updateBandwidthStatsURI: {}", _mmsAPIUpdateBandwidthStatsURI);
	_mmsAPITimeoutInSeconds = JsonPath(&configurationRoot)["api"]["timeoutInSeconds"].as<int32_t>(120);
	SPDLOG_INFO(
		"Configuration item"
		", api->timeoutInSeconds: {}",
		_mmsAPITimeoutInSeconds
	);
	_updateBandwidthStatsUser = JsonPath(&configurationRoot)["api"]["updateBandwidthStatsUser"].as<std::string>();
	SPDLOG_INFO("Configuration item"
		", api->updateBandwidthStatsUser: {}", _updateBandwidthStatsUser);
	auto updateBandwidthStatsPasswordEncrypted = JsonPath(&configurationRoot)["api"]["updateBandwidthStatsPassword"].as<std::string>();
	SPDLOG_INFO("Configuration item"
		", api->updateBandwidthStatsPassword: {}", _updateBandwidthStatsPassword);
	_updateBandwidthStatsPassword = Encrypt::opensslDecrypt(updateBandwidthStatsPasswordEncrypted);

	_encoderKey = JsonPath(&configurationRoot)["ffmpeg"]["encoderKey"].as<int32_t>(-1);
	SPDLOG_INFO("Configuration item"
		", ffmpeg->encoderKey: {}", _encoderKey);
}

void EncoderBandwidthUsageThread::newBandwidthUsageAvailable(uint64_t& txAvgBandwidthUsage, uint64_t& rxAvgBandwidthUsage) const
{

	if (_encoderKey < 0)
	{
		SPDLOG_ERROR("The 'encoderKey' configuration item is not valid and bandwidth stats is not sent to API"
			", encoderKey: {}", _encoderKey);
		return;
	}

	const std::string mmsAPIUpdateBandwidthStatsURL = std::format("{}://{}:{}/catramms/{}/encoder/{}{}/{}/{}",
		_mmsAPIProtocol, _mmsAPIHostname, _mmsAPIPort, _mmsAPIVersion, _encoderKey, _mmsAPIUpdateBandwidthStatsURI,
		txAvgBandwidthUsage, rxAvgBandwidthUsage);

	constexpr std::vector<std::string> otherHeaders;
	nlohmann::json encoderResponse = CurlWrapper::httpPutStringAndGetJson(
		mmsAPIUpdateBandwidthStatsURL, _mmsAPITimeoutInSeconds,
		CurlWrapper::basicAuthorization(_updateBandwidthStatsUser, _updateBandwidthStatsPassword),
		"", "application/json", // contentType
		otherHeaders, ""
	);
}
