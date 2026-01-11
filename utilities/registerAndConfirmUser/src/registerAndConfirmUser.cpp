
// #include "Convert.h"
#include "CurlWrapper.h"
#include "EMailSender.h"
#include "Encrypt.h"
#include "JsonPath.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <fstream>

using namespace std;
using json = nlohmann::json;

string base64_decode(const string &in)
{
	string out;

	vector<int> T(256, -1);
	for (int i = 0; i < 64; i++)
		T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

	int val = 0, valb = -8;
	for (unsigned char c : in)
	{
		if (T[c] == -1)
			break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0)
		{
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

int main(const int iArgc, char *pArgv[])
{
	if (iArgc != 5)
	{
		std::cerr << "Usage: " << pArgv[0] << " name email password workspace-name" << endl;

		return 1;
	}

	auto logger = spdlog::stdout_color_mt("console");

	const char *configurationPathName = getenv("MMS_CONFIGPATHNAME");
	if (configurationPathName == nullptr)
	{
		cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;

		return 1;
	}
	auto configuration = JSONUtils::loadConfigurationFile<json>(configurationPathName, "MMS_");

	logger->info(__FILEREF__ + "Creating MMSEngineDBFacade");
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(configuration,
		JsonPath(&configuration)["log"]["api"]["slowQuery"].as<json>(), 2, 2, logger);

	string name = pArgv[1];
	string email = pArgv[2];
	string password = pArgv[3];
	string workspaceName = pArgv[4];

	int64_t userKey;
	string confirmationCode;
	try
	{
		auto encodingPriority = MMSEngineDBFacade::EncodingPriority::Low;
		auto encodingPeriod = MMSEngineDBFacade::EncodingPeriod::Daily;

		int maxStorageInMB = 1;
		int maxIngestionsNumber = 1;
		int64_t workspaceKey;

		SPDLOG_INFO(
			"Registering User and Workspace..."
			", name: {}"
			", email: {}"
			", password: {}"
			", workspaceName: {}",
			name, email, password, workspaceName
		);
		tie(workspaceKey, userKey, confirmationCode) = mmsEngineDBFacade->registerUserAndAddWorkspace(
			name, email, password, "", "", workspaceName, "",
			MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery, "", encodingPriority,
			encodingPeriod, maxIngestionsNumber, maxStorageInMB, "", "",
			chrono::system_clock::now() + chrono::hours(24 * 365 * 10) // userExpirationDate
		);

		SPDLOG_INFO("Registered User and added Workspace"
			", workspaceName: {}"
			", email: {}"
			", userKey: {}"
			", confirmationCode: {}",
			workspaceName, email, userKey, confirmationCode
		);
	}
	catch (exception &e)
	{
		logger->error(__FILEREF__ + "mmsEngineDBFacade->registerUserAndAddWorkspace failed");

		return 1;
	}

	try
	{
		auto[apiKey, name, emailAddress] = mmsEngineDBFacade->confirmRegistration(
			confirmationCode, 30);

		SPDLOG_INFO("User confirmed"
			", workspaceName: {}"
			", email: {}"
			", userKey: {}"
			", confirmationCode: {}",
			workspaceName, email, userKey, confirmationCode
		);

		{
			auto emailProviderURL = JsonPath(&configuration)["EmailNotification"]["providerURL"].as<string>();
			auto emailUserName = JsonPath(&configuration)["EmailNotification"]["userName"].as<string>();

			auto encryptedPassword = JsonPath(&configuration)["EmailNotification"]["password"].as<string>();
			string emailPassword = Encrypt::opensslDecrypt(encryptedPassword);

			string to = email;
			string subject = "MMS User creation";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Hi ") + name + ",</p>");
			emailBody.emplace_back("<p>the registration has been done successfully, user and default Workspace have been created</p>");
			emailBody.push_back(
				string("<p>here follows the user key <b>") + to_string(userKey) + "</b>, email <b>" + email +
				"</b> and the password <b>" + password + "</b></p>"
			);
			emailBody.emplace_back("<p>Please make sure to change the password once login is done</p>");
			emailBody.emplace_back("<p>Have a nice day, best regards</p>");
			emailBody.emplace_back("<p>MMS technical support</p>");

			CurlWrapper::sendEmail(
				emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				emailUserName,	   // i.e.: info@catramms-cloud.com
				emailPassword, emailUserName, to, "",
				subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			/*
			EMailSender emailSender(configuration);
			bool useMMSCCToo = true;
			emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
			*/
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR("mmsEngineDBFacade->confirmRegistration failed"
			", exception: {}", e.what()
			);

		return 1;
	}

	SPDLOG_INFO("Shutdown done");

	return 0;
}
