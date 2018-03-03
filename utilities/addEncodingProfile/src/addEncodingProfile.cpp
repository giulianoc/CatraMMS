
#include <fstream>
#include "MMSEngineDBFacade.h"
#include "MMSEngine.h"

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 2)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name" << endl;
        
        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);

    auto logger = spdlog::stdout_logger_mt("encodingEngine");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, logger);

    shared_ptr<Customer> customer = mmsEngineDBFacade->getCustomer("Warner");
    
    logger->info(__FILEREF__ + "Creating MMSEngine"
            );
    shared_ptr<MMSEngine>       mmsEngine = make_shared<MMSEngine>(mmsEngineDBFacade, logger);

    mmsEngine->addFFMPEGVideoEncodingProfile(
        customer,
        "",                         // string encodingProfileSet,  // "": default Customer family, != "": named customer family
        MMSEngineDBFacade::EncodingTechnology::MP4,
	"",                         // string label,

	"mp4",                      // string fileFormat,
        
        "libx264",                  // string videoCodec,
        "high",                     // string videoProfile,
        -1,                         // int videoWidth,
        480,                        // int videoHeight,
        "500k",                     // string videoBitRate,
        true,                       // twoPasses
        "500k",                     // string videoMaxRate,
        "1000k",                    // string videoBufSize,
        25,                         // int videoFrameRate,
        5,                          // string videoKeyFrameIntervalInSeconds,

        "libfdk_aac",               // string audioCodec,
        "128k"                     // string audioBitRate
    );

    mmsEngine->addImageEncodingProfile(
        customer,
        "",                         // string encodingProfileSet,  // "": default Customer family, != "": named customer family
	"",                         // string label,

	"PNG",                      // string format,
        
        352,                         // int width,
        240,                        // int height,
        true,                     // bool aspectRatio,  Aspect is true the proportion are NOT maintained
                                                        // if Aspect is false the proportion are maintained, the width is fixed and the height will be calculated
        "NoInterlace"                // interlaceType: NoInterlace, LineInterlace, PlaneInterlace, PartitionInterlace
    );

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
