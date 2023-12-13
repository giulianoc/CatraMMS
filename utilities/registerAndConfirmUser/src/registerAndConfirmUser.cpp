
#include <fstream>
#include "EMailSender.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/Convert.h"
#include "spdlog/sinks/stdout_color_sinks.h"

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 6)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name name email password workspace-name" << endl;
        
        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);

    auto logger = spdlog::stdout_color_mt("registerAndConfirmUser");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, 3, 3, logger);

    /*
    logger->info(__FILEREF__ + "Creating MMSEngine"
            );
    shared_ptr<MMSEngine>       mmsEngine = make_shared<MMSEngine>(mmsEngineDBFacade, logger);
     */

	string name = pArgv[2];
	string email = pArgv[3];
	string password = pArgv[4];
	string workspaceName = pArgv[5];
	string country;
	MMSEngineDBFacade::EncodingPriority encodingPriority = MMSEngineDBFacade::EncodingPriority::Low;
	MMSEngineDBFacade::EncodingPeriod encodingPeriod = MMSEngineDBFacade::EncodingPeriod::Daily;
	int maxIngestionsNumber = 10000;
	int maxStorageInMB = 1000;

	tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode;
	int64_t userKey;
	int64_t workspaceKey;
	try
	{
		logger->info(__FILEREF__ + "Registering User and Workspace..."
			+ ", name: " + name
			+ ", email: " + email
			+ ", password: " + password
			+ ", workspaceName: " + workspaceName
			+ ", country: " + country
			+ ", encodingPriority: " + MMSEngineDBFacade::toString(encodingPriority)
			+ ", encodingPeriod: " + MMSEngineDBFacade::toString(encodingPeriod)
			+ ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
			+ ", maxStorageInMB: " + to_string(maxStorageInMB)
		);
		workspaceKeyUserKeyAndConfirmationCode =
			mmsEngineDBFacade->registerUserAndAddWorkspace(
				name,
				email,
				password,
				country,
				workspaceName,
				MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,  // MMSEngineDBFacade::WorkspaceType workspaceType
				"",                             // string deliveryURL,
				encodingPriority,               //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
				encodingPeriod,                 //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
				maxIngestionsNumber,            // long maxIngestionsNumber,
				maxStorageInMB,                 // long maxStorageInMB,
				"",                             // string languageCode,
				chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
		);

		workspaceKey = get<0>(workspaceKeyUserKeyAndConfirmationCode);
		userKey = get<1>(workspaceKeyUserKeyAndConfirmationCode);

		logger->info(__FILEREF__ + "Registered User and added Workspace"
			+ ", workspaceName: " + workspaceName
			+ ", email: " + email
			+ ", userKey: " + to_string(userKey)
			+ ", confirmationCode: " + get<2>(workspaceKeyUserKeyAndConfirmationCode)
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "mmsEngineDBFacade->registerUserAndAddWorkspace failed");

		return 1;
	}

	try
	{
		tuple<string,string,string> apiKeyNameAndEmailAddress
			= mmsEngineDBFacade->confirmRegistration(
				get<2>(workspaceKeyUserKeyAndConfirmationCode), 30);

		string apiKey;
		string name;
		string emailAddress;

		tie(apiKey, name, emailAddress) = apiKeyNameAndEmailAddress;

		logger->info(__FILEREF__ + "User confirmed"
			+ ", workspaceName: " + workspaceName
			+ ", email: " + email
			+ ", userKey: " + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode))
			+ ", confirmationCode: " + get<2>(workspaceKeyUserKeyAndConfirmationCode)
		);

		// mmsEngineDBFacade->setWorkspaceAsDefault (userKey, workspaceKey,
		//		workspaceKeyToBeSetAsDefault);

		{
			string to = email;
			string subject = "MMS User creation";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Hi ") + name + ",</p>");
			emailBody.push_back(string("<p>the registration has been done successfully, user and default Workspace have been created</p>"));
			emailBody.push_back(string("<p>here follows the user key <b>") + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode)) 
				+ "</b>, email <b>" + email + "</b> and the password <b>" + password + "</b></p>");
			emailBody.push_back(
				string("<p>Please make sure to change the password once login is done</p>"));
			emailBody.push_back("<p>Have a nice day, best regards</p>");
			emailBody.push_back("<p>MMS technical support</p>");

			EMailSender emailSender(logger, configuration);
			bool useMMSCCToo = true;
			emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
		}
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "mmsEngineDBFacade->confirmRegistration failed");

		return 1;
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
