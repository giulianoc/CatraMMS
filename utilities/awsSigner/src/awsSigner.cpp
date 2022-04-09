
#include <fstream>
#include <iostream>
#include "json/json.h"
#include "AWSSigner.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"


using namespace std;

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 2)
    {
        std::cerr << "Usage: " << pArgv[0] << " config-path-name" << endl;

        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);

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

Json::Value loadConfigurationFile(const char* configurationPathName)
{
    Json::Value configurationJson;
    
    try
    {
        ifstream configurationFile(configurationPathName, std::ifstream::binary);
        configurationFile >> configurationJson;
    }
    catch(...)
    {
        cerr << string("wrong json configuration format")
                + ", configurationPathName: " + configurationPathName
            << endl;
    }
    
    return configurationJson;
}
