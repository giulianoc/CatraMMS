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

EncoderBandwidthUsageThread::EncoderBandwidthUsageThread(const json & configurationRoot,
	const std::optional<std::string> &interfaceNameToMonitor,
	const std::shared_ptr<spdlog::logger>& logger):
	BandwidthUsageThread(interfaceNameToMonitor, logger)
{
	_mmsAPIProtocol = JsonPath(&configurationRoot)["api"]["protocol"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->protocol: {}", _mmsAPIProtocol);
	_mmsAPIHostname = JsonPath(&configurationRoot)["api"]["hostname"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->hostname: {}", _mmsAPIHostname);
	_mmsAPIPort = JsonPath(&configurationRoot)["api"]["port"].as<int32_t>(0);
	LOG_INFO("Configuration item"
		", api->port: {}", _mmsAPIPort);
	_mmsAPIVersion = JsonPath(&configurationRoot)["api"]["version"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->version: {}", _mmsAPIVersion);
	_mmsAPIUpdateBandwidthStatsURI = JsonPath(&configurationRoot)["api"]["updateBandwidthStatsURI"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->updateBandwidthStatsURI: {}", _mmsAPIUpdateBandwidthStatsURI);
	_updateStatsUser = JsonPath(&configurationRoot)["api"]["updateStatsUser"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->updateStatsUser: {}", _updateStatsUser);
	auto updateStatsCryptedPassword = JsonPath(&configurationRoot)["api"]["updateStatsCryptedPassword"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->updateStatsCryptedPassword: {}", updateStatsCryptedPassword);
	try
	{
		_updateStatsPassword = Encrypt::opensslDecrypt(updateStatsCryptedPassword);
	}
	catch (std::exception& e)
	{
		_updateStatsPassword = "";

		LOG_ERROR("Encrypt::opensslDecrypt failed"
			", updateStatsCryptedPassword: {}"
			", exception: {}", updateStatsCryptedPassword, e.what()
			);
	}
	auto sEncoderKey = JsonPath(&configurationRoot)["ffmpeg"]["encoderKey"].as<std::string>();
	LOG_INFO("Configuration item"
		", ffmpeg->encoderKey: {}", sEncoderKey);
	try
	{
		_encoderKey = std::stoi(sEncoderKey);
	}
	catch (std::exception& e)
	{
		_encoderKey = -1;

		LOG_ERROR("stoi failed"
			", sEncoderKey: {}"
			", exception: {}", sEncoderKey, e.what()
			);
	}
}

void EncoderBandwidthUsageThread::newBandwidthUsageAvailable(uint64_t& txAvgBandwidthUsage, uint64_t& rxAvgBandwidthUsage) const
{
	if (_encoderKey < 0)
	{
		LOG_ERROR("The 'encoderKey' configuration item is not valid and bandwidth stats is not sent to API MMS Server"
			", encoderKey: {}", _encoderKey);
		return;
	}
	if (_updateStatsPassword.empty())
	{
		LOG_ERROR("The 'updateBandwidthStatsPassword' configuration item is not valid and bandwidth stats is not sent to API MMS Server"
			", _updateStatsPassword: {}", _updateStatsPassword);
		return;
	}

	const std::string mmsAPIUpdateBandwidthStatsURL = std::format("{}://{}:{}/catramms/{}/encoder/{}{}/{}/{}",
		_mmsAPIProtocol, _mmsAPIHostname, _mmsAPIPort, _mmsAPIVersion, _encoderKey, _mmsAPIUpdateBandwidthStatsURI,
		txAvgBandwidthUsage, rxAvgBandwidthUsage);

	constexpr int32_t mmsAPITimeoutInSeconds = 3;
	LOG_INFO("UpdateBandwidthStats"
		", txAvgBandwidthUsage: {}"
		", rxAvgBandwidthUsage: {}",
		txAvgBandwidthUsage, rxAvgBandwidthUsage
		);
	constexpr std::vector<std::string> otherHeaders;
	nlohmann::json encoderResponse = CurlWrapper::httpPutStringAndGetJson(
		mmsAPIUpdateBandwidthStatsURL, mmsAPITimeoutInSeconds,
		CurlWrapper::basicAuthorization(_updateStatsUser, _updateStatsPassword),
		"", "application/json", // contentType
		otherHeaders, ""
	);
}
