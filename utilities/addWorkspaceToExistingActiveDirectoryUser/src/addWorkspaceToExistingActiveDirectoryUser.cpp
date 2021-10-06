
#include <fstream>
#include "MMSEngineDBFacade.h"
#include "catralibraries/Convert.h"
#include "spdlog/sinks/stdout_color_sinks.h"

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 5)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name userKey userEmailAddress workspaceKey" << endl;
        
        return 1;
    }

    Json::Value configuration = loadConfigurationFile(pArgv[1]);
	int64_t userKey = stoll(pArgv[2]);
	string userEmailAddress = pArgv[3];
	int64_t workspaceKey = stoll(pArgv[4]);

    auto logger = spdlog::stdout_color_mt("addWorkspaceToExistingActiveDirectoryUser");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    logger->info(__FILEREF__ + "addWorkspaceToExistingActiveDirectoryUser"
			+ ", userKey: " + to_string(userKey)
			+ ", userEmailAddress: " + userEmailAddress
			+ ", workspaceKey: " + to_string(workspaceKey)
            );

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, 3, logger);

	{
		bool createRemoveWorkspace = false;
		bool ingestWorkflow = true;
		bool createProfiles = false;
		bool deliveryAuthorization = true;
		bool shareWorkspace = true;
		bool editMedia = true;
		bool editConfiguration = false;
		bool killEncoding = false;
		bool cancelIngestionJob = false;
		bool editEncodersPool = false;
		bool applicationRecorder = false;

		mmsEngineDBFacade->createAPIKeyForActiveDirectoryUser(
                userKey,
                userEmailAddress,
                createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
                shareWorkspace, editMedia,
                editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
				applicationRecorder,
                workspaceKey, 30);
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
