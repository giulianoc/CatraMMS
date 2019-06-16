
#include <fstream>
#include "MMSEngineDBFacade.h"
#include "catralibraries/Convert.h"

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 2)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name" << endl;
        
        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);

    auto logger = spdlog::stdout_logger_mt("registerAdministratorWorkspace");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, 3, logger);

    /*
    logger->info(__FILEREF__ + "Creating MMSEngine"
            );
    shared_ptr<MMSEngine>       mmsEngine = make_shared<MMSEngine>(mmsEngineDBFacade, logger);
     */
    string emailAddress = "giulianoc@catrasoftware.it";
    string workspaceName = "Admin";
    logger->info(__FILEREF__ + "Creating Administrator Workspace"
            );
    tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode;
    {
        string workspaceDirectoryName;

        workspaceDirectoryName.resize(workspaceName.size());

        transform(
            workspaceName.begin(), 
            workspaceName.end(), 
            workspaceDirectoryName.begin(), 
            [](unsigned char c){
                if (isalpha(c)) 
                    return c; 
                else 
                    return (unsigned char) '_'; } 
        );

        try
        {
            /*
            workspaceKeyUserKeyAndConfirmationCode = mmsEngineDBFacade->registerWorkspace(
                workspaceName, 
                workspaceDirectoryName,
                "",                             // string street,
                "",                             // string city,
                "",                             // string state,
                "",                             // string zip,
                "",                             // string phone,
                "",                             // string countryCode,
                MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,  // MMSEngineDBFacade::WorkspaceType workspaceType
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
             */
        }
        catch(exception e)
        {
            logger->error(__FILEREF__ + "mmsEngineDBFacade->registerWorkspace failed");
            
            return 1;
        }
    }
        /*
    tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode =
            mmsEngine->registerWorkspace(
                "Admin",                       // string workspaceName,
                "",                             // string street,
                "",                             // string city,
                "",                             // string state,
                "",                             // string zip,
                "",                             // string phone,
                "",                             // string countryCode,
                MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,  // MMSEngineDBFacade::WorkspaceType workspaceType
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

    logger->info(__FILEREF__ + "Confirm Workspace"
            );
    mmsEngineDBFacade->confirmWorkspace(get<2>(workspaceKeyUserKeyAndConfirmationCode));
    // mmsEngine->confirmWorkspace(get<2>(workspaceKeyUserKeyAndConfirmationCode));
    
    bool adminAPI = true;
    bool userAPI = true;
    logger->info(__FILEREF__ + "Create APIKey"
            );
    string apiKey = mmsEngineDBFacade->createAPIKey(
            get<0>(workspaceKeyUserKeyAndConfirmationCode), 
            get<1>(workspaceKeyUserKeyAndConfirmationCode),
            adminAPI,
            userAPI, 
            chrono::system_clock::now() + chrono::hours(24 * 365 * 20)  // apiKeyExpirationDate
    );
    
    cout << "Username (WorkspaceKey): " + to_string(get<0>(workspaceKeyUserKeyAndConfirmationCode)) << endl;
    cout << "Password (Administrator APIKey): " << apiKey << endl;
    */

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
