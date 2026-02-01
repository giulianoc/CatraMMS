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


#include "DeliveryServerCPUUsageThread.h"

#include "CurlWrapper.h"
#include "Encrypt.h"
#include "MMSEngineDBFacade.h"
#include "StringUtils.h"

using namespace std;

DeliveryServerCPUUsageThread::DeliveryServerCPUUsageThread(const json& configurationRoot, int16_t cpuStatsUpdateIntervalInSeconds,
	const bool isDeliveryAndAPIServerTogether,
	const shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade,
	const std::shared_ptr<spdlog::logger>& logger):
	CPUUsageThread(cpuStatsUpdateIntervalInSeconds, logger),
	_isDeliveryAndAPIServerTogether(isDeliveryAndAPIServerTogether), _mmsEngineDBFacade(mmsEngineDBFacade)
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
	_mmsAPIUpdateCPUStatsURI = JsonPath(&configurationRoot)["api"]["updateCPUStatsURI"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->updateCPUStatsURI: {}", _mmsAPIUpdateCPUStatsURI);
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
	auto sDeliveryServerKey = JsonPath(&configurationRoot)["api"]["delivery"]["deliveryServerKey"].as<std::string>();
	LOG_INFO("Configuration item"
		", api->delivery->deliveryServerKey: {}", sDeliveryServerKey);
	try
	{
		_deliveryServerKey = StringUtils::toNumber<int64_t>(sDeliveryServerKey);
	}
	catch (std::exception& e)
	{
		_deliveryServerKey = -1;

		LOG_ERROR("StringUtils::toNumber failed"
			", sDeliveryServerKey: {}"
			", exception: {}", sDeliveryServerKey, e.what()
			);
	}
}

void DeliveryServerCPUUsageThread::newCPUUsageAvailable(uint16_t& cpuUsage) const
{
	LOG_INFO("Sending CPU usage to MMS API Server"
		", _deliveryServerKey: {}"
		", _isDeliveryAndAPIServerTogether: {}"
		", cpuUsage: {}", _deliveryServerKey, _isDeliveryAndAPIServerTogether,
		cpuUsage
	);
	if (_deliveryServerKey < 0)
	{
		LOG_ERROR("The 'deliveryServerKey' configuration item is not valid and CPU stats is not sent to API MMS Server"
			", deliveryServerKey: {}", _deliveryServerKey);
		return;
	}
	if (_updateStatsPassword.empty())
	{
		LOG_ERROR("The 'updateCPUStatsPassword' configuration item is not valid and CPU stats is not sent to API MMS Server"
			", _updateStatsPassword: {}", _updateStatsPassword);
		return;
	}

	if (_isDeliveryAndAPIServerTogether)
		_mmsEngineDBFacade->updateDeliveryServerCPUUsage(_deliveryServerKey, cpuUsage);
	else
	{
		const std::string mmsAPIUpdateCPUStatsURL = std::format("{}://{}:{}/catramms/{}/deliveryServer/{}{}/{}",
			_mmsAPIProtocol, _mmsAPIHostname, _mmsAPIPort, _mmsAPIVersion, _deliveryServerKey, _mmsAPIUpdateCPUStatsURI,
			cpuUsage);

		constexpr int32_t mmsAPITimeoutInSeconds = 3;
		LOG_INFO("UpdateCPUStats"
			", cpuUsage: {}",
			cpuUsage
			);
		constexpr std::vector<std::string> otherHeaders;
		nlohmann::json apiResponseRoot = CurlWrapper::httpPutStringAndGetJson(
			mmsAPIUpdateCPUStatsURL, mmsAPITimeoutInSeconds,
			CurlWrapper::basicAuthorization(_updateStatsUser, _updateStatsPassword),
			"", "application/json", // contentType
			otherHeaders, ""
		);
	}
}
