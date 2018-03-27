
#include <fstream>
#include "MMSEngineDBFacade.h"

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 6)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name customerName userName password emailAddress" << endl;
        
        return 1;
    }
    
    string configPathName = pArgv[1];
    string customerName = pArgv[2];
    string userName = pArgv[3];
    string password = pArgv[4];
    string emailAddress = pArgv[5];

    Json::Value configuration = loadConfigurationFile(configPathName.c_str());

    auto logger = spdlog::stdout_logger_mt("registerCustomer");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, logger);

    /*
    logger->info(__FILEREF__ + "Creating MMSEngine"
            );
    shared_ptr<MMSEngine>       mmsEngine = make_shared<MMSEngine>(mmsEngineDBFacade, logger);
     */
    
    /*
    mmsEngine->registerCustomer(
	"Warner",                       // string customerName,
	"",                             // string street,
        "",                             // string city,
        "",                             // string state,
	"",                             // string zip,
        "",                             // string phone,
        "",                             // string countryCode,
        MMSEngineDBFacade::CustomerType::EncodingOnly,  // MMSEngineDBFacade::CustomerType customerType
	"",                             // string deliveryURL,
        MMSEngineDBFacade::EncodingPriority::Medium,   //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        MMSEngineDBFacade::EncodingPeriod::Daily,       //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
	10,                             // long maxIngestionsNumber,
        10,                             // long maxStorageInGB,
	"",                             // string languageCode,
        "giuliano",                     // string userName,
        "giuliano",                     // string userPassword,
        "giulianoc@catrasoftware.it",   // string userEmailAddress,
        chrono::system_clock::now()     // chrono::system_clock::time_point userExpirationDate
    );
     */
    {
        // string customerName = "Warner";
        string customerDirectoryName;

        customerDirectoryName.resize(customerName.size());

        transform(
            customerName.begin(), 
            customerName.end(), 
            customerDirectoryName.begin(), 
            [](unsigned char c){
                if (isalpha(c)) 
                    return c; 
                else 
                    return (unsigned char) '_'; } 
        );

        try
        {
            mmsEngineDBFacade->registerCustomer(
                customerName, 
                customerDirectoryName,
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
                1000,                            // long maxIngestionsNumber,
                10,                            // long maxStorageInGB,
                "",                             // string languageCode,
                userName,                        // string userName,
                password,                   // string userPassword,
                emailAddress,                   // string userEmailAddress,
                chrono::system_clock::now() + chrono::hours(24 * 365 * 20)     // chrono::system_clock::time_point userExpirationDate
            );
        }
        catch(exception e)
        {
            logger->error(__FILEREF__ + "mmsEngineDBFacade->registerCustomer failed");
            
            return 1;
        }
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
