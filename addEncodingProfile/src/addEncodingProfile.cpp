
#include "CMSEngineDBFacade.h"
#include "CMSEngine.h"


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
    logger->info(__FILEREF__ + "Creating CMSEngineDBFacade"
        + ", dbPoolSize: " + to_string(dbPoolSize)
        + ", dbServer: " + dbServer
        + ", dbUsername: " + dbUsername
        + ", dbPassword: " + dbPassword
        + ", dbName: " + dbName
            );
    shared_ptr<CMSEngineDBFacade>       cmsEngineDBFacade = make_shared<CMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName, logger);

    shared_ptr<Customer> customer = cmsEngineDBFacade->getCustomer("Warner");
    
    logger->info(__FILEREF__ + "Creating CMSEngine"
            );
    shared_ptr<CMSEngine>       cmsEngine = make_shared<CMSEngine>(cmsEngineDBFacade, logger);

    cmsEngine->addFFMPEGVideoEncodingProfile(
        customer,
        "",                         // string encodingProfileSet,  // "": default Customer family, != "": named customer family
        CMSEngineDBFacade::EncodingTechnology::MP4,
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

    logger->info(__FILEREF__ + "Shutdown done"
            );
    
    return 0;
}
