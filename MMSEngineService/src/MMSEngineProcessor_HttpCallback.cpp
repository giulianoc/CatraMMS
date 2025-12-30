
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
#include <regex>
#include <tuple>

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
			string errorMessage = std::format(
				"No configured any media to be notified (HTTP Callback)"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", dependencies.size: {}",
				_processorIdentifier, ingestionJobKey, dependencies.size()
			);
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
		bool forwardInputMedia;
		{
			addMediaData = JSONUtils::asBool(parametersRoot, "addMediaData", true);
			httpProtocol = JSONUtils::asString(parametersRoot, "protocol", "http");
			userName = JSONUtils::asString(parametersRoot, "userName", "");
			password = JSONUtils::asString(parametersRoot, "password", "");
			string field = "hostName";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _processorIdentifier: {}"
					", Field: {}",
					_processorIdentifier, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			httpHostName = JSONUtils::asString(parametersRoot, field, "");

			field = "port";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				if (httpProtocol == "http")
					httpPort = 80;
				else
					httpPort = 443;
			}
			else
				httpPort = JSONUtils::asInt32(parametersRoot, field, 0);

			callbackTimeoutInSeconds = JSONUtils::asInt32(parametersRoot, "timeout", 120);

			field = "uri";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _processorIdentifier: {}"
					", Field: {}",
					_processorIdentifier, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			httpURI = JSONUtils::asString(parametersRoot, field, "");

			httpURLParameters = JSONUtils::asString(parametersRoot, "parameters", "");
			formData = JSONUtils::asBool(parametersRoot, "formData", false);
			httpMethod = JSONUtils::asString(parametersRoot, "method", "POST");
			httpBody = JSONUtils::asString(parametersRoot, "httpBody", "");

			field = "headers";
			if (JSONUtils::isPresent(parametersRoot, field))
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

			maxRetries = JSONUtils::asInt32(parametersRoot, "maxRetries", 1);
			forwardInputMedia = JSONUtils::asBool(parametersRoot, "forwardInputMedia", false);
		}

		if (addMediaData && (httpMethod == "POST" || httpMethod == "PUT"))
		{
			if (httpBody != "")
			{
				SPDLOG_INFO(
					"POST/PUT with httpBody"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size()
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
							"userHttpCallback"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", httpProtocol: {}"
							", httpHostName: {}"
							", httpURI: {}"
							", httpURLParameters: {}"
							", formData: {}"
							", httpMethod: {}"
							", httpBody: {}",
							_processorIdentifier, ingestionJobKey, httpProtocol, httpHostName, httpURI, httpURLParameters, formData, httpMethod,
							httpBody
						);

						userHttpCallback(
							ingestionJobKey, httpProtocol, httpHostName, httpPort, httpURI, httpURLParameters, formData, httpMethod,
							callbackTimeoutInSeconds, httpHeadersRoot, httpBody, userName, password, maxRetries
						);
					}
					catch (exception e)
					{
						string errorMessage = std::format(
							"http callback failed"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", dependencyIndex: {}"
							", dependencies.size(): {}"
							", e.what(): {}",
							_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size(), e.what()
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
										json userDataRoot = JSONUtils::toJson<json>(userData);

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
										json userDataRoot = JSONUtils::toJson<json>(userData);

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
								catch (exception &e)
								{
									SPDLOG_ERROR(
										"getMediaDurationInMilliseconds failed"
										", ingestionJobKey: {}"
										", mediaItemKey: {}"
										", physicalPathKey: {}"
										", e.what: {}",
										ingestionJobKey, mediaItemKey, physicalPathKey, e.what()
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
					catch (exception e)
					{
						string errorMessage = std::format(
							"http callback failed"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", dependencyIndex: {}"
							", dependencies.size(): {}"
							", e.what: {}",
							_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size(), e.what()
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
			catch (exception e)
			{
				string errorMessage = std::format(
					"http callback failed"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size(): {}"
					", e.what: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), e.what()
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (forwardInputMedia)
		{
			SPDLOG_INFO(
				"userHttpCallback"
				", processorIdentifier: {}"
				", ingestionJobKey: {}"
				", forwardInputMedia: {}",
				_processorIdentifier, ingestionJobKey, forwardInputMedia
			);

			for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
			{
				try
				{
					int64_t key;
					Validator::DependencyType dependencyType;

					tie(key, ignore, dependencyType, ignore) = keyAndDependencyType;

					int64_t physicalPathKey;
					int64_t mediaItemKey;

					if (dependencyType == Validator::DependencyType::MediaItemKey)
					{
						mediaItemKey = key;

						{
							int64_t encodingProfileKey = -1;
							bool warningIfMissing = false;
							tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
								mediaItemKey, encodingProfileKey, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato appena aggiunto
								true
							);

							tie(physicalPathKey, ignore, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
						}
					}
					else
					{
						physicalPathKey = key;

						{
							bool warningIfMissing = false;
							tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
								physicalPathKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
									workspace->_workspaceKey, physicalPathKey, warningIfMissing,
									// 2022-12-18: MIK potrebbe essere stato appena aggiunto
									true
								);

							tie(mediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = physicalPathKeyDetails;
						}
					}

					SPDLOG_INFO(
						"userHttpCallback, addIngestionJobOutput"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", mediaItemKey: {}"
						", physicalPathKey: {}",
						_processorIdentifier, ingestionJobKey, mediaItemKey, physicalPathKey
					);
					_mmsEngineDBFacade->addIngestionJobOutput(ingestionJobKey, mediaItemKey, physicalPathKey, -1);
				}
				catch (exception e)
				{
					string errorMessage = std::format(
						"http callback failed"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", dependencies.size(): {}"
						", exception(): {}",
						_processorIdentifier, ingestionJobKey, dependencies.size(), e.what()
					);
					SPDLOG_ERROR(errorMessage);
				}
			}
		}

		SPDLOG_INFO(
			"Update IngestionJob"
			", ingestionJobKey: {}"
			", IngestionStatus: End_TaskSuccess"
			", errorMessage: ",
			ingestionJobKey
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"httpCallbackTask failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}",
			_processorIdentifier, ingestionJobKey
		);

		SPDLOG_INFO(
			"Update IngestionJob"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", IngestionStatus: End_IngestionFailure"
			", errorMessage: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				"Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", errorMessage: {}",
				_processorIdentifier, ingestionJobKey, ex.what()
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

		userURL = std::format("{}://{}:{}{}{}", httpProtocol, httpHostName, httpPort, httpURI, (formData ? "" : httpURLParameters));

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
					json formDataParametersRoot = JSONUtils::toJson<json>(httpBody);
					for (int formFieldIndex = 0; formFieldIndex < formDataParametersRoot.size(); formFieldIndex++)
					{
						json formFieldRoot = formDataParametersRoot[formFieldIndex];

						string name = JSONUtils::asString(formFieldRoot, "name", "");
						string value = JSONUtils::asString(formFieldRoot, "value", "");

						if (name != "")
							formData.push_back(make_pair(name, value));
					}
				}

				CurlWrapper::httpPutFormData(
					userURL, formData, callbackTimeoutInSeconds, std::format(", ingestionJobKey: {}", ingestionJobKey), maxRetries
				);
			}
			else
			{
				string contentType;
				if (httpBody != "")
					contentType = "application/json";

				CurlWrapper::httpPutString(
					userURL, callbackTimeoutInSeconds, CurlWrapper::basicAuthorization(userName, password), httpBody, contentType, otherHeaders,
					std::format(", ingestionJobKey: {}", ingestionJobKey), maxRetries
				);
			}
		}
		else if (httpMethod == "POST")
		{
			if (formData)
			{
				vector<pair<string, string>> formData;
				{
					json formDataParametersRoot = JSONUtils::toJson<json>(httpBody);
					for (int formFieldIndex = 0; formFieldIndex < formDataParametersRoot.size(); formFieldIndex++)
					{
						json formFieldRoot = formDataParametersRoot[formFieldIndex];

						string name = JSONUtils::asString(formFieldRoot, "name", "");
						string value = JSONUtils::asString(formFieldRoot, "value", "");

						if (name != "")
							formData.push_back(make_pair(name, value));
					}
				}

				CurlWrapper::httpPostFormData(
					userURL, formData, callbackTimeoutInSeconds, std::format(", ingestionJobKey: {}", ingestionJobKey), maxRetries
				);
			}
			else
			{
				string contentType;
				if (httpBody != "")
					contentType = "application/json";

				CurlWrapper::httpPostString(
					userURL, callbackTimeoutInSeconds, CurlWrapper::basicAuthorization(userName, password), httpBody, contentType, otherHeaders,
					std::format(", ingestionJobKey: {}", ingestionJobKey), maxRetries
				);
			}
		}
		else if (httpMethod == "DELETE")
		{
			string contentType;
			if (httpBody != "")
				contentType = "application/json";

			CurlWrapper::httpDelete(
				userURL, callbackTimeoutInSeconds, CurlWrapper::basicAuthorization(userName, password), otherHeaders,
				std::format(", ingestionJobKey: {}", ingestionJobKey), maxRetries
			);
		}
		else // if (httpMethod == "GET")
		{
			vector<string> otherHeaders;
			CurlWrapper::httpGet(
				userURL, callbackTimeoutInSeconds, CurlWrapper::basicAuthorization(userName, password), otherHeaders,
				std::format(", ingestionJobKey: {}", ingestionJobKey), maxRetries
			);
		}
	}
	catch (exception e)
	{
		string errorMessage = std::format(
			"User Callback URL failed (exception)"
			", userURL: {}"
			", maxRetries: {}"
			", exception: {}",
			userURL, maxRetries, e.what()
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}
