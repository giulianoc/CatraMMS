
#include "MMSEngineDBFacade.h"
#include "MMSEngine.h"


int main (int iArgc, char *pArgv [])
{

    auto logger = spdlog::stdout_logger_mt("encodingEngine");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    size_t dbPoolSize = 5;
    string dbServer ("tcp://127.0.0.1:3306");
    #ifdef __APPLE__
        string dbUsername("root"); string dbPassword("giuliano"); string dbName("workKing");
    #else
        string dbUsername("root"); string dbPassword("root"); string dbName("catracms");
    #endif
    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
        + ", dbPoolSize: " + to_string(dbPoolSize)
        + ", dbServer: " + dbServer
        + ", dbUsername: " + dbUsername
        + ", dbPassword: " + dbPassword
        + ", dbName: " + dbName
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName, logger);

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
