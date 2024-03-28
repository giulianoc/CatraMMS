
#include <fstream>
#include <iostream>
#include "nlohmann/json.hpp"
#include "AWSSigner.h"
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <regex>

using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;


json loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

	int64_t mediaItemKey = 1234;
	int64_t physicalPathKey = 5678;
	string httpBody = "dkjh lkjsh skjh al ${aaaa} dslkjhdlkjhasd";
	std::cout << "httpBody 1: " << httpBody << endl;
	httpBody = regex_replace(httpBody, regex("mediaItemKey"), to_string(mediaItemKey));
	std::cout << "httpBody 2: " << httpBody << endl;
	httpBody = regex_replace(httpBody, regex("\\$\\{aaaa\\}"),
		to_string(physicalPathKey));
	std::cout << "httpBody 3: " << httpBody << endl;

    if (iArgc != 2)
    {
        std::cerr << "Usage: " << pArgv[0] << " config-path-name" << endl;

        return 1;
    }
    
    json configuration = loadConfigurationFile(pArgv[1]);

    auto logger = spdlog::stdout_color_mt("awsSigner");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

	for(long i = 0; i < 10000000; i++)
	{
    try
    {
		string hostName = "d3ao8qf3jbneud.cloudfront.net";
		string uriPath = "2/000/038/302/423540_29822_28/423540_29822.m3u8";
		string keyPairId = "APKAUYWFOBAADUMU4IGK";
		string privateKeyPEMPathName
			= "/opt/catramms/CatraMMS/conf/pk-APKAUYWFOBAADUMU4IGK.pem";
		int expirationInSeconds = 60 * 60;

        AWSSigner awsSigner(logger);
        string signedURL = awsSigner.calculateSignedURL(
			hostName,
			uriPath,
			keyPairId,
			privateKeyPEMPathName,
			expirationInSeconds
		);
		logger->info(__FILEREF__
			+ "signedURL (" + to_string(i) + "): " + signedURL);
    }
    catch(...)
    {
		logger->error(__FILEREF__ + "awsSigner.mio_awsV4Signature2 failed");
    }
    }

    logger->info(__FILEREF__ + "Shutdown done");

	return 0;
}

json loadConfigurationFile(const char* configurationPathName)
{
    try
    {
        ifstream configurationFile(configurationPathName, ifstream::binary);

		return json::parse(configurationFile,
			nullptr,	// callback
			true,		// allow exceptions
			true		// ignore_comments
		);
    }
    catch(...)
    {
		string errorMessage = fmt::format("wrong json configuration format"
			", configurationPathName: {}", configurationPathName
		);

		throw runtime_error(errorMessage);
    }
}
