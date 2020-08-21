
#include <iostream>
#include <fstream>
#include "FFMpeg.h"

using namespace std;

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 3)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name liveURL" << endl;
        
        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);

    auto logger = spdlog::stdout_logger_mt("ffmpeg_getLiveStreamingInfo");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

	string liveURL = pArgv[2];

	try
	{
		vector<tuple<int, string, string, string, string, int, int>> videoTracks;
		vector<tuple<int, string, string, string, int, bool>> audioTracks;

		FFMpeg ffmpeg(configuration, logger);

		logger->info(__FILEREF__ + "ffmpeg.getLiveStreamingInfo"
			+ ", liveURL: " + liveURL
		);
		ffmpeg.getLiveStreamingInfo(
			liveURL,
			"",	// userAgent
			0,	// ingestionJobKey,
			videoTracks,
			audioTracks
		);
	}
	catch(exception e)
	{
		logger->error(__FILEREF__ + "ffmpeg.getLiveStreamingInfo failed");

		return 1;
	}

    logger->info(__FILEREF__ + "Shutdown done"
            );

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

