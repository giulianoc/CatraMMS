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

EncodersLoadBalancer::EncodersLoadBalancer(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, json configuration, shared_ptr<spdlog::logger> logger)
{
	_logger = logger;
	_mmsEngineDBFacade = mmsEngineDBFacade;

	/*
	json encodersPools = configuration["ffmpeg"]["hosts"];

	for (auto const& encodersPoolName : encodersPools.getMemberNames())
	{
		_logger->info(__FILEREF__ + "encodersPools"
			+ ", encodersPoolName: " + encodersPoolName
		);

		EncodersPoolDetails encodersPoolDetails;

		encodersPoolDetails._lastEncoderUsed = -1;

		// encodersPool will be "common" or "workspaceKey"
		json encodersPool = encodersPools[encodersPoolName];

		for (int encoderPoolIndex = 0; encoderPoolIndex < encodersPool.size(); encoderPoolIndex++)
		{
			string encoderHostName = encodersPool[encoderPoolIndex].asString();

			_logger->info(__FILEREF__ + "encodersPool"
				+ ", encoderHostName: " + encoderHostName
			);

			encodersPoolDetails._encoders.push_back(encoderHostName);
		}

		_encodersPools[encodersPoolName] = encodersPoolDetails;
	}
	*/
}

EncodersLoadBalancer::~EncodersLoadBalancer() {}

/*
string EncodersLoadBalancer::getEncoderHost(string encodersPool, shared_ptr<Workspace> workspace,
	string transcoderToSkip)
{
	string defaultEncodersPool = "common";

	map<string, EncodersPoolDetails>::iterator it = _encodersPools.end();
	// Priority 1: encodersPool
	if (encodersPool != "")
		it = _encodersPools.find(encodersPool);
	// Priority 2: workspace
	if (it == _encodersPools.end())
		it = _encodersPools.find(workspace->_directoryName);
	// Priority 3: default encoders pool (common)
	if (it == _encodersPools.end())
		it = _encodersPools.find(defaultEncodersPool);

	if (it == _encodersPools.end())
	{
		string errorMessage = "No encoders pools found";
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	it->second._lastEncoderUsed     = (it->second._lastEncoderUsed + 1) % it->second._encoders.size();
	if (transcoderToSkip != "" && transcoderToSkip == it->second._encoders[it->second._lastEncoderUsed])
		it->second._lastEncoderUsed     = (it->second._lastEncoderUsed + 1) % it->second._encoders.size();

	return it->second._encoders[it->second._lastEncoderUsed];
}
*/

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
		string errorMessage = fmt::format(
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
