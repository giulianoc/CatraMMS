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


#include "DeliveryServerBandwidthUsageThread.h"

#include "CurlWrapper.h"
#include "Encrypt.h"
#include "MMSEngineDBFacade.h"
#include "StringUtils.h"

using namespace std;

DeliveryServerBandwidthUsageThread::DeliveryServerBandwidthUsageThread(const json & configurationRoot,
	const std::optional<std::string> &interfaceNameToMonitor,
	const bool isDeliveryAndAPIServerTogether,
	const shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade,
	const std::shared_ptr<spdlog::logger>& logger):
	BandwidthUsageThread(interfaceNameToMonitor, logger),
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

void DeliveryServerBandwidthUsageThread::newBandwidthUsageAvailable(uint64_t& txAvgBandwidthUsage, uint64_t& rxAvgBandwidthUsage) const
{
	LOG_INFO("Sending bandwidth usage stats to MMS API Server"
		", _deliveryServerKey: {}"
		", _isDeliveryAndAPIServerTogether: {}"
		", txAvgBandwidthUsage: {}"
		", rxAvgBandwidthUsage: {}", _deliveryServerKey, _isDeliveryAndAPIServerTogether,
		txAvgBandwidthUsage, rxAvgBandwidthUsage
	);

	if (_deliveryServerKey < 0)
	{
		LOG_ERROR("The 'deliveryServerKey' configuration item is not valid and bandwidth stats is not sent to API MMS Server"
			", deliveryServerKey: {}", _deliveryServerKey);
		return;
	}
	if (_updateStatsPassword.empty())
	{
		LOG_ERROR("The 'updateBandwidthStatsPassword' configuration item is not valid and bandwidth stats is not sent to API MMS Server"
			", _updateStatsPassword: {}", _updateStatsPassword);
		return;
	}

	if (_isDeliveryAndAPIServerTogether)
		_mmsEngineDBFacade->updateDeliveryServerAvgBandwidthUsage(_deliveryServerKey, txAvgBandwidthUsage, rxAvgBandwidthUsage);
	else
	{
		const std::string mmsAPIUpdateBandwidthStatsURL = std::format("{}://{}:{}/catramms/{}/deliveryServer/{}{}/{}/{}",
			_mmsAPIProtocol, _mmsAPIHostname, _mmsAPIPort, _mmsAPIVersion, _deliveryServerKey, _mmsAPIUpdateBandwidthStatsURI,
			txAvgBandwidthUsage, rxAvgBandwidthUsage);

		constexpr int32_t mmsAPITimeoutInSeconds = 3;
		LOG_INFO("UpdateBandwidthStats"
			", txAvgBandwidthUsage: {}"
			", rxAvgBandwidthUsage: {}",
			txAvgBandwidthUsage, rxAvgBandwidthUsage
			);
		constexpr std::vector<std::string> otherHeaders;
		nlohmann::json apiResponseRoot = CurlWrapper::httpPutStringAndGetJson(
			mmsAPIUpdateBandwidthStatsURL, mmsAPITimeoutInSeconds,
			CurlWrapper::basicAuthorization(_updateStatsUser, _updateStatsPassword),
			"", "application/json", // contentType
			otherHeaders, ""
		);
	}
}
