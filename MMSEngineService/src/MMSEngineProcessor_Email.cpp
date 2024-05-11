
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineProcessor.h"
/*
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "FFMpeg.h"
#include "GEOInfoTimes.h"
#include "PersistenceLock.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

void MMSEngineProcessor::emailNotificationThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "emailNotificationThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		string sParameters = JSONUtils::toString(parametersRoot);

		SPDLOG_INFO(
			string() + "emailNotificationThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count()) +
			", dependencies.size: " + to_string(dependencies.size()) + ", sParameters: " + sParameters
		);

		string sDependencies;
		{
			for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
			{
				try
				{
					int64_t key;
					MMSEngineDBFacade::ContentType referenceContentType;
					Validator::DependencyType dependencyType;
					bool stopIfReferenceProcessingError;

					tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

					if (dependencyType == Validator::DependencyType::MediaItemKey)
					{
						bool warningIfMissing = false;

						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						MMSEngineDBFacade::ContentType contentType;
						string title;
						string userData;
						string ingestionDate;
						int64_t localIngestionJobKey;
						tie(contentType, title, userData, ingestionDate, ignore, localIngestionJobKey) = mediaItemDetails;

						sDependencies += string("MediaItemKey") + ", mediaItemKey: " + to_string(key) + ", title: " + title + ". ";
					}
					else if (dependencyType == Validator::DependencyType::PhysicalPathKey)
					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						int64_t mediaItemKey;
						string title;
						MMSEngineDBFacade::ContentType localContentType;
						string userData;
						string ingestionDate;
						int64_t localIngestionJobKey;
						tie(mediaItemKey, localContentType, title, userData, ingestionDate, localIngestionJobKey, ignore, ignore, ignore) =
							mediaItemDetails;

						sDependencies += string("PhysicalPathKey") + ", physicalPathKey: " + to_string(key) + ", title: " + title + ". ";
					}
					else // if (dependencyType ==
						 // Validator::DependencyType::IngestionJobKey)
					{
						bool warningIfMissing = false;
						tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
							_mmsEngineDBFacade->getIngestionJobDetails(
								workspace->_workspaceKey, key,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						string label;
						MMSEngineDBFacade::IngestionType ingestionType;
						MMSEngineDBFacade::IngestionStatus ingestionStatus;
						string metaDataContent;
						string errorMessage;

						tie(label, ingestionType, ingestionStatus, metaDataContent, errorMessage) = ingestionJobDetails;

						sDependencies += string("<br>IngestionJob") + ", dependencyType: " + to_string(static_cast<int>(dependencyType)) +
										 ", ingestionJobKey: " + to_string(key) + ", label: " + label + ". ";
					}
				}
				catch (...)
				{
					string errorMessage = string("Exception processing dependencies") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey);
					SPDLOG_ERROR(string() + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}

		string sReferencies;
		string checkStreaming_streamingName;
		string checkStreaming_streamingUrl;
		{
			string field = "references";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json referencesRoot = parametersRoot[field];
				for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
				{
					try
					{
						json referenceRoot = referencesRoot[referenceIndex];
						field = "ingestionJobKey";
						if (JSONUtils::isMetadataPresent(referenceRoot, field))
						{
							int64_t referenceIngestionJobKey = JSONUtils::asInt64(referenceRoot, field, 0);

							string referenceLabel;
							MMSEngineDBFacade::IngestionType ingestionType;
							string parameters;
							string referenceErrorMessage;

							tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
								_mmsEngineDBFacade->getIngestionJobDetails(
									workspace->_workspaceKey, referenceIngestionJobKey,
									// 2022-12-18: MIK potrebbe essere stato
									// appena aggiunto
									true
								);
							tie(referenceLabel, ingestionType, ignore, parameters, referenceErrorMessage) = ingestionJobDetails;

							sReferencies += string("<br>IngestionJob") + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType) +
											", ingestionJobKey: " + to_string(referenceIngestionJobKey) + ", label: " + referenceLabel +
											", errorMessage: " + referenceErrorMessage + ". ";

							if (ingestionType == MMSEngineDBFacade::IngestionType::CheckStreaming)
							{
								json parametersRoot = JSONUtils::toJson(parameters);

								string inputType;
								field = "inputType";
								inputType = JSONUtils::asString(parametersRoot, field, "");

								if (inputType == "Channel")
								{
									field = "channelConfigurationLabel";
									if (JSONUtils::isMetadataPresent(parametersRoot, field))
									{
										checkStreaming_streamingName = JSONUtils::asString(parametersRoot, field, "");

										bool warningIfMissing = false;
										tuple<
											int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int,
											int, int64_t>
											channelDetails = _mmsEngineDBFacade->getStreamDetails(
												workspace->_workspaceKey, checkStreaming_streamingName, warningIfMissing
											);

										string streamSourceType;
										tie(ignore, streamSourceType, ignore, checkStreaming_streamingUrl, ignore, ignore, ignore, ignore, ignore,
											ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = channelDetails;
									}
								}
								else
								{
									field = "streamingName";
									if (JSONUtils::isMetadataPresent(parametersRoot, field))
										checkStreaming_streamingName = JSONUtils::asString(parametersRoot, field, "");
									field = "streamingUrl";
									if (JSONUtils::isMetadataPresent(parametersRoot, field))
										checkStreaming_streamingUrl = JSONUtils::asString(parametersRoot, field, "");
								}
							}
						}
					}
					catch (...)
					{
						string errorMessage = string("Exception processing referencies") +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey);
						SPDLOG_ERROR(string() + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}

		string field = "configurationLabel";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string configurationLabel = JSONUtils::asString(parametersRoot, field, "");

		string tosCommaSeparated;
		string subject;
		string message;
		tuple<string, string, string> email = _mmsEngineDBFacade->getEMailByConfigurationLabel(workspace->_workspaceKey, configurationLabel);
		tie(tosCommaSeparated, subject, message) = email;

		field = "UserSubstitutions";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			json userSubstitutionsRoot = parametersRoot[field];

			for (int userSubstitutionIndex = 0; userSubstitutionIndex < userSubstitutionsRoot.size(); userSubstitutionIndex++)
			{
				json userSubstitutionRoot = userSubstitutionsRoot[userSubstitutionIndex];

				field = "ToBeReplaced";
				if (!JSONUtils::isMetadataPresent(userSubstitutionRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
					_logger->warn(errorMessage);

					continue;
				}
				string strToBeReplaced = JSONUtils::asString(userSubstitutionRoot, field, "");

				field = "ReplaceWith";
				if (!JSONUtils::isMetadataPresent(userSubstitutionRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
					_logger->warn(errorMessage);

					continue;
				}
				string strToReplace = JSONUtils::asString(userSubstitutionRoot, field, "");

				SPDLOG_INFO(
					string() + "User substitution" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", strToBeReplaced: " + strToBeReplaced + ", strToReplace: " + strToReplace
				);
				if (strToBeReplaced != "")
				{
					while (subject.find(strToBeReplaced) != string::npos)
						subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
					while (message.find(strToBeReplaced) != string::npos)
						message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
				}
			}
		}
		else
		{
			SPDLOG_INFO(
				"NO User substitution"
				", ingestionJobKey: {}"
				", _processorIdentifier: {}",
				ingestionJobKey, _processorIdentifier
			);
		}

		{
			string strToBeReplaced = "${Dependencies}";
			string strToReplace = sDependencies;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}
		{
			string strToBeReplaced = "${Referencies}";
			string strToReplace = sReferencies;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}
		{
			string strToBeReplaced = "${CheckStreaming_streamingName}";
			string strToReplace = checkStreaming_streamingName;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}
		{
			string strToBeReplaced = "${CheckStreaming_streamingUrl}";
			string strToReplace = checkStreaming_streamingUrl;
			while (subject.find(strToBeReplaced) != string::npos)
				subject.replace(subject.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
			while (message.find(strToBeReplaced) != string::npos)
				message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}

		vector<string> emailBody;
		emailBody.push_back(message);

		SPDLOG_INFO(
			"Sending email..."
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", _emailProviderURL: {}"
			", _emailUserName: {}"
			", subject: {}"
			", emailBody: {}"
			// ", _emailPassword: {}"
			,
			_processorIdentifier, ingestionJobKey, _emailProviderURL, _emailUserName, subject,
			message //, _emailPassword
		);
		MMSCURL::sendEmail(
			_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
			_emailUserName,	   // i.e.: info@catramms-cloud.com
			tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, _emailPassword
		);
		// EMailSender emailSender(_logger, _configuration);
		// bool useMMSCCToo = false;
		// emailSender.sendEmail(emailAddresses, subject, emailBody,
		// useMMSCCToo);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "sendEmail failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "sendEmail failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}
