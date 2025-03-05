/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Validator.cpp
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#include "EMailSender.h"
#include "JSONUtils.h"
#include "catralibraries/Encrypt.h"
#include "spdlog/spdlog.h"
#include <curl/curl.h>
#include <deque>
#include <sstream>

EMailSender::EMailSender(json configuration) { _configuration = configuration; }

EMailSender::~EMailSender() {}

void EMailSender::sendEmail(string tosCommaSeparated, string subject, vector<string> &emailBody, bool useMMSCCToo)
{
	// see: https://everything.curl.dev/usingcurl/smtp
	// curl --ssl-reqd --url 'smtps://smtppro.zoho.eu:465' --mail-from 'info@catramms-cloud.com' --mail-rcpt 'giulianocatrambone@gmail.com'
	// --upload-file ./email.txt --user 'info@catramms-cloud.com:<write here the password>'

	// string emailServerURL = "smtp://smtp.gmail.com:587";
	string emailProtocol = JSONUtils::asString(_configuration["EmailNotification"], "protocol", "");
	string emailServer = JSONUtils::asString(_configuration["EmailNotification"], "server", "");
	int emailPort = JSONUtils::asInt(_configuration["EmailNotification"], "port", 0);
	string userName = JSONUtils::asString(_configuration["EmailNotification"], "userName", "");
	string password;
	{
		string encryptedPassword = JSONUtils::asString(_configuration["EmailNotification"], "password", "");
		password = Encrypt::opensslDecrypt(encryptedPassword);
	}
	string from = JSONUtils::asString(_configuration["EmailNotification"], "from", "");
	// string to = "giulianoc@catrasoftware.it";
	string cc;

	if (useMMSCCToo)
		cc = JSONUtils::asString(_configuration["EmailNotification"], "cc", "");

	string emailServerURL = emailProtocol + "://" + emailServer + ":" + to_string(emailPort);

	CURL *curl;
	CURLcode res = CURLE_OK;
	struct curl_slist *recipients = NULL;
	deque<string> emailLines;

	emailLines.push_back(string("From: <") + from + ">" + "\r\n");

	string sTosForEmail;
	{
		stringstream ssTosCommaSeparated(tosCommaSeparated);
		string sTo;
		char delim = ',';
		while (getline(ssTosCommaSeparated, sTo, delim))
		{
			if (!sTo.empty())
			{
				if (sTosForEmail == "")
					sTosForEmail = string("<") + sTo + ">";
				else
					sTosForEmail += (string(", <") + sTo + ">");
			}
		}
	}
	emailLines.push_back(string("To: ") + sTosForEmail + "\r\n");
	// emailLines.push_back(string("To: <") + to + ">" + "\r\n");

	if (cc != "")
	{
		emailLines.push_back(string("Cc: <") + cc + ">" + "\r\n");
	}

	emailLines.push_back(string("Subject: ") + subject + "\r\n");
	emailLines.push_back(string("Content-Type: text/html; charset=\"UTF-8\"") + "\r\n");
	emailLines.push_back("\r\n"); // empty line to divide headers from body, see RFC5322
	emailLines.insert(emailLines.end(), emailBody.begin(), emailBody.end());

	curl = curl_easy_init();

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, emailServerURL.c_str());
		if (userName != "")
			curl_easy_setopt(curl, CURLOPT_USERNAME, userName.c_str());
		if (password != "")
			curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());

		//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		/* Note that this option isn't strictly required, omitting it will result
		 * in libcurl sending the MAIL FROM command with empty sender data. All
		 * autoresponses should have an empty reverse-path, and should be directed
		 * to the address in the reverse-path which triggered them. Otherwise,
		 * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
		 * details.
		 */
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

		/* Add two recipients, in this particular case they correspond to the
		 * To: and Cc: addressees in the header, but they could be any kind of
		 * recipient. */
		{
			stringstream ssTosCommaSeparated(tosCommaSeparated);
			string sTo;
			char delim = ',';
			while (getline(ssTosCommaSeparated, sTo, delim))
			{
				if (!sTo.empty())
				{
					recipients = curl_slist_append(recipients, sTo.c_str());
				}
			}
		}
		// recipients = curl_slist_append(recipients, to.c_str());

		if (cc != "")
			recipients = curl_slist_append(recipients, cc.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

		curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

		/* We're using a callback function to specify the payload (the headers and
		 * body of the message). You could just use the CURLOPT_READDATA option to
		 * specify a FILE pointer to read from. */
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, EMailSender::emailPayloadFeed);
		curl_easy_setopt(curl, CURLOPT_READDATA, &emailLines);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

		/* Send the message */
		SPDLOG_INFO(
			"Sending email"
			", emailServerURL: {}"
			", userName: {}"
			", password: {}"
			", from: {}"
			", to: {}"
			", cc: {}"
			", subject: {}",
			emailServerURL, userName, password, from, tosCommaSeparated, cc, subject
		);
		res = curl_easy_perform(curl);

		/* Check for errors */
		if (res != CURLE_OK)
			SPDLOG_ERROR(
				"curl_easy_perform() failed"
				", curl_easy_strerror(res): {}",
				curl_easy_strerror(res)
			);
		else
			SPDLOG_INFO("Email sent successful");

		/* Free the list of recipients */
		curl_slist_free_all(recipients);

		/* curl won't send the QUIT command until you call cleanup, so you should
		 * be able to re-use this connection for additional messages (setting
		 * CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and calling
		 * curl_easy_perform() again. It may not be a good idea to keep the
		 * connection open for a very long time though (more than a few minutes
		 * may result in the server timing out the connection), and you do want to
		 * clean up in the end.
		 */
		curl_easy_cleanup(curl);
	}
}

size_t EMailSender::emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp)
{
	deque<string> *pEmailLines = (deque<string> *)userp;

	if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1))
	{
		return 0;
	}

	if (pEmailLines->size() == 0)
		return 0; // no more lines

	string emailLine = pEmailLines->front();
	// cout << "emailLine: " << emailLine << endl;

	memcpy(ptr, emailLine.c_str(), emailLine.length());
	pEmailLines->pop_front();

	return emailLine.length();
}
