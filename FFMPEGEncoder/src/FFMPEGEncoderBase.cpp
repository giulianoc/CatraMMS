
#include "FFMPEGEncoderBase.h"
#include "JSONUtils.h"
#include "JsonPath.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

FFMPEGEncoderBase::FFMPEGEncoderBase(json configurationRoot)
{
	try
	{
		/*
		_mmsAPIVODDeliveryURI = JsonPath(&_configurationRoot)["api"]["vodDeliveryURI"].as<string>();
		LOG_INFO(string() + "Configuration item" + ", api->vodDeliveryURI: " + _mmsAPIVODDeliveryURI);
		_mmsAPITimeoutInSeconds = JsonPath(&_configurationRoot)["api"]["timeoutInSeconds"].as<int32_t>(120);
		LOG_INFO(string() + "Configuration item" + ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds));
		*/

		const auto mmsAPIProtocol = JsonPath(&configurationRoot)["api"]["protocol"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->protocol: " + mmsAPIProtocol);
		const auto mmsAPIHostname = JsonPath(&configurationRoot)["api"]["hostname"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->hostname: " + mmsAPIHostname);
		const auto mmsAPIPort = JsonPath(&configurationRoot)["api"]["port"].as<int32_t>(0);
		LOG_TRACE(string() + "Configuration item" + ", api->port: " + to_string(mmsAPIPort));
		const auto mmsAPIVersion = JsonPath(&configurationRoot)["api"]["version"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->version: " + mmsAPIVersion);
		const auto mmsAPIIngestionURI = JsonPath(&configurationRoot)["api"]["ingestionURI"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->ingestionURI: " + mmsAPIIngestionURI);
		auto mmsAPIWorkflowURI = JsonPath(&configurationRoot)["api"]["workflowURI"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->workflowURI: " + mmsAPIWorkflowURI);
		auto mmsBinaryProtocol = JsonPath(&configurationRoot)["api"]["binary"]["protocol"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->binary->protocol: " + mmsBinaryProtocol);
		auto mmsBinaryHostname = JsonPath(&configurationRoot)["api"]["binary"]["hostname"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->binary->hostname: " + mmsBinaryHostname);
		auto mmsBinaryPort = JsonPath(&configurationRoot)["api"]["binary"]["port"].as<int32_t>(0);
		LOG_TRACE(string() + "Configuration item" + ", api->binary->port: " + to_string(mmsBinaryPort));
		auto mmsBinaryVersion = JsonPath(&configurationRoot)["api"]["binary"]["version"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->binary->version: " + mmsBinaryVersion);
		auto mmsBinaryIngestionURI = JsonPath(&configurationRoot)["api"]["binary"]["ingestionURI"].as<string>();
		LOG_TRACE(string() + "Configuration item" + ", api->binary->ingestionURI: " + mmsBinaryIngestionURI);

		_mmsWorkflowIngestionURL = std::format("{}://{}:{}/catramms/{}/{}",
			mmsAPIProtocol, mmsAPIHostname, mmsAPIPort, mmsAPIVersion, mmsAPIWorkflowURI);

		_mmsIngestionURL = std::format("{}://{}:{}/catramms/{}{}",
			mmsAPIProtocol, mmsAPIHostname, mmsAPIPort, mmsAPIVersion, mmsAPIIngestionURI);

		_mmsBinaryIngestionURL = std::format("{}://{}:{}/catramms/{}{}",
			mmsBinaryProtocol, mmsBinaryHostname, mmsBinaryPort, mmsBinaryVersion, mmsBinaryIngestionURI);

		_mmsAPITimeoutInSeconds = JsonPath(&configurationRoot)["api"]["timeoutInSeconds"].as<int32_t>(120);
		LOG_TRACE(
			"Configuration item"
			", api->timeoutInSeconds: {}",
			_mmsAPITimeoutInSeconds
		);
		_mmsBinaryTimeoutInSeconds = JsonPath(&configurationRoot)["api"]["binary"]["timeoutInSeconds"].as<int32_t>(120);
		LOG_TRACE(
			"Configuration item"
			", api->binary->timeoutInSeconds: {}",
			_mmsBinaryTimeoutInSeconds
		);
	}
	catch (exception &e)
	{
		// error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

FFMPEGEncoderBase::~FFMPEGEncoderBase()
{
	try
	{
	}
	catch (runtime_error &e)
	{
		// error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch (exception &e)
	{
		// error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

long FFMPEGEncoderBase::getAddContentIngestionJobKey(int64_t ingestionJobKey, string ingestionResponse)
{
	try
	{
		int64_t addContentIngestionJobKey;

		/*
		{
			"tasks" :
			[
				{
					"ingestionJobKey" : 10793,
					"label" : "Add Content test",
					"type" : "Add-Content"
				},
				{
					"ingestionJobKey" : 10794,
					"label" : "Frame Containing Face: test",
					"type" : "Face-Recognition"
				},
				...
			],
			"workflow" :
			{
				"ingestionRootKey" : 831,
				"label" : "ingestContent test"
			}
		}
		*/
		json ingestionResponseRoot = JSONUtils::toJson<json>(ingestionResponse);

		string field = "tasks";
		if (!JSONUtils::isPresent(ingestionResponseRoot, field))
		{
			string errorMessage = std::format(
				"ingestion workflow. Response Body json is not well format"
				", ingestionJobKey: {}"
				", ingestionResponse: {}",
				ingestionJobKey, ingestionResponse
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		json tasksRoot = ingestionResponseRoot[field];

		for (int taskIndex = 0; taskIndex < tasksRoot.size(); taskIndex++)
		{
			json ingestionJobRoot = tasksRoot[taskIndex];

			field = "type";
			if (!JSONUtils::isPresent(ingestionJobRoot, field))
			{
				string errorMessage = std::format(
					"ingestion workflow. Response Body json is not well format"
					", ingestionJobKey: {}"
					", ingestionResponse: {}",
					ingestionJobKey, ingestionResponse
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string type = JSONUtils::asString(ingestionJobRoot, field, "");

			if (type == "Add-Content")
			{
				field = "ingestionJobKey";
				if (!JSONUtils::isPresent(ingestionJobRoot, field))
				{
					string errorMessage = std::format(
						"ingestion workflow. Response Body json is not well format"
						", ingestionJobKey: {}"
						", ingestionResponse: {}",
						ingestionJobKey, ingestionResponse
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				addContentIngestionJobKey = JSONUtils::asInt64(ingestionJobRoot, field, -1);

				break;
			}
		}

		return addContentIngestionJobKey;
	}
	catch (...)
	{
		string errorMessage = std::format(
			"ingestion workflow. Response Body json is not well format"
			", ingestionJobKey: {}"
			", ingestionResponse: {}",
			ingestionJobKey, ingestionResponse
		);
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}
