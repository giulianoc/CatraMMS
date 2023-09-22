
#include <iostream>
#include <fstream>
#include "FFMpeg.h"
#include "spdlog/sinks/stdout_color_sinks.h"

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

    auto logger = spdlog::stdout_color_mt("ffmpeg_getLiveStreamingInfo");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

	string liveURL = pArgv[2];

	try
	{
		/*
		FFMpeg ffmpeg(configuration, logger);
		int64_t ingestionJobKey = 10;
		cout << "probeChannel: " << ffmpeg.probeChannel(ingestionJobKey, liveURL) << endl;
		*/

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
			0,	// encodingJobKey,
			videoTracks,
			audioTracks
		);

		for(tuple<int, string, string, string, string, int, int> videoTrack: videoTracks)
		{
			int videoProgramId;
			string videoStreamId;
			string videoStreamDescription;
			string videoCodec;
			string videoYUV;
			int videoWidth;
			int videoHeight;

			tie(videoProgramId, videoStreamId, videoStreamDescription,
				videoCodec, videoYUV, videoWidth, videoHeight) = videoTrack;

			cout << endl;
			cout << "videoProgramId: " << videoProgramId << endl;
			cout << "videoStreamId: " << videoStreamId << endl;
			cout << "videoStreamDescription: " << videoStreamDescription << endl;
			cout << "videoCodec: " << videoCodec << endl;
			cout << "videoYUV: " << videoYUV << endl;
			cout << "videoWidth: " << videoWidth << endl;
			cout << "videoHeight: " << videoHeight << endl;
		}

		for (tuple<int, string, string, string, int, bool> audioTrack: audioTracks)
		{
			int audioProgramId;
			string audioStreamId;
			string audioStreamDescription;
			string audioCodec;
			int audioSamplingRate;
			bool audioStereo;

			tie(audioProgramId, audioStreamId, audioStreamDescription,
				audioCodec, audioSamplingRate, audioStereo) = audioTrack;

			cout << endl;
			cout << "audioProgramId: " << audioProgramId << endl;
			cout << "audioStreamId: " << audioStreamId << endl;
			cout << "audioStreamDescription: " << audioStreamDescription << endl;
			cout << "audioCodec: " << audioCodec << endl;
			cout << "audioSamplingRate: " << audioSamplingRate << endl;
			cout << "audioStereo: " << audioStereo << endl;
		}

		cout << endl;
	}
	catch(exception& e)
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

