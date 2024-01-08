
#include <fstream>
// #include "EMailSender.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/Convert.h"
#include "spdlog/sinks/stdout_color_sinks.h"

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 5)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name userName userEmailAddress defaultWorkspaceKeys" << endl;
        
        return 1;
    }
    
    Json::Value configuration = loadConfigurationFile(pArgv[1]);

    auto logger = spdlog::stdout_color_mt("registerActiveDirectoryUser");
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

	string userName = pArgv[2];
	string userEmailAddress = pArgv[3];
	string userCountry;
	string defaultWorkspaceKeys = pArgv[4];

	bool createRemoveWorkspace = true;
	bool ingestWorkflow = true;
	bool createProfiles = true;
	bool deliveryAuthorization = true;
	bool shareWorkspace = true;
	bool editMedia = true;
    bool editConfiguration = true;
	bool killEncoding = true;
	bool cancelIngestionJob = true;
	bool editEncodersPool = true;
    bool applicationRecorder = true;

	tuple<int64_t,string> registrationDetails;
	int64_t userKey;
	string apiKey;
	try
	{
		logger->info(__FILEREF__ + "Registering Active Directory User..."
			+ ", userName: " + userName
			+ ", userEmailAddress: " + userEmailAddress
			+ ", userCountry: " + userCountry
			+ ", defaultWorkspaceKeys: " + defaultWorkspaceKeys
			+ ", createRemoveWorkspace: " + to_string(createRemoveWorkspace)
			+ ", ingestWorkflow: " + to_string(ingestWorkflow)
			+ ", createProfiles: " + to_string(createProfiles)
			+ ", deliveryAuthorization: " + to_string(deliveryAuthorization)
			+ ", shareWorkspace: " + to_string(shareWorkspace)
			+ ", editMedia: " + to_string(editMedia)
			+ ", editConfiguration: " + to_string(editConfiguration)
			+ ", killEncoding: " + to_string(killEncoding)
			+ ", cancelIngestionJob: " + to_string(cancelIngestionJob)
			+ ", editEncodersPool: " + to_string(editEncodersPool)
			+ ", applicationRecorder: " + to_string(applicationRecorder)
		);
		registrationDetails =
			mmsEngineDBFacade->registerActiveDirectoryUser(
				userName,
				userEmailAddress,
				userCountry,
				"",
				createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
				shareWorkspace, editMedia,
				editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
				applicationRecorder,
				defaultWorkspaceKeys, 30,
				chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
		);

		userKey = get<0>(registrationDetails);
		apiKey = get<1>(registrationDetails);

		logger->info(__FILEREF__ + "Registered Active Directory User"
			+ ", userKey: " + to_string(userKey)
			+ ", apiKey: " + apiKey
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "mmsEngineDBFacade->registerActiveDirectoryUser failed");

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
