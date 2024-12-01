
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineProcessor.h"
#include <regex>
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
#include <sstream>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

void MMSEngineProcessor::httpCallbackThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "httpCallbackThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be notified (HTTP Callback)" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		bool addMediaData;
		string httpProtocol;
		string httpHostName;
		string userName;
		string password;
		int httpPort;
		string httpURI;
		string httpURLParameters;
		bool formData;
		string httpMethod;
		long callbackTimeoutInSeconds;
		int maxRetries;
		string httpBody;
		json httpHeadersRoot = json::array();
		{
			string field = "addMediaData";
			addMediaData = JSONUtils::asBool(parametersRoot, field, true);

			field = "protocol";
			httpProtocol = JSONUtils::asString(parametersRoot, field, "http");

			field = "userName";
			userName = JSONUtils::asString(parametersRoot, field, "");

			field = "password";
			password = JSONUtils::asString(parametersRoot, field, "");

			field = "hostName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			httpHostName = JSONUtils::asString(parametersRoot, field, "");

			field = "port";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				if (httpProtocol == "http")
					httpPort = 80;
				else
					httpPort = 443;
			}
			else
				httpPort = JSONUtils::asInt(parametersRoot, field, 0);

			field = "timeout";
			callbackTimeoutInSeconds = JSONUtils::asInt(parametersRoot, field, 120);

			field = "uri";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			httpURI = JSONUtils::asString(parametersRoot, field, "");

			field = "parameters";
			httpURLParameters = JSONUtils::asString(parametersRoot, field, "");

			field = "formData";
			formData = JSONUtils::asBool(parametersRoot, field, false);

			field = "method";
			httpMethod = JSONUtils::asString(parametersRoot, field, "POST");

			field = "httpBody";
			httpBody = JSONUtils::asString(parametersRoot, field, "");

			field = "headers";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				// semicolon as separator
				stringstream ss(JSONUtils::asString(parametersRoot, field, ""));
				string token;
				char delim = ';';
				while (getline(ss, token, delim))
				{
					if (!token.empty())
						httpHeadersRoot.push_back(token);
				}
				// httpHeadersRoot = parametersRoot[field];
			}

			field = "maxRetries";
			maxRetries = JSONUtils::asInt(parametersRoot, field, 1);
		}

		if (addMediaData && (httpMethod == "POST" || httpMethod == "PUT"))
		{
			if (httpBody != "")
			{
				SPDLOG_INFO(
					string() + "POST/PUT with httpBody" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size())
				);

				int dependencyIndex = 0;
				for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
				{
					bool stopIfReferenceProcessingError = false;

					try
					{
						int64_t key;
						Validator::DependencyType dependencyType;

						tie(key, ignore, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

						int64_t physicalPathKey;
						int64_t mediaItemKey;

						if (dependencyType == Validator::DependencyType::MediaItemKey)
						{
							mediaItemKey = key;

							int64_t encodingProfileKey = -1;

							bool warningIfMissing = false;
							tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
								key, encodingProfileKey, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);
							tie(physicalPathKey, ignore, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
						}
						else
						{
							physicalPathKey = key;

							{
								bool warningIfMissing = false;
								tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
									mediaItemDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
										workspace->_workspaceKey, key, warningIfMissing,
										// 2022-12-18: MIK potrebbe
										// essere stato appena aggiunto
										true
									);

								tie(mediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemDetails;
							}
						}

						httpBody = regex_replace(httpBody, regex("\\$\\{mediaItemKey\\}"), to_string(mediaItemKey));
						httpBody = regex_replace(httpBody, regex("\\$\\{physicalPathKey\\}"), to_string(physicalPathKey));

						SPDLOG_INFO(
							string() + "userHttpCallback" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", httpProtocol: " + httpProtocol +
							", httpHostName: " + httpHostName + ", httpURI: " + httpURI + ", httpURLParameters: " + httpURLParameters +
							", formData: " + to_string(formData) + ", httpMethod: " + httpMethod + ", httpBody: " + httpBody
						);

						userHttpCallback(
							ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, httpMethod,
							callbackTimeoutInSeconds, httpHeadersRoot, httpBody, userName, password, maxRetries
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", dependencyIndex: " + to_string(dependencyIndex) +
											  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}
					catch (exception e)
					{
						string errorMessage = fmt::format(
							"http callback failed"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", dependencyIndex: {}"
							", dependencies.size(): {}",
							_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size()
						);
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}

					dependencyIndex++;
				}
			}
			else
			{
				int dependencyIndex = 0;
				for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
				{
					bool stopIfReferenceProcessingError = false;

					try
					{
						int64_t key;
						MMSEngineDBFacade::ContentType referenceContentType;
						Validator::DependencyType dependencyType;

						tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

						json callbackMedatada;
						{
							callbackMedatada["workspaceKey"] = (int64_t)(workspace->_workspaceKey);

							MMSEngineDBFacade::ContentType contentType;
							int64_t physicalPathKey;
							int64_t mediaItemKey;

							if (dependencyType == Validator::DependencyType::MediaItemKey)
							{
								mediaItemKey = key;

								callbackMedatada["mediaItemKey"] = mediaItemKey;

								{
									bool warningIfMissing = false;
									tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
										contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
											workspace->_workspaceKey, mediaItemKey, warningIfMissing,
											// 2022-12-18: MIK potrebbe
											// essere stato appena
											// aggiunto
											true
										);

									string localTitle;
									string userData;
									tie(contentType, localTitle, userData, ignore, ignore, ignore) =
										contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;

									callbackMedatada["title"] = localTitle;

									if (userData == "")
										callbackMedatada["userData"] = nullptr;
									else
									{
										json userDataRoot = JSONUtils::toJson(userData);

										callbackMedatada["userData"] = userDataRoot;
									}
								}

								{
									int64_t encodingProfileKey = -1;
									bool warningIfMissing = false;
									tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails =
										_mmsStorage->getPhysicalPathDetails(
											key, encodingProfileKey, warningIfMissing,
											// 2022-12-18: MIK potrebbe
											// essere stato appena aggiunto
											true
										);

									string physicalPath;
									string fileName;
									int64_t sizeInBytes;
									string deliveryFileName;

									tie(physicalPathKey, physicalPath, ignore, ignore, fileName, ignore, ignore) = physicalPathDetails;

									callbackMedatada["physicalPathKey"] = physicalPathKey;
									callbackMedatada["fileName"] = fileName;
									// callbackMedatada["physicalPath"] =
									// physicalPath;
								}
							}
							else
							{
								physicalPathKey = key;

								callbackMedatada["physicalPathKey"] = physicalPathKey;

								{
									bool warningIfMissing = false;
									tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
										mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
											_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
												workspace->_workspaceKey, physicalPathKey, warningIfMissing,
												// 2022-12-18: MIK potrebbe
												// essere stato appena
												// aggiunto
												true
											);

									string localTitle;
									string userData;
									tie(mediaItemKey, contentType, localTitle, userData, ignore, ignore, ignore, ignore, ignore) =
										mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;

									callbackMedatada["mediaItemKey"] = mediaItemKey;
									callbackMedatada["title"] = localTitle;

									if (userData == "")
										callbackMedatada["userData"] = nullptr;
									else
									{
										json userDataRoot = JSONUtils::toJson(userData);

										callbackMedatada["userData"] = userDataRoot;
									}
								}

								{
									int64_t encodingProfileKey = -1;
									tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
										physicalPathKey,
										// 2022-12-18: MIK potrebbe
										// essere stato appena aggiunto
										true
									);

									string physicalPath;
									string fileName;
									int64_t sizeInBytes;
									string deliveryFileName;

									tie(physicalPath, ignore, ignore, fileName, ignore, ignore) = physicalPathDetails;

									callbackMedatada["fileName"] = fileName;
									// callbackMedatada["physicalPath"] =
									// physicalPath;
								}
							}

							if (contentType == MMSEngineDBFacade::ContentType::Video || contentType == MMSEngineDBFacade::ContentType::Audio)
							{
								try
								{
									int64_t durationInMilliSeconds = _mmsEngineDBFacade->getMediaDurationInMilliseconds(
										mediaItemKey, physicalPathKey,
										// 2022-12-18: MIK potrebbe
										// essere stato appena aggiunto
										true
									);

									float durationInSeconds = durationInMilliSeconds / 1000;

									callbackMedatada["durationInSeconds"] = durationInSeconds;
								}
								catch (runtime_error &e)
								{
									SPDLOG_ERROR(
										string() +
										"getMediaDurationInMilliseconds "
										"failed" +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
										", physicalPathKey: " + to_string(physicalPathKey) + ", exception: " + e.what()
									);
								}
								catch (exception &e)
								{
									SPDLOG_ERROR(
										string() +
										"getMediaDurationInMilliseconds "
										"failed" +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
										", physicalPathKey: " + to_string(physicalPathKey)
									);
								}
							}
						}

						string data = JSONUtils::toString(callbackMedatada);

						userHttpCallback(
							ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, httpMethod,
							callbackTimeoutInSeconds, httpHeadersRoot, data, userName, password, maxRetries
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", dependencyIndex: " + to_string(dependencyIndex) +
											  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}
					catch (exception e)
					{
						string errorMessage = fmt::format(
							"http callback failed"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", dependencyIndex: {}"
							", dependencies.size(): {}",
							_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size()
						);
						SPDLOG_ERROR(errorMessage);

						if (dependencies.size() > 1)
						{
							if (stopIfReferenceProcessingError)
								throw runtime_error(errorMessage);
						}
						else
							throw runtime_error(errorMessage);
					}

					dependencyIndex++;
				}
			}
		}
		else
		{
			try
			{
				userHttpCallback(
					ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, httpMethod, callbackTimeoutInSeconds,
					httpHeadersRoot, httpBody, userName, password, maxRetries
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "http callback failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "httpCallbackTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "httpCallbackTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::userHttpCallback(
	int64_t ingestionJobKey, string httpProtocol, string httpHostName, int httpPort, string httpURI, string httpURLParameters, bool formData,
	string httpMethod, long callbackTimeoutInSeconds, json userHeadersRoot, string &httpBody, string userName, string password, int maxRetries
)
{

	string userURL;

	try
	{
		SPDLOG_INFO(
			"userHttpCallback"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", httpProtocol: {}"
			", httpHostName: {}"
			", httpPort: {}"
			", httpURI: {}"
			", httpURLParameters: {}"
			", formData: {}"
			", maxRetries: {}",
			_processorIdentifier, ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, maxRetries
		);

		userURL = httpProtocol + "://" + httpHostName + ":" + to_string(httpPort) + httpURI + (formData ? "" : httpURLParameters);

		vector<string> otherHeaders;
		for (int userHeaderIndex = 0; userHeaderIndex < userHeadersRoot.size(); ++userHeaderIndex)
		{
			string userHeader = JSONUtils::asString(userHeadersRoot[userHeaderIndex]);

			otherHeaders.push_back(userHeader);
		}

		if (httpMethod == "PUT")
		{
			if (formData)
			{
				vector<pair<string, string>> formData;
				{
					json formDataParametersRoot = JSONUtils::toJson(httpBody);
					for (int formFieldIndex = 0; formFieldIndex < formDataParametersRoot.size(); formFieldIndex++)
					{
						json formFieldRoot = formDataParametersRoot[formFieldIndex];

						string name = JSONUtils::asString(formFieldRoot, "name", "");
						string value = JSONUtils::asString(formFieldRoot, "value", "");

						if (name != "")
							formData.push_back(make_pair(name, value));
					}
				}

				MMSCURL::httpPutFormData(_logger, ingestionJobKey, userURL, formData, callbackTimeoutInSeconds, maxRetries);
			}
			else
			{
				string contentType;
				if (httpBody != "")
					contentType = "application/json";

				MMSCURL::httpPutString(
					_logger, ingestionJobKey, userURL, callbackTimeoutInSeconds, userName, password, httpBody, contentType, otherHeaders, maxRetries
				);
			}
		}
		else if (httpMethod == "POST")
		{
			if (formData)
			{
				vector<pair<string, string>> formData;
				{
					json formDataParametersRoot = JSONUtils::toJson(httpBody);
					for (int formFieldIndex = 0; formFieldIndex < formDataParametersRoot.size(); formFieldIndex++)
					{
						json formFieldRoot = formDataParametersRoot[formFieldIndex];

						string name = JSONUtils::asString(formFieldRoot, "name", "");
						string value = JSONUtils::asString(formFieldRoot, "value", "");

						if (name != "")
							formData.push_back(make_pair(name, value));
					}
				}

				MMSCURL::httpPostFormData(_logger, ingestionJobKey, userURL, formData, callbackTimeoutInSeconds, maxRetries);
			}
			else
			{
				string contentType;
				if (httpBody != "")
					contentType = "application/json";

				MMSCURL::httpPostString(
					_logger, ingestionJobKey, userURL, callbackTimeoutInSeconds, userName, password, httpBody, contentType, otherHeaders, maxRetries
				);
			}
		}
		else // if (httpMethod == "GET")
		{
			vector<string> otherHeaders;
			MMSCURL::httpGet(_logger, ingestionJobKey, userURL, callbackTimeoutInSeconds, userName, password, otherHeaders, maxRetries);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = string() + "User Callback URL failed (runtime_error)" + ", userURL: " + userURL +
							  ", maxRetries: " + to_string(maxRetries) + ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "User Callback URL failed (exception)" + ", userURL: " + userURL + ", maxRetries: " + to_string(maxRetries) +
							  ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}
