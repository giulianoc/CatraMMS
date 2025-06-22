
#include "CurlWrapper.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <fstream>
#include <iostream>
#include <regex>

using namespace std;

json loadConfigurationFile(string configurationPathName, string environmentPrefix);
string applyEnvironmentToConfiguration(string configuration, string environmentPrefix);

int main(int iArgc, char *pArgv[])
{

	if (iArgc != 5)
	{
		std::cerr << "Usage: " << pArgv[0] << " config-path-name destination-email-addresses (comma separated) subjext <body path file name>" << endl;

		return 1;
	}

	json configuration = loadConfigurationFile(pArgv[1], "MMS_");
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

		CurlWrapper::sendEmail(
			emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
			emailUserName,	  // i.e.: support@catramms-cloud.com
			emailPassword, emailUserName, tosCommaSeparated,
			emailUserName, // cc
			subject, emailBody, "text/html; charset=\"UTF-8\""
		);
	}
	catch (...)
	{
		logger->error(__FILEREF__ + "emailSender.sendEmail failed");
	}

	logger->info(__FILEREF__ + "Shutdown done");

	return 0;
}

json loadConfigurationFile(string configurationPathName, string environmentPrefix)
{

#ifdef BOOTSERVICE_DEBUG_LOG
	ofstream of("/tmp/bootservice.log", ofstream::app);
	of << "loadConfigurationFile..." << endl;
#endif

	string sConfigurationFile;
	{
		ifstream configurationFile(configurationPathName, ifstream::binary);
		stringstream buffer;
		buffer << configurationFile.rdbuf();
		if (environmentPrefix == "")
			sConfigurationFile = buffer.str();
		else
			sConfigurationFile = applyEnvironmentToConfiguration(buffer.str(), environmentPrefix);
	}

	json configurationRoot = json::parse(
		sConfigurationFile,
		nullptr, // callback
		true,	 // allow exceptions
		true	 // ignore_comments
	);

	return configurationRoot;
}

string applyEnvironmentToConfiguration(string configuration, string environmentPrefix)
{
	char **s = environ;

#ifdef BOOTSERVICE_DEBUG_LOG
	ofstream of("/tmp/bootservice.log", ofstream::app);
#endif

	int envNumber = 0;
	for (; *s; s++)
	{
		string envVariable = *s;
#ifdef BOOTSERVICE_DEBUG_LOG
//					of << "ENV " << *s << endl;
#endif
		if (envVariable.starts_with(environmentPrefix))
		{
			size_t endOfVarName = envVariable.find("=");
			if (endOfVarName == string::npos)
				continue;

			envNumber++;

			// sarebbe \$\{ZORAC_SOLR_PWD\}
			string envLabel = std::format("\\$\\{{{}\\}}", envVariable.substr(0, endOfVarName));
			string envValue = envVariable.substr(endOfVarName + 1);
#ifdef BOOTSERVICE_DEBUG_LOG
			of << "ENV " << envLabel << ": " << envValue << endl;
#endif
			configuration = regex_replace(configuration, regex(envLabel), envValue);
		}
	}

	return configuration;
}
