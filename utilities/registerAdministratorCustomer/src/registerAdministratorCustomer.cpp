
#include "CMSEngineDBFacade.h"
#include "CMSEngine.h"


int main (int iArgc, char *pArgv [])
{

    auto logger = spdlog::stdout_logger_mt("registerAdministratorCustomer");
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

    string emailAddress = "giulianoc@catrasoftware.it";
    logger->info(__FILEREF__ + "Creating Administrator Customer"
            );
    pair<int64_t,string> customerKeyAndConfirmationCode =
            cmsEngine->registerCustomer(
                "Admin",                       // string customerName,
                "",                             // string street,
                "",                             // string city,
                "",                             // string state,
                "",                             // string zip,
                "",                             // string phone,
                "",                             // string countryCode,
                CMSEngineDBFacade::CustomerType::IngestionAndDelivery,  // CMSEngineDBFacade::CustomerType customerType
                "",                             // string deliveryURL,
                CMSEngineDBFacade::EncodingPriority::High,   //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                CMSEngineDBFacade::EncodingPeriod::Daily,       //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
                100,                            // long maxIngestionsNumber,
                100,                            // long maxStorageInGB,
                "",                             // string languageCode,
                "admin",                        // string userName,
                "admin_2018",                   // string userPassword,
                emailAddress,                   // string userEmailAddress,
                chrono::system_clock::now() + chrono::hours(24 * 365 * 20)     // chrono::system_clock::time_point userExpirationDate
    );

    logger->info(__FILEREF__ + "Confirm Customer"
            );
    cmsEngine->confirmCustomer(customerKeyAndConfirmationCode.second);
    
    string flags = "('ADMIN_API,USER_API')";
    logger->info(__FILEREF__ + "Create APIKey"
            );
    string apiKey = cmsEngine->createAPIKey(
            emailAddress, 
            flags,
            chrono::system_clock::now() + chrono::hours(24 * 365 * 20)  // apiKeyExpirationDate
    );
    
    cout << "Administrator APIKey: " << apiKey << endl;

    logger->info(__FILEREF__ + "Shutdown done"
            );

    return 0;
}
