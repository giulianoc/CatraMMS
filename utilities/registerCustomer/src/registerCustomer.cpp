
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

    logger->info(__FILEREF__ + "Creating CMSEngine"
            );
    shared_ptr<CMSEngine>       cmsEngine = make_shared<CMSEngine>(cmsEngineDBFacade, logger);

    cmsEngine->registerCustomer(
	"Warner",                       // string customerName,
	"",                             // string street,
        "",                             // string city,
        "",                             // string state,
	"",                             // string zip,
        "",                             // string phone,
        "",                             // string countryCode,
        CMSEngineDBFacade::CustomerType::EncodingOnly,  // CMSEngineDBFacade::CustomerType customerType
	"",                             // string deliveryURL,
        CMSEngineDBFacade::EncodingPriority::Medium,   //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        CMSEngineDBFacade::EncodingPeriod::Daily,       //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
	10,                             // long maxIngestionsNumber,
        10,                             // long maxStorageInGB,
	"",                             // string languageCode,
        "giuliano",                     // string userName,
        "giuliano",                     // string userPassword,
        "giulianoc@catrasoftware.it",   // string userEmailAddress,
        chrono::system_clock::now()     // chrono::system_clock::time_point userExpirationDate
    );

    logger->info(__FILEREF__ + "Shutdown done"
            );

    return 0;
}
