
#include <fstream>
#include "MMSEngineDBFacade.h"
#include "catralibraries/Convert.h"
#include "spdlog/sinks/stdout_color_sinks.h"

json loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 2)
    {
        cerr << "Usage: " << pArgv[0] << " config-path-name" << endl;
        
        return 1;
    }
    
    json configuration = loadConfigurationFile(pArgv[1]);

    auto logger = spdlog::stdout_color_mt("updateGEOInfo");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

	spdlog::set_default_logger(logger);

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, 3, 3, logger);

	try
	{
		mmsEngineDBFacade->updateGEOInfo();
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "mmsEngineDBFacade->updateGEOInfo failed");

		return 1;
	}

    logger->info(__FILEREF__ + "Shutdown done"
            );

    return 0;
}

json loadConfigurationFile(const char* configurationPathName)
{
    try
    {
        ifstream configurationFile(configurationPathName, ifstream::binary);

		return json::parse(configurationFile,
			nullptr,	// callback
			true,		// allow exceptions
			true		// ignore_comments
		);
    }
    catch(...)
    {
		string errorMessage = fmt::format("wrong json configuration format"
			", configurationPathName: {}", configurationPathName
		);

		throw runtime_error(errorMessage);
    }
}

