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

#pragma once

#include "CPUUsageThread.h"
#include "JsonPath.h"

#include <nlohmann/json_fwd.hpp>

using json = nlohmann::json;

class DeliveryServerCPUUsageThread final : public CPUUsageThread
{
public:
	explicit DeliveryServerCPUUsageThread(const json & configurationRoot, int16_t cpuStatsUpdateIntervalInSeconds,
		const std::shared_ptr<spdlog::logger>& logger = nullptr);

	void newCPUUsageAvailable(uint16_t& cpuUsage) const override;

private:
	std::string _mmsAPIProtocol;
	std::string _mmsAPIHostname;
	int32_t _mmsAPIPort;
	std::string _mmsAPIVersion;
	std::string _mmsAPIUpdateCPUStatsURI;
	std::string _updateStatsUser;
	std::string _updateStatsPassword;
	int64_t _deliveryServerKey;
};
