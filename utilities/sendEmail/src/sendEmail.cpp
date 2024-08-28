
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "catralibraries/Encrypt.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <fstream>
#include <iostream>

using namespace std;

json loadConfigurationFile(const char *configurationPathName);

int main(int iArgc, char *pArgv[])
{

	if (iArgc != 5)
	{
		std::cerr << "Usage: " << pArgv[0] << " config-path-name destination-email-addresses (comma separated) subjext <body path file name>" << endl;

		return 1;
	}

	json configuration = loadConfigurationFile(pArgv[1]);
	string tosCommaSeparated = pArgv[2];
	string subject = pArgv[3];
	string bodyPathFileName = pArgv[4];

	auto logger = spdlog::stdout_color_mt("sendEmail");
	spdlog::set_level(spdlog::level::trace);
	// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
	// spdlog::register_logger(logger);

	try
	{
		logger->info(__FILEREF__ + "Sending email to " + tosCommaSeparated);

		string emailProviderURL = JSONUtils::asString(configuration["EmailNotification"], "providerURL", "");
		string emailUserName = JSONUtils::asString(configuration["EmailNotification"], "userName", "");

		string emailPassword;
		{
			string encryptedPassword = JSONUtils::asString(configuration["EmailNotification"], "password", "");
			emailPassword = Encrypt::opensslDecrypt(encryptedPassword);
		}

		// string subject = "Test Email";

		vector<string> emailBody;
		{
			ifstream bodyFile(bodyPathFileName);
			string line;
			while (getline(bodyFile, line))
				emailBody.push_back(line);
		}

		MMSCURL::sendEmail(
			emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
			emailUserName,	  // i.e.: support@catramms-cloud.com
			tosCommaSeparated, "", subject, emailBody, emailPassword
		);
	}
	catch (...)
	{
		logger->error(__FILEREF__ + "emailSender.sendEmail failed");
	}

	logger->info(__FILEREF__ + "Shutdown done");

	return 0;
}

json loadConfigurationFile(const char *configurationPathName)
{
	try
	{
		ifstream configurationFile(configurationPathName, ifstream::binary);

		return json::parse(
			configurationFile,
			nullptr, // callback
			true,	 // allow exceptions
			true	 // ignore_comments
		);
	}
	catch (...)
	{
		string errorMessage = fmt::format(
			"wrong json configuration format"
			", configurationPathName: {}",
			configurationPathName
		);

		throw runtime_error(errorMessage);
	}
}
