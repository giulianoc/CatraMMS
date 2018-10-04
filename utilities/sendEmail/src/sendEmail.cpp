
#include <fstream>
#include <iostream>
#include "EMailSender.h"


using namespace std;

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 3)
    {
        std::cerr << "Usage: " << pArgv[0] << " config-path-name destination-email-address" << endl;
        
        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);
    string emailAddress = pArgv[2];

    auto logger = spdlog::stdout_logger_mt("sendEmail");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    try
    {
        logger->info(__FILEREF__ + "Sending email to " + emailAddress
                );
        
        vector<string> emailBody;
        emailBody.push_back("Test body");

        EMailSender emailSender(logger, configuration);
        emailSender.sendEmail(emailAddress, "Test subject", emailBody);
    }
    catch(...)
    {
        logger->error(__FILEREF__ + "emailSender.sendEmail failed");
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
