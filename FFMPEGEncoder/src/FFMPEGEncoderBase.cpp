
#include "FFMPEGEncoderBase.h"
#include "JSONUtils.h"
#include "spdlog/spdlog.h"

FFMPEGEncoderBase::FFMPEGEncoderBase(json configurationRoot)
{
	try
	{
		_mmsAPITimeoutInSeconds = JSONUtils::asInt32(configurationRoot["api"], "timeoutInSeconds", 120);
		SPDLOG_INFO(
			"Configuration item"
			", api->timeoutInSeconds: {}",
			_mmsAPITimeoutInSeconds
		);
		_mmsBinaryTimeoutInSeconds = JSONUtils::asInt32(configurationRoot["api"]["binary"], "timeoutInSeconds", 120);
		SPDLOG_INFO(
			"Configuration item"
			", api->binary->timeoutInSeconds: {}",
			_mmsBinaryTimeoutInSeconds
		);
	}
	catch (runtime_error &e)
	{
		// error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
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
		json ingestionResponseRoot = JSONUtils::toJson(ingestionResponse);

		string field = "tasks";
		if (!JSONUtils::isPresent(ingestionResponseRoot, field))
		{
			string errorMessage = std::format(
				"ingestion workflow. Response Body json is not well format"
				", ingestionJobKey: {}"
				", ingestionResponse: {}",
				ingestionJobKey, ingestionResponse
			);
			SPDLOG_ERROR(errorMessage);

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
				SPDLOG_ERROR(errorMessage);

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
					SPDLOG_ERROR(errorMessage);

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
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}
