
#include <fstream>
#include <iostream>
#include "MMSCURL.h"
#include "JSONUtils.h"
#include "catralibraries/Encrypt.h"
#include "spdlog/sinks/stdout_color_sinks.h"


using namespace std;

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 4)
    {
        std::cerr << "Usage: " << pArgv[0] << " config-path-name destination-email-addresses (comma separated) cc-email-addresses (comma separated)" << endl;
        
        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);
    string tosCommaSeparated = pArgv[2];
    string ccsCommaSeparated = pArgv[3];

    auto logger = spdlog::stdout_color_mt("sendEmail");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    try
    {
        logger->info(__FILEREF__ + "Sending email to " + tosCommaSeparated
                );
        
		string emailProviderURL = JSONUtils::asString(configuration["EmailNotification"], "providerURL", "");
		string emailUserName = JSONUtils::asString(configuration["EmailNotification"], "userName", "");

		string emailPassword;
		{
			string encryptedPassword = JSONUtils::asString(configuration["EmailNotification"], "password", "");
			emailPassword = Encrypt::opensslDecrypt(encryptedPassword);        
		}
	
		string subject = "Test Email";

        vector<string> emailBody;
        emailBody.push_back("<p>Dear lucia,</p>");
        emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;the registration has been done successfully</p>");
        emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Here follows the user key <b>4</b> and the confirmation code <b>202871650</b> to be used to confirm the registration</p>");
        emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"https://mms-gui-test.catramms-cloud.com/catramms/login.xhtml?confirmationRequested=true&confirmationUserKey=4&confirmationCode=202871650\">here</a> to confirm the registration</b></p>");
        emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
        emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

		MMSCURL::sendEmail(
			logger,
			emailProviderURL,	// i.e.: smtps://smtppro.zoho.eu:465
			emailUserName,	// i.e.: info@catramms-cloud.com
			tosCommaSeparated,
			ccsCommaSeparated,
			subject,
			emailBody,
			emailPassword
		);
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
