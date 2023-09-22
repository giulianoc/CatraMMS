
#include "FFMPEGEncoderBase.h"
#include "JSONUtils.h"
// #include "MMSCURL.h"
// #include <sstream>
// #include "catralibraries/Encrypt.h"
// #include "catralibraries/ProcessUtility.h"
// #include "catralibraries/StringUtils.h"

FFMPEGEncoderBase::FFMPEGEncoderBase(
	Json::Value configuration,
	shared_ptr<spdlog::logger> logger)
{
	try
	{
		_logger = logger;

		_mmsAPITimeoutInSeconds = JSONUtils::asInt(configuration["api"], "timeoutInSeconds", 120);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds)
		);
		_mmsBinaryTimeoutInSeconds = JSONUtils::asInt(configuration["api"]["binary"], "timeoutInSeconds", 120);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", api->binary->timeoutInSeconds: " + to_string(_mmsBinaryTimeoutInSeconds)
		);

	}
	catch(runtime_error& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

FFMPEGEncoderBase::~FFMPEGEncoderBase()
{
	try
	{
	}
	catch(runtime_error& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

long FFMPEGEncoderBase::getAddContentIngestionJobKey(
	int64_t ingestionJobKey,
	string ingestionResponse
)
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
        Json::Value ingestionResponseRoot = JSONUtils::toJson(
			ingestionJobKey, -1, ingestionResponse);

		string field = "tasks";
		if (!JSONUtils::isMetadataPresent(ingestionResponseRoot, field))
		{
			string errorMessage = __FILEREF__
				"ingestion workflow. Response Body json is not well format"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", ingestionResponse: " + ingestionResponse
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		Json::Value tasksRoot = ingestionResponseRoot[field];

		for(int taskIndex = 0; taskIndex < tasksRoot.size(); taskIndex++)
		{
			Json::Value ingestionJobRoot = tasksRoot[taskIndex];

			field = "type";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage = __FILEREF__
					"ingestion workflow. Response Body json is not well format"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ingestionResponse: " + ingestionResponse
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string type = JSONUtils::asString(ingestionJobRoot, field, "");

			if (type == "Add-Content")
			{
				field = "ingestionJobKey";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage = __FILEREF__
						"ingestion workflow. Response Body json is not well format"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ingestionResponse: " + ingestionResponse
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				addContentIngestionJobKey = JSONUtils::asInt64(ingestionJobRoot, field, -1);

				break;
			}
		}

		return addContentIngestionJobKey;
	}
	catch(...)
	{
		string errorMessage =
			string("ingestion workflow. Response Body json is not well format")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ingestionResponse: " + ingestionResponse
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

