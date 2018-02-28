
#include "MMSEngineDBFacade.h"
#include "MMSEngine.h"
#include "catralibraries/Convert.h"


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
    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
        + ", dbPoolSize: " + to_string(dbPoolSize)
        + ", dbServer: " + dbServer
        + ", dbUsername: " + dbUsername
        + ", dbPassword: " + dbPassword
        + ", dbName: " + dbName
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName, logger);

    logger->info(__FILEREF__ + "Creating MMSEngine"
            );
    shared_ptr<MMSEngine>       mmsEngine = make_shared<MMSEngine>(mmsEngineDBFacade, logger);

    string emailAddress = "giulianoc@catrasoftware.it";
    logger->info(__FILEREF__ + "Creating Administrator Customer"
            );
    tuple<int64_t,int64_t,string> customerKeyUserKeyAndConfirmationCode =
            mmsEngine->registerCustomer(
                "Admin",                       // string customerName,
                "",                             // string street,
                "",                             // string city,
                "",                             // string state,
                "",                             // string zip,
                "",                             // string phone,
                "",                             // string countryCode,
                MMSEngineDBFacade::CustomerType::IngestionAndDelivery,  // MMSEngineDBFacade::CustomerType customerType
                "",                             // string deliveryURL,
                MMSEngineDBFacade::EncodingPriority::High,   //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                MMSEngineDBFacade::EncodingPeriod::Daily,       //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
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
    mmsEngine->confirmCustomer(get<2>(customerKeyUserKeyAndConfirmationCode));
    
    bool adminAPI = true;
    bool userAPI = true;
    logger->info(__FILEREF__ + "Create APIKey"
            );
    string apiKey = mmsEngine->createAPIKey(
            get<0>(customerKeyUserKeyAndConfirmationCode),
            get<1>(customerKeyUserKeyAndConfirmationCode),
            adminAPI,
            userAPI,
            chrono::system_clock::now() + chrono::hours(24 * 365 * 20)  // apiKeyExpirationDate
    );
    
    cout << "Username (CustomerKey): " + to_string(get<0>(customerKeyUserKeyAndConfirmationCode)) << endl;
    cout << "Password (Administrator APIKey): " << apiKey << endl;

    logger->info(__FILEREF__ + "Shutdown done"
            );

    return 0;
}
