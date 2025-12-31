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
using ordered_json = nlohmann::ordered_json;

EncodersLoadBalancer::EncodersLoadBalancer(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, json configuration)
{
	_mmsEngineDBFacade = mmsEngineDBFacade;
}

EncodersLoadBalancer::~EncodersLoadBalancer() {}

tuple<int64_t, string, bool> EncodersLoadBalancer::getEncoderURL(
	int64_t ingestionJobKey, string encodersPoolLabel, shared_ptr<Workspace> workspace, int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed
)
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

		string encoderURL;
		if (externalEncoder)
			encoderURL = protocol + "://" + publicServerName + ":" + to_string(port);
		else
			encoderURL = protocol + "://" + internalServerName + ":" + to_string(port);

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
	catch (EncoderNotFound &e)
	{
		SPDLOG_ERROR(
			"getEncoderURL failed"
			", ingestionJobKey: {}"
			", workspaceKey: {}"
			", encodersPoolLabel: {}"
			", encoderKeyToBeSkipped: {}"
			", e.what(): {}",
			ingestionJobKey, workspace->_workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getEncoderURL failed"
			", ingestionJobKey: {}"
			", workspaceKey: {}"
			", encodersPoolLabel: {}"
			", encoderKeyToBeSkipped: {}"
			", e.what(): {}",
			ingestionJobKey, workspace->_workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped, e.what()
		);

		throw e;
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

		throw runtime_error(errorMessage);
	}
}
