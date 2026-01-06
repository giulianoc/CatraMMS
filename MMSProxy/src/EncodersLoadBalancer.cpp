/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   EncodersLoadBalancer.cpp
 * Author: giuliano
 *
 * Created on April 28, 2018, 2:33 PM
 */

#include "EncodersLoadBalancer.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

EncodersLoadBalancer::EncodersLoadBalancer(const shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade,
	const json& configuration): _mmsEngineDBFacade(mmsEngineDBFacade)
{
}

EncodersLoadBalancer::~EncodersLoadBalancer() = default;

tuple<int64_t, string, bool> EncodersLoadBalancer::getEncoderURL(
	int64_t ingestionJobKey, string encodersPoolLabel, const shared_ptr<Workspace>& workspace, int64_t encoderKeyToBeSkipped,
	bool externalEncoderAllowed
) const
{
	SPDLOG_INFO(
		"Received getEncoderURL"
		", ingestionJobKey: {}"
		", workspaceKey: {}"
		", encodersPoolLabel: {}"
		", encoderKeyToBeSkipped: {}",
		ingestionJobKey, workspace->_workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped
	);

	try
	{
		auto [encoderKey, externalEncoder, protocol, publicServerName, internalServerName, port] =
			_mmsEngineDBFacade->getRunningEncoderByEncodersPool(
				workspace->_workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped, externalEncoderAllowed
			);

		string encoderURL = std::format("{}://{}:{}", protocol,
			externalEncoder ? publicServerName : internalServerName, port);

		SPDLOG_INFO(
			"getEncoderURL"
			", ingestionJobKey: {}"
			", workspaceKey: {}"
			", encodersPoolLabel: {}"
			", encoderKeyToBeSkipped: {}"
			", encoderKey: {}"
			", encoderURL: {}",
			ingestionJobKey, workspace->_workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped, encoderKey, encoderURL
		);

		return make_tuple(encoderKey, encoderURL, externalEncoder);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"getEncoderURL failed"
			", ingestionJobKey: {}"
			", workspaceKey: {}"
			", encodersPoolLabel: {}"
			", encoderKeyToBeSkipped: {}"
			", e.what(): {}",
			ingestionJobKey, workspace->_workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw;
	}
}
