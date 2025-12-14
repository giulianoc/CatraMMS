/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.cpp
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#include "API.h"
#include "Convert.h"
#include "CurlWrapper.h"
#include "Datetime.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "ProcessUtility.h"
#include "SafeFileSystem.h"
#include "Validator.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"
#include <format>
#include <fstream>
#include <regex>
#include <sstream>

void API::ingestion(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestion";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canIngestWorkflow)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canIngestWorkflow: {}",
			apiAuthorizationDetails->canIngestWorkflow
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	json responseBodyRoot;
	chrono::system_clock::time_point startPoint = chrono::system_clock::now();
	try
	{
		json requestBodyRoot = manageWorkflowVariables(requestData.requestBody, nullptr);

		// string responseBody;
		json responseBodyTasksRoot = json::array();

#ifdef __POSTGRES__
		PostgresConnTrans trans(_mmsEngineDBFacade->masterPostgresConnectionPool(), true);
		/*
		shared_ptr<PostgresConnection> conn = _mmsEngineDBFacade->beginWorkflow();
		work trans{*(conn->_sqlConnection)};
		*/
#else
		shared_ptr<MySQLConnection> conn = _mmsEngineDBFacade->beginIngestionJobs();
#endif
		try
		{
			/*
			int milliSecondsToSleepWaitingLock = 200;

			PersistenceLock persistenceLock(_mmsEngineDBFacade.get(),
					MMSEngineDBFacade::LockType::Ingestion,
					_maxSecondsToWaitAPIIngestionLock,
					_hostName, "APIIngestion", milliSecondsToSleepWaitingLock,
					_logger);
			*/

			// used to save <label of the task> ---> vector of ingestionJobKey.
			// A vector is used in case the same label is used more times It is
			// used when ReferenceLabel is used.
			unordered_map<string, vector<int64_t>> mapLabelAndIngestionJobKey;

			Validator validator(_mmsEngineDBFacade, _configurationRoot);
			// it starts from the root and validate recursively the entire body
			validator.validateIngestedRootMetadata(apiAuthorizationDetails->workspace->_workspaceKey, requestBodyRoot);

			if (!JSONUtils::isMetadataPresent(requestBodyRoot, "type"))
			{
				string errorMessage = std::format("Field is not present or it is null"
												  ", Field: type");
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string rootType = JSONUtils::asString(requestBodyRoot, "type", "");

			string rootLabel = JSONUtils::asString(requestBodyRoot, "label", "");
			bool rootHidden = JSONUtils::asBool(requestBodyRoot, "hidden", false);

#ifdef __POSTGRES__
			int64_t ingestionRootKey =
				_mmsEngineDBFacade->addWorkflow(trans, apiAuthorizationDetails->workspace->_workspaceKey, apiAuthorizationDetails->userKey, rootType, rootLabel, rootHidden,
					requestData.requestBody);
#else
			int64_t ingestionRootKey =
				_mmsEngineDBFacade->addIngestionRoot(conn, apiAuthorizationDetails->workspace->_workspaceKey, apiAuthorizationDetails->userKey, rootType, rootLabel, requestBody.c_str());
#endif
			requestBodyRoot["ingestionRootKey"] = ingestionRootKey;

			if (!JSONUtils::isMetadataPresent(requestBodyRoot, "task"))
			{
				string errorMessage = std::format("Field is not present or it is null"
												  ", Field: task");
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json &taskRoot = requestBodyRoot["task"];

			if (!JSONUtils::isMetadataPresent(taskRoot, "type"))
			{
				string errorMessage = std::format("Field is not present or it is null"
												  ", Field: type");
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string taskType = JSONUtils::asString(taskRoot, "type", "");

			if (taskType == "GroupOfTasks")
			{
				vector<int64_t> dependOnIngestionJobKeysForStarting;

				// 2019-01-01: it is not important since dependOnIngestionJobKey
				// is -1 int localDependOnSuccess = 0; 2019-07-24: in case of a
				// group of tasks, as it is, this is important
				//	because, otherwise, in case of a group of tasks as first
				// element of the workflow, 	it will not work correctly. I saw
				// this for example in the scenario where, using the player, 	we
				// do two cuts. The workflow generated than was: two cuts in
				// parallel and then the concat. 	This scenario works if
				// localDependOnSuccess is 1
				int localDependOnSuccess = 1;
#ifdef __POSTGRES__
				ingestionGroupOfTasks(
					trans, apiAuthorizationDetails->userKey, apiAuthorizationDetails->password, apiAuthorizationDetails->workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysForStarting, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#else
				ingestionGroupOfTasks(
					conn, apiAuthorizationDetails->userKey, apiAuthorizationDetails->password, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysForStarting, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#endif
			}
			else
			{
				vector<int64_t> dependOnIngestionJobKeysForStarting;
				int localDependOnSuccess = 0; // it is not important since
											  // dependOnIngestionJobKey is -1
#ifdef __POSTGRES__
				ingestionSingleTask(
					trans, apiAuthorizationDetails->userKey, apiAuthorizationDetails->password, apiAuthorizationDetails->workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysForStarting, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#else
				ingestionSingleTask(
					conn, apiAuthorizationDetails->userKey, apiAuthorizationDetails->password, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysForStarting, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#endif
			}

			string processedMetadataContent;
			{
				processedMetadataContent = JSONUtils::toString(requestBodyRoot);
			}

			bool commit = true;
#ifdef __POSTGRES__
			_mmsEngineDBFacade->endWorkflow(trans, commit, ingestionRootKey, processedMetadataContent);
#else
			_mmsEngineDBFacade->endIngestionJobs(conn, commit, ingestionRootKey, processedMetadataContent);
#endif

			{
				/*
				string beginOfResponseBody = string("{ ")
						+ "\"workflow\": { "
						+ "\"ingestionRootKey\": " + to_string(ingestionRootKey)
						+ ", \"label\": \"" + rootLabel + "\" "
						+ "}, "
						+ "\"tasks\": [ ";
				responseBody.insert(0, beginOfResponseBody);
				responseBody += " ] }";
				*/

				json responseBodyWorkflowRoot;
				responseBodyWorkflowRoot["ingestionRootKey"] = ingestionRootKey;
				responseBodyWorkflowRoot["label"] = rootLabel;

				responseBodyRoot["workflow"] = responseBodyWorkflowRoot;
				responseBodyRoot["tasks"] = responseBodyTasksRoot;
			}
		}
		catch (exception &e)
		{
			bool commit = false;
#ifdef __POSTGRES__
			_mmsEngineDBFacade->endWorkflow(trans, commit, -1, string());
#else
			_mmsEngineDBFacade->endIngestionJobs(conn, commit, -1, string());
#endif

			SPDLOG_ERROR(
				"request body parsing failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}

	try
	{
		string responseBody = JSONUtils::toString(responseBodyRoot);

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		SPDLOG_INFO(
			"Ingestion"
			", @MMS statistics@ - elapsed (secs): @{}@",
			chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()
		);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

json API::manageWorkflowVariables(const string_view& requestBody, json variablesValuesToBeUsedRoot)
{
	json requestBodyRoot;

	try
	{
		SPDLOG_INFO(
			"manageWorkflowVariables"
			", requestData.requestBody: {}",
			requestBody
		);

		if (variablesValuesToBeUsedRoot == nullptr)
		{
			SPDLOG_INFO("manageWorkflowVariables"
						", there are no variables");
		}
		else
		{
			string sVariablesValuesToBeUsedRoot = JSONUtils::toString(variablesValuesToBeUsedRoot);

			SPDLOG_INFO(
				"manageWorkflowVariables"
				", sVariablesValuesToBeUsedRoot: {}",
				sVariablesValuesToBeUsedRoot
			);
		}

		requestBodyRoot = JSONUtils::toJson(requestBody);

		/*
		 * Definition of the Variables into the Workflow:
		"variables": {
				"var n. 1": {
						"type": "int",	// or string
						"isNull": false,
						"value": 10,
						"description": "..."
				},
				"var n. 2": {
						"type": "string",
						"isNull": false,
						"value": "...",
						"description": "..."
				}
		}

		Workflow instantiated (example):
				"task": {
						"label": "Use of a WorkflowAsLibrary",

						"parameters": {
								"workflowAsLibraryLabel": "Best Picture of the
		Video", "workflowAsLibraryType": "MMS",

								"imageRetention": "1d",
								"imageTags": "FACE",
								"ingester": "Admin",
								"initialFramesNumberToBeSkipped": 1500,
								"instantInSeconds": 60,
								"label": "Image label",
								"title": "My Title"
						},
						"type": "Workflow-As-Library"
				}
		 */
		string field = "variables";
		if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
		{
			json variablesRoot = requestBodyRoot[field];
			if (variablesRoot.begin() != variablesRoot.end())
			// if (variablesRoot.size() > 0)
			{
				string localRequestBody(requestBody);

				SPDLOG_INFO("variables processing...");

				for (auto &[keyRoot, valRoot] : variablesRoot.items())
				{
					string sKey = JSONUtils::toString(json(keyRoot));
					if (sKey.length() > 2)
						sKey = sKey.substr(1, sKey.length() - 2);

					SPDLOG_INFO(
						"variable processing"
						", sKey: {}",
						sKey
					);

					string variableToBeReplaced;
					string sValue;
					{
						json variableDetails = valRoot;

						field = "type";
						string variableType = JSONUtils::asString(variableDetails, field, "");

						field = "isNull";
						bool variableIsNull = JSONUtils::asBool(variableDetails, field, false);

						if (variableType == "jsonObject" || variableType == "jsonArray")
							variableToBeReplaced = string("\"${") + sKey + "}\"";
						else
							variableToBeReplaced = string("${") + sKey + "}";

						if (variablesValuesToBeUsedRoot == nullptr)
						{
							field = "value";
							if (variableType == "string")
							{
								if (variableIsNull)
								{
									sValue = "";
								}
								else
								{
									sValue = JSONUtils::asString(variableDetails, field, "");

									// scenario, the json will be: "field":
									// "${var_name}"
									//	so in case the value of the variable
									// contains " we have 	to replace it with \"
									sValue = regex_replace(sValue, regex("\""), "\\\"");
								}
							}
							else if (variableType == "integer")
							{
								if (variableIsNull)
									sValue = "null";
								else
									sValue = to_string(JSONUtils::asInt64(variableDetails, field, 0));
							}
							else if (variableType == "decimal")
							{
								if (variableIsNull)
									sValue = "null";
								else
									sValue = to_string(JSONUtils::asDouble(variableDetails, field, 0.0));
							}
							else if (variableType == "boolean")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									bool bValue = JSONUtils::asBool(variableDetails, field, false);
									sValue = bValue ? "true" : "false";
								}
							}
							else if (variableType == "datetime")
							{
								if (variableIsNull)
									sValue = "";
								else
									sValue = JSONUtils::asString(variableDetails, field, "");
							}
							else if (variableType == "datetime-millisecs")
							{
								if (variableIsNull)
									sValue = "";
								else
									sValue = JSONUtils::asString(variableDetails, field, "");
							}
							else if (variableType == "jsonObject")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									sValue = JSONUtils::toString(variableDetails[field]);
								}
							}
							else if (variableType == "jsonArray")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									sValue = JSONUtils::toString(variableDetails[field]);
								}
							}
							else
							{
								string errorMessage = std::format(
									"Wrong Variable Type parsing RequestBody"
									", variableType: {}"
									", requestBody: {}",
									variableType, requestBody
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							SPDLOG_INFO(
								"variable information"
								", sKey: {}"
								", variableType: {}"
								", variableIsNull: {}"
								", sValue: {}",
								sKey, variableType, variableIsNull, sValue
							);
						}
						else
						{
							if (variableType == "string")
							{
								sValue = JSONUtils::asString(variablesValuesToBeUsedRoot, sKey, "");

								// scenario, the json will be: "field":
								// "${var_name}"
								//	so in case the value of the variable
								// contains " we have 	to replace it with \"
								sValue = regex_replace(sValue, regex("\""), "\\\"");
							}
							else if (variableType == "integer")
								sValue = to_string(JSONUtils::asInt64(variablesValuesToBeUsedRoot, sKey, 0));
							else if (variableType == "decimal")
								sValue = to_string(JSONUtils::asDouble(variablesValuesToBeUsedRoot, sKey, 0.0));
							else if (variableType == "boolean")
							{
								bool bValue = JSONUtils::asBool(variablesValuesToBeUsedRoot, sKey, false);
								sValue = bValue ? "true" : "false";
							}
							else if (variableType == "datetime")
								sValue = JSONUtils::asString(variablesValuesToBeUsedRoot, sKey, "");
							else if (variableType == "datetime-millisecs")
								sValue = JSONUtils::asString(variablesValuesToBeUsedRoot, sKey, "");
							else if (variableType == "jsonObject")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									sValue = JSONUtils::toString(variablesValuesToBeUsedRoot[sKey]);
								}
							}
							else if (variableType == "jsonArray")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									sValue = JSONUtils::toString(variablesValuesToBeUsedRoot[sKey]);
								}
							}
							else
							{
								string errorMessage = std::format(
									"Wrong Variable Type parsing RequestBody"
									", variableType: {}"
									", requestBody: {}",
									variableType, requestBody
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							SPDLOG_INFO(
								"variable information"
								", sKey: {}"
								", variableType: {}"
								", variableIsNull: {}"
								", sValue: {}",
								sKey, variableType, variableIsNull, sValue
							);
						}
					}

					SPDLOG_INFO(
						"requestBody, replace"
						", variableToBeReplaced: {}"
						", sValue: {}",
						variableToBeReplaced, sValue
					);
					size_t index = 0;
					while (true)
					{
						// Locate the substring to replace.
						index = localRequestBody.find(variableToBeReplaced, index);
						if (index == string::npos)
							break;

						// Make the replacement.
						localRequestBody.replace(index, variableToBeReplaced.length(), sValue);

						// Advance index forward so the next iteration doesn't
						// pick it up as well.
						index += sValue.length();
					}
				}

				SPDLOG_INFO(
					"requestBody after the replacement of the variables"
					", localRequestBody: {}",
					localRequestBody
				);

				requestBodyRoot = JSONUtils::toJson(localRequestBody);
			}
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"requestBody json is not well format"
			", requestData.requestBody: {}",
			requestBody
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	return requestBodyRoot;
}

void API::manageReferencesInput(
	int64_t ingestionRootKey, string taskOrGroupOfTasksLabel, string ingestionType,
	json &taskRoot, // taskRoot updated with the new parametersRoot
	bool parametersSectionPresent,

	// parametersRoot is changed:
	//	1. added ReferenceIngestionJobKey in case of ReferenceLabel
	//	2. added all the inherited references
	json &parametersRoot,

	// dependOnIngestionJobKeysForStarting is extended with the
	// ReferenceIngestionJobKey in case of ReferenceLabel
	vector<int64_t> &dependOnIngestionJobKeysForStarting,

	// dependOnIngestionJobKeysOverallInput is extended with the References
	// present into the Task
	vector<int64_t> &dependOnIngestionJobKeysOverallInput,

	// mapLabelAndIngestionJobKey is extended with the ReferenceLabels
	unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey
)
{
	string field;

	// just for logging
	{
		string sDependOnIngestionJobKeysOverallInput;
		for (int referenceIndex = 0; referenceIndex < dependOnIngestionJobKeysOverallInput.size(); ++referenceIndex)
		{
			if (referenceIndex == 0)
				sDependOnIngestionJobKeysOverallInput = to_string(dependOnIngestionJobKeysOverallInput.at(referenceIndex));
			else
				sDependOnIngestionJobKeysOverallInput += (string(", ") + to_string(dependOnIngestionJobKeysOverallInput.at(referenceIndex)));
		}

		SPDLOG_INFO(
			"manageReferencesInput"
			", taskOrGroupOfTasksLabel: {}"
			", IngestionType: {}"
			", parametersSectionPresent: {}"
			", sDependOnIngestionJobKeysOverallInput: {}",
			taskOrGroupOfTasksLabel, ingestionType, parametersSectionPresent, sDependOnIngestionJobKeysOverallInput
		);
	}

	// initialize referencesRoot
	bool referencesSectionPresent = false;
	json referencesRoot = json::array();
	if (parametersSectionPresent)
	{
		field = "references";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			referencesRoot = parametersRoot[field];

			referencesSectionPresent = true;
		}
	}

	// Generally if the References tag is present, these will be used as
	// references for the Task In case the References tag is NOT present,
	// inherited references are used Sometimes, we want to use both, the
	// references coming from the tag and the inherid references. For example a
	// video is ingested and we want to overlay a logo that is already present
	// into MMS. In this case we add the Reference for the Image and we inherit
	// the video from the Add-Content Task. In these case we use the
	// "dependenciesToBeAddedToReferencesAt" parameter.

	// 2021-04-25: "dependenciesToBeAddedToReferencesAt" could be:
	//	- AtTheBeginning
	//	- AtTheEnd
	//	- an integer specifying the position where to place the dependencies.
	//		0 means AtTheBeginning
	int dependenciesToBeAddedToReferencesAtIndex = -1;
	{
		string atTheBeginning = "Beginning";
		string atTheEnd = "End";

		string dependenciesToBeAddedToReferencesAt;
		field = "dependenciesToBeAddedToReferencesAt";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			dependenciesToBeAddedToReferencesAt = JSONUtils::asString(parametersRoot, field, "");
			if (!dependenciesToBeAddedToReferencesAt.empty())
			{
				if (dependenciesToBeAddedToReferencesAt == atTheBeginning)
					dependenciesToBeAddedToReferencesAtIndex = 0;
				else if (dependenciesToBeAddedToReferencesAt == atTheEnd)
					dependenciesToBeAddedToReferencesAtIndex = referencesRoot.size();
				else
				{
					try
					{
						dependenciesToBeAddedToReferencesAtIndex = stoi(dependenciesToBeAddedToReferencesAt);
						if (dependenciesToBeAddedToReferencesAtIndex > referencesRoot.size())
							dependenciesToBeAddedToReferencesAtIndex = referencesRoot.size();
					}
					catch (exception& e)
					{
						string errorMessage = std::format(
							"dependenciesToBeAddedToReferencesAt is not well format"
							", dependenciesToBeAddedToReferencesAt: {}",
							dependenciesToBeAddedToReferencesAt
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}
	}

	// manage ReferenceLabel, inside the References Tag, If ReferenceLabel is
	// present, replace it with ReferenceIngestionJobKey
	if (referencesSectionPresent)
	{
		// bool referencesChanged = false;

		for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); ++referenceIndex)
		{
			json referenceRoot = referencesRoot[referenceIndex];

			field = "label";
			if (JSONUtils::isMetadataPresent(referenceRoot, field))
			{
				string referenceLabel = JSONUtils::asString(referenceRoot, field, "");

				if (referenceLabel.empty())
				{
					string errorMessage = std::format(
						"The 'label' value cannot be empty"
						", processing label: {}"
						", referenceLabel: {}",
						taskOrGroupOfTasksLabel, referenceLabel
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

				if (ingestionJobKeys.empty())
				{
					string errorMessage = std::format(
						"The 'label' value is not found"
						", processing label: {}"
						", referenceLabel: {}",
						taskOrGroupOfTasksLabel, referenceLabel
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (ingestionJobKeys.size() > 1)
				{
					string errorMessage = std::format(
						"The 'label' value cannot be used in more than one Task"
						", referenceLabel: {}"
						", ingestionJobKeys.size(): {}",
						referenceLabel, ingestionJobKeys.size()
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				field = "ingestionJobKey";
				referenceRoot[field] = ingestionJobKeys.back();

				referencesRoot[referenceIndex] = referenceRoot;

				field = "references";
				parametersRoot[field] = referencesRoot;

				// referencesChanged = true;

				// The workflow specifies expliticily a reference (input for the
				// task). Probable this is because the Reference is not part of
				// the 'dependOnIngestionJobKeysOverallInput' parameter that it
				// is generally same of 'dependOnIngestionJobKeysForStarting'.
				// For this reason we have to make sure this Reference is inside
				// dependOnIngestionJobKeysForStarting in order to avoid the
				// Task starts when the input is not yet ready
				vector<int64_t>::iterator itrIngestionJobKey =
					find(dependOnIngestionJobKeysForStarting.begin(), dependOnIngestionJobKeysForStarting.end(), ingestionJobKeys.back());
				if (itrIngestionJobKey == dependOnIngestionJobKeysForStarting.end())
					dependOnIngestionJobKeysForStarting.push_back(ingestionJobKeys.back());
			}
		}

		/*
		if (referencesChanged)
		{
			{
				taskMetadata = JSONUtils::toString(parametersRoot);
			}

			// commented because already logged in mmsEngineDBFacade
			// _logger->info(__FILEREF__ + "update IngestionJob"
			//     + ", localDependOnIngestionJobKey: " +
		to_string(localDependOnIngestionJobKey)
			//    + ", taskMetadata: " + taskMetadata
			// );

			_mmsEngineDBFacade->updateIngestionJobMetadataContent(conn,
		localDependOnIngestionJobKeyExecution, taskMetadata);
		}
		*/
	}

	SPDLOG_INFO(
		"add to referencesRoot all the inherited references?"
		", ingestionRootKey: {}"
		", taskOrGroupOfTasksLabel: {}"
		", IngestionType: {}"
		", parametersSectionPresent: {}"
		", referencesSectionPresent: {}"
		", dependenciesToBeAddedToReferencesAtIndex: {}"
		", dependOnIngestionJobKeysOverallInput.size(): {}",
		ingestionRootKey, taskOrGroupOfTasksLabel, ingestionType, parametersSectionPresent, referencesSectionPresent,
		dependenciesToBeAddedToReferencesAtIndex, dependOnIngestionJobKeysOverallInput.size()
	);

	// add to referencesRoot all the inherited references
	if ((!referencesSectionPresent || dependenciesToBeAddedToReferencesAtIndex != -1) && dependOnIngestionJobKeysOverallInput.size() > 0)
	{
		// Enter here if No References tag is present (so we have to add the
		// inherit input) OR we want to add dependOnReferences to the Raferences
		// tag

		if (dependenciesToBeAddedToReferencesAtIndex != -1)
		{
			{
				int previousReferencesRootSize = referencesRoot.size();
				int dependOnIngestionJobKeysSize = dependOnIngestionJobKeysOverallInput.size();

				SPDLOG_INFO(
					"add to referencesRoot all the inherited references"
					", ingestionRootKey: {}"
					", taskOrGroupOfTasksLabel: |{}"
					", previousReferencesRootSize: {}"
					", dependOnIngestionJobKeysSize: {}"
					", dependenciesToBeAddedToReferencesAtIndex: {}",
					ingestionRootKey, taskOrGroupOfTasksLabel, previousReferencesRootSize, dependOnIngestionJobKeysSize,
					dependenciesToBeAddedToReferencesAtIndex
				);

				// referencesRoot.resize(
				//   previousReferencesRootSize + dependOnIngestionJobKeysSize
				// );
				for (int index = previousReferencesRootSize - 1; index >= dependenciesToBeAddedToReferencesAtIndex; index--)
				{
					SPDLOG_INFO(
						"making 'space' in referencesRoot"
						", ingestionRootKey: {}"
						", from {} to {}",
						ingestionRootKey, index, index + dependOnIngestionJobKeysSize
					);

					referencesRoot[index + dependOnIngestionJobKeysSize] = referencesRoot[index];
				}

				for (int index = dependenciesToBeAddedToReferencesAtIndex;
					 index < dependenciesToBeAddedToReferencesAtIndex + dependOnIngestionJobKeysSize; index++)
				{
					SPDLOG_INFO(
						"fill in dependOnIngestionJobKey"
						", ingestionRootKey: {}"
						", from {} to {}",
						ingestionRootKey, index - dependenciesToBeAddedToReferencesAtIndex, index
					);

					json referenceRoot;
					string addedField = "ingestionJobKey";
					referenceRoot[addedField] = dependOnIngestionJobKeysOverallInput.at(index - dependenciesToBeAddedToReferencesAtIndex);

					referencesRoot[index] = referenceRoot;
				}
			}

			/*
			for (int referenceIndex =
			dependOnIngestionJobKeysOverallInput.size(); referenceIndex > 0;
			--referenceIndex)
			{
					json referenceRoot;
					string addedField = "ingestionJobKey";
					referenceRoot[addedField] =
			dependOnIngestionJobKeysOverallInput.at(referenceIndex - 1);

					// add at the beginning in referencesRoot
					{
							int previousSize = referencesRoot.size();
							referencesRoot.resize(previousSize + 1);
							for(int index = previousSize; index >
			dependenciesToBeAddedToReferencesAtIndex; index--)
									referencesRoot[index] = referencesRoot[index
			- 1]; referencesRoot[dependenciesToBeAddedToReferencesAtIndex] =
			referenceRoot;
					}
			}
			*/
		}
		else
		{
			for (int64_t & referenceIndex : dependOnIngestionJobKeysOverallInput)
			{
				json referenceRoot;
				string addedField = "ingestionJobKey";
				referenceRoot[addedField] = referenceIndex;

				referencesRoot.push_back(referenceRoot);
			}
		}

		field = "parameters";
		string arrayField = "references";
		parametersRoot[arrayField] = referencesRoot;
		if (!parametersSectionPresent)
		{
			taskRoot[field] = parametersRoot;
		}

		//{
		//    taskMetadata = JSONUtils::toString(parametersRoot);
		//}

		// commented because already logged in mmsEngineDBFacade
		// _logger->info(__FILEREF__ + "update IngestionJob"
		//     + ", localDependOnIngestionJobKey: " +
		//     to_string(localDependOnIngestionJobKey)
		//    + ", taskMetadata: " + taskMetadata
		// );

		//_mmsEngineDBFacade->updateIngestionJobMetadataContent(conn,
		// localDependOnIngestionJobKeyExecution, taskMetadata);
	}
	if (taskOrGroupOfTasksLabel == "Check Streaming OnError")
	{
		string taskMetadata = JSONUtils::toString(parametersRoot);

		SPDLOG_INFO(
			"testttttttt"
			", taskMetadata: {}"
			", referencesSectionPresent: {}"
			", dependenciesToBeAddedToReferencesAtIndex: {}"
			", dependOnIngestionJobKeysOverallInput.size: {}",
			taskMetadata, referencesSectionPresent, dependenciesToBeAddedToReferencesAtIndex, dependOnIngestionJobKeysOverallInput.size()
		);
	}
}

// return: ingestionJobKey associated to this task
#ifdef __POSTGRES__
vector<int64_t> API::ingestionSingleTask(
	PostgresConnTrans &trans, int64_t userKey, const string& apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &taskRoot,

	// dependOnSuccess == 0 -> OnError
	// dependOnSuccess == 1 -> OnSuccess
	// dependOnSuccess == -1 -> OnComplete
	// list of ingestion job keys to be executed before this task
	vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,

	// the media input are retrieved looking at the media generated by this list
	vector<int64_t> dependOnIngestionJobKeysOverallInput,

	unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
	/* string& responseBody, */ json &responseBodyTasksRoot
)
#else
vector<int64_t> API::ingestionSingleTask(
	shared_ptr<MySQLConnection> conn, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &taskRoot,

	// dependOnSuccess == 0 -> OnError
	// dependOnSuccess == 1 -> OnSuccess
	// dependOnSuccess == -1 -> OnComplete
	// list of ingestion job keys to be executed before this task
	vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,

	// the media input are retrieved looking at the media generated by this list
	vector<int64_t> dependOnIngestionJobKeysOverallInput,

	unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
	/* string& responseBody, */ json &responseBodyTasksRoot
)
#endif
{
	string field = "type";
	string type = JSONUtils::asString(taskRoot, field, "");

	string taskLabel;
	field = "label";
	taskLabel = JSONUtils::asString(taskRoot, field, "");

	SPDLOG_INFO(
		"Processing SingleTask..."
		", ingestionRootKey: {}"
		", type: {}"
		", taskLabel: {}",
		ingestionRootKey, type, taskLabel
	);

	field = "parameters";
	json parametersRoot;
	bool parametersSectionPresent = false;
	if (JSONUtils::isMetadataPresent(taskRoot, field))
	{
		parametersRoot = taskRoot[field];

		parametersSectionPresent = true;
	}

	// 2022-11-19: in case of Broadcaster, internalMMS already exist
	field = "internalMMS";
	json internalMMSRoot;
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
		internalMMSRoot = parametersRoot[field];

	// 2022-11-05: inizialmente internalMMSRoot con userKey e apiKey era
	// aggiunto solo per alcuni Task (i.e. il Live-Recorder), ora li aggiungo sempre perchè, in caso di external
	// encoder, 	questi parametri servono sempre
	{
		json credentialsRoot;
		{
			field = "userKey";
			credentialsRoot[field] = userKey;

			string apiKeyEncrypted = Encrypt::opensslEncrypt(apiKey);

			field = "apiKey";
			credentialsRoot[field] = apiKeyEncrypted;
		}
		field = "credentials";
		internalMMSRoot[field] = credentialsRoot;

		field = "internalMMS";
		parametersRoot[field] = internalMMSRoot;
	}

	if (type == "Encode")
	{
		// we will create a group of tasks and add there the Encode task in two
		// scenarios: case 1. in case of EncodingProfilesSet case 2. in case we
		// will have more than one References

		string encodingProfilesSetKeyField = "encodingProfilesSetKey";
		string encodingProfilesSetLabelField = "encodingProfilesSetLabel";
		string referencesField = "references";

		if (parametersSectionPresent &&
			(
				// case 1
				(JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetKeyField) ||
				 JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)) ||
				// case 2
				(JSONUtils::isMetadataPresent(parametersRoot, referencesField) && parametersRoot[referencesField].size() > 1)
			))
		{
			// we will replace the single Task with a GroupOfTasks where every
			// task is just for one profile/one reference

			// case 1
			bool profilesSetPresent = false;
			// case 2
			bool multiReferencesPresent = false;

			// we will use the vector for case 1
			vector<int64_t> encodingProfilesSetKeys;

			if (JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetKeyField) ||
				JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetLabelField))
			{
				// case 1

				profilesSetPresent = true;

				string encodingProfilesSetReference;

				if (JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetKeyField))
				{
					int64_t encodingProfilesSetKey = JSONUtils::asInt64(parametersRoot, encodingProfilesSetKeyField, 0);

					encodingProfilesSetReference = to_string(encodingProfilesSetKey);

					encodingProfilesSetKeys = _mmsEngineDBFacade->getEncodingProfileKeysBySetKey(workspace->_workspaceKey, encodingProfilesSetKey);

					{
						parametersRoot.erase(encodingProfilesSetKeyField);
						// json removed;
						// parametersRoot.removeMember(
						//   encodingProfilesSetKeyField, &removed
						// );
					}
				}
				else // if (JSONUtils::isMetadataPresent(parametersRoot,
					 // encodingProfilesSetLabelField))
				{
					string encodingProfilesSetLabel = JSONUtils::asString(parametersRoot, encodingProfilesSetLabelField, "");

					encodingProfilesSetReference = encodingProfilesSetLabel;

					encodingProfilesSetKeys =
						_mmsEngineDBFacade->getEncodingProfileKeysBySetLabel(workspace->_workspaceKey, encodingProfilesSetLabel);

					parametersRoot.erase(encodingProfilesSetLabelField);
				}

				if (encodingProfilesSetKeys.empty())
				{
					string errorMessage = std::format(
						"No EncodingProfileKey into the encodingProfilesSetKey"
						", encodingProfilesSetKey/encodingProfilesSetLabel: {}"
						", ingestionRootKey: {}"
						", type: {}"
						", taskLabel: {}",
						encodingProfilesSetReference, ingestionRootKey, type, taskLabel
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			// both, case 1 and case 2 could have multiple references
			json multiReferencesRoot;
			if (JSONUtils::isMetadataPresent(parametersRoot, referencesField) && parametersRoot[referencesField].size() > 1)
			{
				multiReferencesPresent = true;

				multiReferencesRoot = parametersRoot[referencesField];

				parametersRoot.erase(referencesField);
			}

			if (!profilesSetPresent && !multiReferencesPresent)
			{
				string errorMessage = std::format(
					"It's not possible to be here"
					", type: {}"
					", taskLabel: {}",
					type, taskLabel
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// based of the removeMember, parametersRoot will be:
			// in case of profiles set, without the profiles set parameter
			// in case of multiple references, without the References parameter

			json newTasksRoot = json::array();

			if (profilesSetPresent && multiReferencesPresent)
			{
				for (int64_t encodingProfileKey : encodingProfilesSetKeys)
				{
					for (int referenceIndex = 0; referenceIndex < multiReferencesRoot.size(); referenceIndex++)
					{
						json newTaskRoot;
						string localLabel =
							taskLabel + " - EncodingProfileKey: " + to_string(encodingProfileKey) + " - referenceIndex: " + to_string(referenceIndex);

						field = "label";
						newTaskRoot[field] = localLabel;

						field = "type";
						newTaskRoot[field] = "Encode";

						json newParametersRoot = parametersRoot;

						field = "encodingProfileKey";
						newParametersRoot[field] = encodingProfileKey;

						{
							json newReferencesRoot = json::array();
							newReferencesRoot.push_back(multiReferencesRoot[referenceIndex]);

							field = "references";
							newParametersRoot[field] = newReferencesRoot;
						}

						field = "parameters";
						newTaskRoot[field] = newParametersRoot;

						newTasksRoot.push_back(newTaskRoot);
					}
				}
			}
			else if (profilesSetPresent && !multiReferencesPresent)
			{
				for (int64_t encodingProfileKey : encodingProfilesSetKeys)
				{
					json newTaskRoot;
					string localLabel = taskLabel + " - EncodingProfileKey: " + to_string(encodingProfileKey);

					field = "label";
					newTaskRoot[field] = localLabel;

					field = "type";
					newTaskRoot[field] = "Encode";

					json newParametersRoot = parametersRoot;

					field = "encodingProfileKey";
					newParametersRoot[field] = encodingProfileKey;

					field = "parameters";
					newTaskRoot[field] = newParametersRoot;

					newTasksRoot.push_back(newTaskRoot);
				}
			}
			else if (!profilesSetPresent && multiReferencesPresent)
			{
				for (int referenceIndex = 0; referenceIndex < multiReferencesRoot.size(); referenceIndex++)
				{
					json newTaskRoot;
					string localLabel = taskLabel + " - referenceIndex: " + to_string(referenceIndex);

					field = "label";
					newTaskRoot[field] = localLabel;

					field = "type";
					newTaskRoot[field] = "Encode";

					json newParametersRoot = parametersRoot;

					{
						json newReferencesRoot = json::array();
						newReferencesRoot.push_back(multiReferencesRoot[referenceIndex]);

						field = "references";
						newParametersRoot[field] = newReferencesRoot;
					}

					field = "parameters";
					newTaskRoot[field] = newParametersRoot;

					newTasksRoot.push_back(newTaskRoot);
				}
			}

			json newParametersTasksGroupRoot;

			field = "executionType";
			newParametersTasksGroupRoot[field] = "parallel";

			field = "tasks";
			newParametersTasksGroupRoot[field] = newTasksRoot;

			json newTasksGroupRoot;

			field = "type";
			newTasksGroupRoot[field] = "GroupOfTasks";

			field = "parameters";
			newTasksGroupRoot[field] = newParametersTasksGroupRoot;

			field = "onSuccess";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}

			field = "onError";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}

			field = "onComplete";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}

#ifdef __POSTGRES__
			return ingestionGroupOfTasks(
				trans, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			return ingestionGroupOfTasks(
				conn, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}
		else
		{
			SPDLOG_INFO(
				"No special management for Encode"
				", ingestionRootKey: {}"
				", taskLabel: {}"
				", workspace->_workspaceKey: {}",
				ingestionRootKey, taskLabel, workspace->_workspaceKey
			);
		}
	}
	else if (type == "Face-Recognition")
	{
		// In case we will have more than one References,
		//  we will create a group of tasks and add there the Face-Recognition
		//  task

		string referencesField = "references";

		if (parametersSectionPresent && (JSONUtils::isMetadataPresent(parametersRoot, referencesField) && parametersRoot[referencesField].size() > 1))
		{
			// we will replace the single Task with a GroupOfTasks where every
			// task is just for one reference

			json multiReferencesRoot = parametersRoot[referencesField];

			parametersRoot.erase(referencesField);

			json newTasksRoot = json::array();

			for (int referenceIndex = 0; referenceIndex < multiReferencesRoot.size(); referenceIndex++)
			{
				json newTaskRoot;
				string localLabel = taskLabel + " - referenceIndex: " + to_string(referenceIndex);

				field = "label";
				newTaskRoot[field] = localLabel;

				field = "type";
				newTaskRoot[field] = "Face-Recognition";

				json newParametersRoot = parametersRoot;

				{
					json newReferencesRoot = json::array();
					newReferencesRoot.push_back(multiReferencesRoot[referenceIndex]);

					field = "references";
					newParametersRoot[field] = newReferencesRoot;
				}

				field = "parameters";
				newTaskRoot[field] = newParametersRoot;

				field = "onSuccess";
				if (JSONUtils::isMetadataPresent(taskRoot, field))
					newTaskRoot[field] = taskRoot[field];

				field = "onError";
				if (JSONUtils::isMetadataPresent(taskRoot, field))
					newTaskRoot[field] = taskRoot[field];

				field = "onComplete";
				if (JSONUtils::isMetadataPresent(taskRoot, field))
					newTaskRoot[field] = taskRoot[field];

				newTasksRoot.push_back(newTaskRoot);
			}

			json newParametersTasksGroupRoot;

			field = "executionType";
			newParametersTasksGroupRoot[field] = "parallel";

			field = "tasks";
			newParametersTasksGroupRoot[field] = newTasksRoot;

			json newTasksGroupRoot;

			field = "type";
			newTasksGroupRoot[field] = "GroupOfTasks";

			field = "parameters";
			newTasksGroupRoot[field] = newParametersTasksGroupRoot;

#ifdef __POSTGRES__
			return ingestionGroupOfTasks(
				trans, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			return ingestionGroupOfTasks(
				conn, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}
		else
		{
			SPDLOG_INFO(
				"No special management for Face-Recognition"
				", ingestionRootKey: {}"
				", taskLabel: {}"
				", workspace->_workspaceKey: {}",
				ingestionRootKey, taskLabel, workspace->_workspaceKey
			);
		}
	}
	else if (type == "Live-Recorder" || type == "Live-Proxy" || type == "VOD-Proxy" || type == "Countdown")
	{
		// 1. Live-Recorder needs the UserKey/ApiKey for the ingestion of the
		// chunks. The same UserKey/ApiKey used for the ingestion of the
		// Workflow are used to ingest the chunks

		// 2. Live-Recorder generates MediaItems as soon as the files/segments
		// are generated by the Live-Recorder. For this reason, the events
		// (onSuccess, onError, onComplete) have to be attached to the workflow
		// built to add these contents Here, we will remove the events
		// (onSuccess, onError, onComplete) from LiveRecorder, if present, and
		// we will add temporary inside the Parameters section. These events
		// will be managed later in EncoderVideoAudioProxy.cpp when the workflow
		// for the generated contents will be created

		// 3. Live-Proxy, in caso di filtri come 'detect freeze frame',
		// viene usato l'evento onError del Live-Proxy
		// Here, we will remove the events
		// (onSuccess, onError, onComplete) from LiveProxy, if present, and
		// we will add temporary inside the Parameters section. These events
		// will be managed later in the Encoder when the workflow
		// for this event will be created

		json eventsRoot;
		{
			string onSuccessField = "onSuccess";
			string onErrorField = "onError";
			string onCompleteField = "onComplete";
			if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField) || JSONUtils::isMetadataPresent(taskRoot, onErrorField) ||
				JSONUtils::isMetadataPresent(taskRoot, onCompleteField))
			{
				if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField))
				{
					json onSuccessRoot = taskRoot[onSuccessField];

					eventsRoot[onSuccessField] = onSuccessRoot;

					taskRoot.erase(onSuccessField);
				}
				if (JSONUtils::isMetadataPresent(taskRoot, onErrorField))
				{
					json onErrorRoot = taskRoot[onErrorField];

					eventsRoot[onErrorField] = onErrorRoot;

					taskRoot.erase(onErrorField);
				}
				if (JSONUtils::isMetadataPresent(taskRoot, onCompleteField))
				{
					json onCompleteRoot = taskRoot[onCompleteField];

					eventsRoot[onCompleteField] = onCompleteRoot;

					taskRoot.erase(onCompleteField);
				}
			}
		}
		field = "events";
		internalMMSRoot[field] = eventsRoot;

		string internalMMSField = "internalMMS";
		parametersRoot[internalMMSField] = internalMMSRoot;
	}
	else if (type == "Live-Cut" || type == "YouTube-Live-Broadcast")
	{
		// 1. Live-Cut and YouTube-Live-Broadcast need the UserKey/ApiKey for
		// the ingestion of the workflow they generate. The same UserKey/ApiKey
		// used for the ingestion of the Workflow are used to ingest the new
		// workflow they generate
		//
		// 2. Live-Cut generates a workflow made of Concat plus Cut.
		// YouTube-Live-Broadcast generates a workflow made of Live-Proxy or
		// VOD-Proxy For this reason, the events (onSuccess, onError,
		// onComplete) have to be attached to the new workflow Here, we will
		// remove the events (onSuccess, onError, onComplete) from
		// LiveCut/YouTubeLiveBroadcast, if present, and we will add temporary
		// inside the Parameters section. These events will be managed later in
		// MMSEngineProcessor.cpp when the new workflow will be created

		/*
json internalMMSRoot;
		{
				string field = "userKey";
				internalMMSRoot[field] = userKey;

				field = "apiKey";
				internalMMSRoot[field] = apiKey;
		}
		*/

		json eventsRoot;
		{
			string onSuccessField = "onSuccess";
			string onErrorField = "onError";
			string onCompleteField = "onComplete";
			if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField) || JSONUtils::isMetadataPresent(taskRoot, onErrorField) ||
				JSONUtils::isMetadataPresent(taskRoot, onCompleteField))
			{
				if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField))
				{
					json onSuccessRoot = taskRoot[onSuccessField];

					eventsRoot[onSuccessField] = onSuccessRoot;

					taskRoot.erase(onSuccessField);
				}
				if (JSONUtils::isMetadataPresent(taskRoot, onErrorField))
				{
					json onErrorRoot = taskRoot[onErrorField];

					eventsRoot[onErrorField] = onErrorRoot;

					taskRoot.erase(onErrorField);
				}
				if (JSONUtils::isMetadataPresent(taskRoot, onCompleteField))
				{
					json onCompleteRoot = taskRoot[onCompleteField];

					eventsRoot[onCompleteField] = onCompleteRoot;

					taskRoot.erase(onCompleteField);
				}
			}
		}
		field = "events";
		internalMMSRoot[field] = eventsRoot;

		string internalMMSField = "internalMMS";
		parametersRoot[internalMMSField] = internalMMSRoot;
	}
	else if (type == "Add-Content")
	{
		// The Add-Content Task can be used also to add just a variant/profile
		// of a content that it is already present into the MMS Repository. This
		// content that it is already present can be referenced using the
		// apposite parameter (variantOfMediaItemKey) or using the
		// variantOfReferencedLabel parameter. In this last case, we have to add
		// the VariantOfIngestionJobKey parameter using variantOfReferencedLabel

		string field = "variantOfReferencedLabel";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string referenceLabel = JSONUtils::asString(parametersRoot, field, "");

			if (referenceLabel == "")
			{
				string errorMessage = std::format(
					"The 'label' value cannot be empty"
					", ingestionRootKey: {}"
					", type: {}"
					", taskLabel: {}"
					", referenceLabel: {}",
					ingestionRootKey, type, taskLabel, referenceLabel
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

			if (ingestionJobKeys.empty())
			{
				string errorMessage = std::format(
					"The 'label' value is not found"
					", ingestionRootKey: {}"
					", type: {}"
					", taskLabel: {}"
					", referenceLabel: {}",
					ingestionRootKey, type, taskLabel, referenceLabel
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			else if (ingestionJobKeys.size() > 1)
			{
				string errorMessage = std::format(
					"The 'label' value cannot be used in more than one Task"
					", ingestionRootKey: {}"
					", type: {}"
					", taskLabel: {}"
					", referenceLabel: {}"
					", ingestionJobKeys.size(): {}",
					ingestionRootKey, type, taskLabel, referenceLabel, ingestionJobKeys.size()
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "VariantOfIngestionJobKey";
			parametersRoot[field] = ingestionJobKeys.back();
		}
	}
	else if (type == "Workflow-As-Library")
	{
		// read the WorkflowAsLibrary
		string workflowLibraryContent;
		{
			string workflowAsLibraryTypeField = "workflowAsLibraryType";
			string workflowAsLibraryLabelField = "workflowAsLibraryLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, workflowAsLibraryTypeField) ||
				!JSONUtils::isMetadataPresent(parametersRoot, workflowAsLibraryLabelField))
			{
				string errorMessage = __FILEREF__ +
									  "No workflowAsLibraryType/WorkflowAsLibraryLabel "
									  "parameters into the Workflow-As-Library Task" +
									  ", ingestionRootKey: " + to_string(ingestionRootKey) + ", type: " + type + ", taskLabel: " + taskLabel;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string workflowAsLibraryType = JSONUtils::asString(parametersRoot, workflowAsLibraryTypeField, "");
			string workflowAsLibraryLabel = JSONUtils::asString(parametersRoot, workflowAsLibraryLabelField, "");

			int workspaceKey;
			if (workflowAsLibraryType == "MMS")
				workspaceKey = -1;
			else
				workspaceKey = workspace->_workspaceKey;

			workflowLibraryContent = _mmsEngineDBFacade->getWorkflowAsLibraryContent(workspaceKey, workflowAsLibraryLabel);
		}

		json workflowLibraryRoot = manageWorkflowVariables(workflowLibraryContent, parametersRoot);

		// create a GroupOfTasks and add the Root Task of the Library to the
		// newGroupOfTasks

		json workflowLibraryTaskRoot;
		{
			string workflowRootTaskField = "task";
			if (!JSONUtils::isMetadataPresent(workflowLibraryRoot, workflowRootTaskField))
			{
				string errorMessage = __FILEREF__ +
									  "Wrong Workflow-As-Library format. Root Task was not "
									  "found" +
									  ", ingestionRootKey: " + to_string(ingestionRootKey) + ", type: " + type + ", taskLabel: " + taskLabel +
									  ", workflowLibraryContent: " + workflowLibraryContent;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			workflowLibraryTaskRoot = workflowLibraryRoot[workflowRootTaskField];
		}

		json newGroupOfTasksRoot;
		{
			json newGroupOfTasksParametersRoot;

			field = "executionType";
			newGroupOfTasksParametersRoot[field] = "parallel";

			{
				json newTasksRoot = json::array();
				newTasksRoot.push_back(workflowLibraryTaskRoot);

				field = "tasks";
				newGroupOfTasksParametersRoot[field] = newTasksRoot;
			}

			field = "type";
			newGroupOfTasksRoot[field] = "GroupOfTasks";

			field = "parameters";
			newGroupOfTasksRoot[field] = newGroupOfTasksParametersRoot;

			field = "onSuccess";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newGroupOfTasksRoot[field] = taskRoot[field];
			}

			field = "onError";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newGroupOfTasksRoot[field] = taskRoot[field];
			}

			field = "onComplete";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newGroupOfTasksRoot[field] = taskRoot[field];
			}
		}

#ifdef __POSTGRES__
		return ingestionGroupOfTasks(
			trans, userKey, apiKey, workspace, ingestionRootKey, newGroupOfTasksRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
			dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
			/* responseBody, */ responseBodyTasksRoot
		);
#else
		return ingestionGroupOfTasks(
			conn, userKey, apiKey, workspace, ingestionRootKey, newGroupOfTasksRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
			dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
			/* responseBody, */ responseBodyTasksRoot
		);
#endif
	}

	// just log initial parameters
	/*
{
			SPDLOG_INFO(__FILEREF__ + "IngestionJob to be added"
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", type: " + type
					+ ", taskLabel: " + taskLabel
					+ ", taskMetadata before: " +
JSONUtils::toString(parametersRoot)
			);
}
	*/

	manageReferencesInput(
		ingestionRootKey, taskLabel, type, taskRoot, parametersSectionPresent, parametersRoot, dependOnIngestionJobKeysForStarting,
		dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey
	);

	string taskMetadata;

	// if (parametersSectionPresent)
	{
		taskMetadata = JSONUtils::toString(parametersRoot);
	}

	vector<int64_t> waitForGlobalIngestionJobKeys;
	{
		field = "waitFor";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			json waitForRoot = parametersRoot[field];

			for (int waitForIndex = 0; waitForIndex < waitForRoot.size(); ++waitForIndex)
			{
				json waitForLabelRoot = waitForRoot[waitForIndex];

				field = "globalIngestionLabel";
				if (JSONUtils::isMetadataPresent(waitForLabelRoot, field))
				{
					string waitForGlobalIngestionLabel = JSONUtils::asString(waitForLabelRoot, field, "");

					_mmsEngineDBFacade->ingestionJob_IngestionJobKeys(
						workspace->_workspaceKey, waitForGlobalIngestionLabel,
						// 2022-12-18: true perchè IngestionJob dovrebbe essere
						// stato appena aggiunto
						true, waitForGlobalIngestionJobKeys
					);
					SPDLOG_INFO(
						"ingestionJob_IngestionJobKeys"
						", ingestionRootKey: {}"
						", taskLabel: {}"
						", workspace->_workspaceKey: {}"
						", waitForGlobalIngestionLabel: {}"
						", waitForGlobalIngestionJobKeys.size(): {}",
						ingestionRootKey, taskLabel, workspace->_workspaceKey, waitForGlobalIngestionLabel, waitForGlobalIngestionJobKeys.size()
					);
				}
			}
		}
	}

	string processingStartingFrom;
	{
		field = "processingStartingFrom";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
			processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");

		if (processingStartingFrom.empty())
		{
			tm tmUTCDateTime{};
			// char sProcessingStartingFrom[64];
			string sProcessingStartingFrom;

			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcNow = chrono::system_clock::to_time_t(now);

			gmtime_r(&utcNow, &tmUTCDateTime);
			/*
			sprintf(
				sProcessingStartingFrom, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmUTCDateTime.tm_year + 1900, tmUTCDateTime.tm_mon + 1,
				tmUTCDateTime.tm_mday, tmUTCDateTime.tm_hour, tmUTCDateTime.tm_min, tmUTCDateTime.tm_sec
			);
			*/
			sProcessingStartingFrom = std::format(
				"{:0>4}-{:0>2}-{:0>2}T{:0>2}:{:0>2}:{:0>2}Z", tmUTCDateTime.tm_year + 1900, tmUTCDateTime.tm_mon + 1, tmUTCDateTime.tm_mday,
				tmUTCDateTime.tm_hour, tmUTCDateTime.tm_min, tmUTCDateTime.tm_sec
			);

			processingStartingFrom = sProcessingStartingFrom;
		}
	}

	SPDLOG_INFO(
		"add IngestionJob"
		", ingestionRootKey: {}"
		", taskLabel: {}"
		", taskMetadata: {}"
		", IngestionType: {}"
		", processingStartingFrom: {}"
		", dependOnIngestionJobKeysForStarting.size(): {}"
		", dependOnSuccess: {}"
		", waitForGlobalIngestionJobKeys.size(): {}",
		ingestionRootKey, taskLabel, taskMetadata, type, processingStartingFrom, dependOnIngestionJobKeysForStarting.size(), dependOnSuccess,
		waitForGlobalIngestionJobKeys.size()
	);

#ifdef __POSTGRES__
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(
		trans, workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type), processingStartingFrom,
		dependOnIngestionJobKeysForStarting, dependOnSuccess, waitForGlobalIngestionJobKeys
	);
#else
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(
		conn, workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type), processingStartingFrom,
		dependOnIngestionJobKeysForStarting, dependOnSuccess, waitForGlobalIngestionJobKeys
	);
#endif
	field = "ingestionJobKey";
	taskRoot[field] = localDependOnIngestionJobKeyExecution;

	SPDLOG_INFO(
		"Save Label..."
		", ingestionRootKey: {}"
		", taskLabel: {}"
		", localDependOnIngestionJobKeyExecution: {}",
		ingestionRootKey, taskLabel, localDependOnIngestionJobKeyExecution
	);
	if (!taskLabel.empty())
		(mapLabelAndIngestionJobKey[taskLabel]).push_back(localDependOnIngestionJobKeyExecution);

	{
		/*
		if (responseBody != "")
				responseBody += ", ";
		responseBody +=
				(string("{ ")
						+ "\"ingestionJobKey\": " +
		to_string(localDependOnIngestionJobKeyExecution) + ", "
						+ "\"label\": \"" + taskLabel + "\" "
						+ "}");
		*/
		json localresponseBodyTaskRoot;
		localresponseBodyTaskRoot["ingestionJobKey"] = localDependOnIngestionJobKeyExecution;
		localresponseBodyTaskRoot["label"] = taskLabel;
		localresponseBodyTaskRoot["type"] = type;
		responseBodyTasksRoot.push_back(localresponseBodyTaskRoot);
	}

	vector<int64_t> localDependOnIngestionJobKeysForStarting;
	vector<int64_t> localDependOnIngestionJobKeysOverallInput;
	localDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKeyExecution);
	localDependOnIngestionJobKeysOverallInput.push_back(localDependOnIngestionJobKeyExecution);

	// 2022-03-15: Let's say we have a Task A and on his error we have Task B.
	//		When Task A fails, it will not generate any output and the Task
	// B, 		configured OnError, will not receive any input. 		For this reason, only
	// in case of a failure (onError), the overall input 		for the Task B has to be
	// the same input of the Task A. 		For this reason, in ingestionEvents, I added
	// the next parameter 		(dependOnIngestionJobKeysOverallInputOnError) 		to be
	// used for the OnError Task. 		We added this change because, in the 'Best
	// Picture Of Video' WorkflowLibrary, 		in case of the 'Face Recognition'
	// failure, the 'Frame' OnError task was not 		receiving any input. Now with
	// this fix/change, it works and the 'Frame' task 		is receiving the same input
	// of the 'Face Recognition' task.
	// 2022-04-29: Now we have the following scenario:
	//		CheckStreaming task and, on error, the emailNotification task.
	//		In this scenario, it not important, as in the previous comment
	//(2022-03-15), 		that the emailNotification task receives the same input of
	// the parent task 		also because the CheckStreaming task does not have any
	// input. 		It is important that the emailNotification task receives the
	//		ReferenceIngestionJobKey of the CheckStreaming task.
	//		This is used by the emailNotification task to retrieve the
	// information of the 		parent task (CheckStreaming task) and prepare for the
	// right substitution 		(checkStreaming_streamingName, ...) 		For this reason, we
	// are adding here, also the ReferenceIngestionJobKey 		of the parent task. 		So,
	// in case of OnError, the task (in our case the emailNotification task) 		will
	// receive as input:
	//		1. the ReferenceIngestionJobKey of the granparent, in order to
	// received 			the same input by his parent (scenario of the 2022-03-15 comment
	//		2. the ReferenceIngestionJobKey of the parent (this comment)
	vector<int64_t> dependOnIngestionJobKeysOverallInputOnError = dependOnIngestionJobKeysOverallInput;
	dependOnIngestionJobKeysOverallInputOnError.insert(
		dependOnIngestionJobKeysOverallInputOnError.end(), localDependOnIngestionJobKeysOverallInput.begin(),
		localDependOnIngestionJobKeysOverallInput.end()
	);

	vector<int64_t> referencesOutputIngestionJobKeys;
#ifdef __POSTGRES__
	ingestionEvents(
		trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, localDependOnIngestionJobKeysForStarting,
		localDependOnIngestionJobKeysOverallInput,

		dependOnIngestionJobKeysOverallInputOnError,

		referencesOutputIngestionJobKeys,

		mapLabelAndIngestionJobKey, /* responseBody, */ responseBodyTasksRoot
	);
#else
	ingestionEvents(
		conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, localDependOnIngestionJobKeysForStarting,
		localDependOnIngestionJobKeysOverallInput,

		dependOnIngestionJobKeysOverallInputOnError,

		referencesOutputIngestionJobKeys,

		mapLabelAndIngestionJobKey, /* responseBody, */ responseBodyTasksRoot
	);
#endif

	return localDependOnIngestionJobKeysForStarting;
}

#ifdef __POSTGRES__
vector<int64_t> API::ingestionGroupOfTasks(
	PostgresConnTrans &trans, int64_t userKey, string apiKey, const shared_ptr<Workspace>& workspace, int64_t ingestionRootKey, json &groupOfTasksRoot,
	vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess, vector<int64_t> dependOnIngestionJobKeysOverallInput,
	unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
	/* string& responseBody, */ json &responseBodyTasksRoot
)
#else
vector<int64_t> API::ingestionGroupOfTasks(
	shared_ptr<MySQLConnection> conn, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
	json &groupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
	vector<int64_t> dependOnIngestionJobKeysOverallInput, unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
	/* string& responseBody, */ json &responseBodyTasksRoot
)
#endif
{

	string type = "GroupOfTasks";

	string groupOfTaskLabel;
	string field = "label";
	groupOfTaskLabel = JSONUtils::asString(groupOfTasksRoot, field, "");

	SPDLOG_INFO(
		"Processing GroupOfTasks..."
		", ingestionRootKey: {}"
		", groupOfTaskLabel: {}",
		ingestionRootKey, groupOfTaskLabel
	);

	// initialize parametersRoot
	field = "parameters";
	if (!JSONUtils::isMetadataPresent(groupOfTasksRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	json &parametersRoot = groupOfTasksRoot[field];

	bool parallelTasks;

	field = "executionType";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	string executionType = JSONUtils::asString(parametersRoot, field, "");
	if (executionType == "parallel")
		parallelTasks = true;
	else if (executionType == "sequential")
		parallelTasks = false;
	else
	{
		string errorMessage = __FILEREF__ + "executionType field is wrong" + ", executionType: " + executionType;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	field = "tasks";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	json &tasksRoot = parametersRoot[field];

	/* 2021-02-20: A group that does not have any Task couls be a scenario,
	 * so we do not have to raise an error. Same check commented in
Validation.cpp if (tasksRoot.size() == 0)
{
	string errorMessage = __FILEREF__ + "No Tasks are present inside the
GroupOfTasks item"; SPDLOG_ERROR(errorMessage);

	throw runtime_error(errorMessage);
}
	*/

	// vector<int64_t> newDependOnIngestionJobKeysForStarting;
	vector<int64_t> newDependOnIngestionJobKeysOverallInputBecauseOfTasks;
	vector<int64_t> newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput;
	vector<int64_t> lastDependOnIngestionJobKeysForStarting;

	// dependOnSuccess for the Tasks
	// case 1: parent (IngestionJob or Group of Tasks) On Success --->
	// GroupOfTasks
	//		in this case the Tasks will be executed depending the status of
	// the parent, 		if success, the Tasks have to be executed. 		So
	// dependOnSuccessForTasks = dependOnSuccess
	// case 2: parent Tasks of a Group of Tasks ---> GroupOfTasks (destination)
	//		In this case, if the parent Group of Tasks is executed, also the
	// GroupOfTasks (destination) 		has to be executed 		So dependOnSuccessForTasks =
	//-1 (OnComplete)
	for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
	{
		json &taskRoot = tasksRoot[taskIndex];

		string field = "type";
		if (!JSONUtils::isMetadataPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::asString(taskRoot, field, "");

		vector<int64_t> localIngestionTaskDependOnIngestionJobKeyExecution;
		if (parallelTasks)
		{
			if (taskType == "GroupOfTasks")
			{
				int localDependOnSuccess = -1;

#ifdef __POSTGRES__
				localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
					trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#else
				localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
					conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#endif
			}
			else
			{
#ifdef __POSTGRES__
				localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
					trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
					dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#else
				localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
					conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
					dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#endif
			}
		}
		else
		{
			if (taskIndex == 0)
			{
				if (taskType == "GroupOfTasks")
				{
					int localDependOnSuccess = -1;

#ifdef __POSTGRES__
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
						trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#else
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
						conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#endif
				}
				else
				{
#ifdef __POSTGRES__
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
						trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#else
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
						conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#endif
				}
			}
			else
			{
				int localDependOnSuccess = -1;

				if (taskType == "GroupOfTasks")
				{
#ifdef __POSTGRES__
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
						trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, lastDependOnIngestionJobKeysForStarting, localDependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#else
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
						conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, lastDependOnIngestionJobKeysForStarting, localDependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#endif
				}
				else
				{
#ifdef __POSTGRES__
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
						trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, lastDependOnIngestionJobKeysForStarting, localDependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#else
					localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
						conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, lastDependOnIngestionJobKeysForStarting, localDependOnSuccess,
						dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
						/* responseBody, */ responseBodyTasksRoot
					);
#endif
				}
			}

			lastDependOnIngestionJobKeysForStarting = localIngestionTaskDependOnIngestionJobKeyExecution;
		}

		for (int64_t localDependOnIngestionJobKey : localIngestionTaskDependOnIngestionJobKeyExecution)
		{
			// newDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKey);
			newDependOnIngestionJobKeysOverallInputBecauseOfTasks.push_back(localDependOnIngestionJobKey);
		}
	}

	vector<int64_t> referencesOutputIngestionJobKeys;

	// The GroupOfTasks output (media) can be:
	// 1. the one generated by the first level of Tasks
	// (newDependOnIngestionJobKeysOverallInputBecauseOfTasks)
	// 2. the one specified by the ReferencesOutput tag
	// (newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput)
	//
	// In case of 1. it is needed to add the ReferencesOutput tag into the
	// metadata json and fill it with the
	// newDependOnIngestionJobKeysOverallInputBecauseOfTasks data In case of 2.,
	// ReferencesOutput is already into the metadata json. In case
	// ReferenceLabel is used, we have to change them with
	// ReferenceIngestionJobKey
	bool referencesOutputPresent = false;
	{
		// initialize referencesRoot
		json referencesOutputRoot = json::array();

		field = "referencesOutput";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			referencesOutputRoot = parametersRoot[field];

			referencesOutputPresent = !referencesOutputRoot.empty();
		}

		// manage ReferenceOutputLabel, inside the References Tag, If present
		// ReferenceLabel, replace it with ReferenceIngestionJobKey
		if (referencesOutputPresent)
		{
			// GroupOfTasks will wait only the specified ReferencesOutput. For
			// this reason we replace the ingestionJobKeys into
			// newDependOnIngestionJobKeysOverallInput with the one of
			// ReferencesOutput

			for (int referenceIndex = 0; referenceIndex < referencesOutputRoot.size(); ++referenceIndex)
			{
				json referenceOutputRoot = referencesOutputRoot[referenceIndex];

				field = "label";
				if (JSONUtils::isMetadataPresent(referenceOutputRoot, field))
				{
					string referenceLabel = JSONUtils::asString(referenceOutputRoot, field, "");

					if (referenceLabel.empty())
					{
						string errorMessage = __FILEREF__ + "The 'label' value cannot be empty" + ", referenceLabel: " + referenceLabel;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

					if (ingestionJobKeys.empty())
					{
						string errorMessage = __FILEREF__ + "The 'label' value is not found" + ", referenceLabel: " + referenceLabel +
											  ", groupOfTasksRoot: " + JSONUtils::toString(groupOfTasksRoot);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					else if (ingestionJobKeys.size() > 1)
					{
						string errorMessage = __FILEREF__ +
											  "The 'label' value cannot be used in more than one "
											  "Task" +
											  ", referenceLabel: " + referenceLabel +
											  ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size());
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					field = "ingestionJobKey";
					referenceOutputRoot[field] = ingestionJobKeys.back();

					referencesOutputRoot[referenceIndex] = referenceOutputRoot;

					field = "referencesOutput";
					parametersRoot[field] = referencesOutputRoot;

					newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.push_back(ingestionJobKeys.back());

					referencesOutputIngestionJobKeys.push_back(ingestionJobKeys.back());
				}
			}
		}
		else if (newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size() > 0)
		{
			SPDLOG_INFO(
				"add to referencesOutputRoot all the inherited references?"
				", ingestionRootKey: {}"
				", groupOfTaskLabel: {}"
				", referencesOutputPresent: {}"
				", newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(): {}",
				ingestionRootKey, groupOfTaskLabel, referencesOutputPresent, newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size()
			);

			// Enter here if No ReferencesOutput tag is present (so we have to
			// add the inherit input) OR we want to add dependOnReferences to
			// the Raferences tag

			for (int64_t & newDependOnIngestionJobKeysOverallInputBecauseOfTask : newDependOnIngestionJobKeysOverallInputBecauseOfTasks)
			{
				json referenceOutputRoot;
				field = "ingestionJobKey";
				referenceOutputRoot[field] = newDependOnIngestionJobKeysOverallInputBecauseOfTask;

				referencesOutputRoot.push_back(referenceOutputRoot);

				referencesOutputIngestionJobKeys.push_back(newDependOnIngestionJobKeysOverallInputBecauseOfTask);
			}

			SPDLOG_INFO(
				"Since ReferencesOutput is not present, set automatically the ReferencesOutput array tag using the ingestionJobKey of the Tasks"
				", ingestionRootKey: {}"
				", groupOfTaskLabel: {}"
				", newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(): {}"
				", referencesOutputRoot.size: {}",
				ingestionRootKey, groupOfTaskLabel, newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(), referencesOutputRoot.size()
			);

			field = "referencesOutput";
			parametersRoot[field] = referencesOutputRoot;
			/*
			field = "parameters";
			if (!parametersSectionPresent)
			{
					groupOfTaskRoot[field] = parametersRoot;
			}
			*/
		}
	}

	string processingStartingFrom;
	{
		field = "processingStartingFrom";
		processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");

		if (processingStartingFrom.empty())
		{
			tm tmUTCDateTime;
			// char sProcessingStartingFrom[64];
			string sProcessingStartingFrom;

			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcNow = chrono::system_clock::to_time_t(now);

			gmtime_r(&utcNow, &tmUTCDateTime);
			/*
			sprintf(
				sProcessingStartingFrom, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmUTCDateTime.tm_year + 1900, tmUTCDateTime.tm_mon + 1,
				tmUTCDateTime.tm_mday, tmUTCDateTime.tm_hour, tmUTCDateTime.tm_min, tmUTCDateTime.tm_sec
			);
			*/
			sProcessingStartingFrom = std::format(
				"{:0>4}-{:0>2}-{:0>2}T{:0>2}:{:0>2}:{:0>2}Z", tmUTCDateTime.tm_year + 1900, tmUTCDateTime.tm_mon + 1, tmUTCDateTime.tm_mday,
				tmUTCDateTime.tm_hour, tmUTCDateTime.tm_min, tmUTCDateTime.tm_sec
			);

			processingStartingFrom = sProcessingStartingFrom;
		}
	}

	string taskMetadata;
	{
		taskMetadata = JSONUtils::toString(parametersRoot);
	}

	SPDLOG_INFO(
		"add IngestionJob (Group of Tasks)"
		", ingestionRootKey: {}"
		", groupOfTaskLabel: {}"
		", taskMetadata: {}"
		", IngestionType: {}"
		", processingStartingFrom: {}"
		", newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(): {}"
		", newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.size(): {}"
		", dependOnSuccess: {}"
		", referencesOutputPresent: {}",
		ingestionRootKey, groupOfTaskLabel, taskMetadata, type, processingStartingFrom, newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(),
		newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.size(), dependOnSuccess, referencesOutputPresent
	);

	// - By default we fill newDependOnIngestionJobKeysOverallInput with the
	// ingestionJobKeys
	//		of the first level of Tasks to be executed by the Group of Tasks
	// - dependOnSuccess: we have to set it to -1, otherwise,
	//		if the dependent job will fail and the dependency is OnSuccess
	// or viceversa, 		the GroupOfTasks will not be executed
	vector<int64_t> waitForGlobalIngestionJobKeys;
#ifdef __POSTGRES__
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(
		trans, workspace->_workspaceKey, ingestionRootKey, groupOfTaskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type),
		processingStartingFrom,
		referencesOutputPresent ? newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput
								: newDependOnIngestionJobKeysOverallInputBecauseOfTasks,
		dependOnSuccess, waitForGlobalIngestionJobKeys
	);
#else
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(
		conn, workspace->_workspaceKey, ingestionRootKey, groupOfTaskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type),
		processingStartingFrom,
		referencesOutputPresent ? newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput
								: newDependOnIngestionJobKeysOverallInputBecauseOfTasks,
		dependOnSuccess, waitForGlobalIngestionJobKeys
	);
#endif
	field = "ingestionJobKey";
	groupOfTasksRoot[field] = localDependOnIngestionJobKeyExecution;

	// for each group of tasks child, the group of tasks (parent)
	// IngestionJobKey is set
	{
		int64_t parentGroupOfTasksIngestionJobKey = localDependOnIngestionJobKeyExecution;
		for (int64_t childIngestionJobKey : newDependOnIngestionJobKeysOverallInputBecauseOfTasks)
		{
#ifdef __POSTGRES__
			_mmsEngineDBFacade->updateIngestionJobParentGroupOfTasks(trans, childIngestionJobKey, parentGroupOfTasksIngestionJobKey);
#else
			_mmsEngineDBFacade->updateIngestionJobParentGroupOfTasks(conn, childIngestionJobKey, parentGroupOfTasksIngestionJobKey);
#endif
		}
	}

	SPDLOG_INFO(
		"Save Label..."
		", ingestionRootKey: {}"
		", groupOfTaskLabel: {}"
		", localDependOnIngestionJobKeyExecution: {}",
		ingestionRootKey, groupOfTaskLabel, localDependOnIngestionJobKeyExecution
	);
	if (!groupOfTaskLabel.empty())
		(mapLabelAndIngestionJobKey[groupOfTaskLabel]).push_back(localDependOnIngestionJobKeyExecution);

	{
		/*
		if (responseBody != "")
				responseBody += ", ";
		responseBody +=
						(string("{ ")
						+ "\"ingestionJobKey\": " +
		to_string(localDependOnIngestionJobKeyExecution) + ", "
						+ "\"label\": \"" + groupOfTaskLabel + "\" "
						+ "}");
		*/
		json localresponseBodyTaskRoot;
		localresponseBodyTaskRoot["ingestionJobKey"] = localDependOnIngestionJobKeyExecution;
		localresponseBodyTaskRoot["label"] = groupOfTaskLabel;
		localresponseBodyTaskRoot["type"] = type;
		responseBodyTasksRoot.push_back(localresponseBodyTaskRoot);
	}

	/*
	 * 2019-10-01.
	 *		We have the following workflow:
	 *			GroupOfTasks to execute three Cuts (the three cuts have
	 *retention set to 0). OnSuccess of the GroupOfTasks we have the Concat of
	 *the three Cuts
	 *
	 *			Here we are managing the GroupOfTasks and, in the below
	 *ingestionEvents, we are passing as dependencies, just the ingestionJobKey
	 *of the GroupOfTasks. In this case, we may have the following scenario:
	 *				1. MMSEngine first execute the three Cuts
	 *				2. MMSEngine execute the GroupOfTasks
	 *				3. MMSEngine starts the retention check and
	 *remove the three cuts. This is because the GroupOfTasks is executed and
	 *the three cuts does not have any other dependencies
	 *				4. MMSEngine executes the Concat and fails
	 *because there are no cuts anymore
	 *
	 *		Actually the Tasks (1) specified by OnSuccess/OnError/OnComplete
	 *of the GroupOfTasks depend just on the GroupOfTasks IngestionJobKey. We
	 *need to add the ONCOMPLETE dependencies between the Tasks just mentioned
	 *above (1) and the ReferencesOutput of the GroupOfTasks. This will solve
	 *the issue above. It is important that the dependency is ONCOMPLETE. This
	 *because otherwise, if the dependency is OnSuccess and a ReferenceOutput
	 *fails, the Tasks (1) will be marked as End_NotToBeExecuted and we do not
	 *want this because the execution or not of the Task has to be decided ONLY
	 *by the logic inside the GroupOfTasks and not by the ReferenceOutput Task.
	 *
	 *		To implement that, we provide, as input parameter, the
	 *ReferencesOutput to the ingestionEvents method. The ingestionEvents add
	 *the dependencies, OnComplete, between the Tasks (1) and the
	 *ReferencesOutput.
	 *
	 */
	vector<int64_t> localDependOnIngestionJobKeysForStarting;
	localDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKeyExecution);

	// 2022-03-15: Let's say we have a Task A and on his error we have Task B.
	//		When Task A fails, it will not generate any output and the Task
	// B, 		configured OnError, will not receive any input. 		For this reason, only
	// in case of a failure (onError), the overall input 		for the Task B has to be
	// the same input of the Task A. 		For this reason, in ingestionEvents, I added
	// the next parameter 		(dependOnIngestionJobKeysOverallInputOnError) 		to be
	// used for the OnError Task. 		We added this change because, in the 'Best
	// Picture Of Video' WorkflowLibrary, 		in case of the 'Face Recognition'
	// failure, the 'Frame' OnError task was not 		receiving any input. Now with
	// this fix/change, it works and the 'Frame' task 		is receiving the same input
	// of the 'Face Recognition' task.
	// 2022-04-29: Now we have the following scenario:
	//		CheckStreaming task and, on error, the emailNotification task.
	//		In this scenario, it not important, as in the previous comment
	//(2022-03-15), 		that the emailNotification task receives the same input of
	// the parent task 		also because the CheckStreaming task does not have any
	// input. 		It is important that the emailNotification task receives the
	//		ReferenceIngestionJobKey of the CheckStreaming task.
	//		This is used by the emailNotification task to retrieve the
	// information of the 		parent task (CheckStreaming task) and prepare for the
	// right substitution 		(checkStreaming_streamingName, ...) 		For this reason, we
	// are adding here, also the ReferenceIngestionJobKey 		of the parent task. 		So,
	// in case of OnError, the task (in our case the emailNotification task) 		will
	// receive as input:
	//		1. the ReferenceIngestionJobKey of the granparent, in order to
	// received 			the same input by his parent (scenario of the 2022-03-15 comment
	//		2. the ReferenceIngestionJobKey of the parent (this comment)
	vector<int64_t> dependOnIngestionJobKeysOverallInputOnError = dependOnIngestionJobKeysOverallInput;
	dependOnIngestionJobKeysOverallInputOnError.insert(
		dependOnIngestionJobKeysOverallInputOnError.end(), localDependOnIngestionJobKeysForStarting.begin(),
		localDependOnIngestionJobKeysForStarting.end()
	);

#ifdef __POSTGRES__
	ingestionEvents(
		trans, userKey, apiKey, workspace, ingestionRootKey, groupOfTasksRoot, localDependOnIngestionJobKeysForStarting,
		localDependOnIngestionJobKeysForStarting,

		dependOnIngestionJobKeysOverallInputOnError,

		referencesOutputIngestionJobKeys, mapLabelAndIngestionJobKey,
		/* responseBody, */ responseBodyTasksRoot
	);
#else
	ingestionEvents(
		conn, userKey, apiKey, workspace, ingestionRootKey, groupOfTasksRoot, localDependOnIngestionJobKeysForStarting,
		localDependOnIngestionJobKeysForStarting,

		dependOnIngestionJobKeysOverallInputOnError,

		referencesOutputIngestionJobKeys, mapLabelAndIngestionJobKey,
		/* responseBody, */ responseBodyTasksRoot
	);
#endif

	return localDependOnIngestionJobKeysForStarting;
}

#ifdef __POSTGRES__
void API::ingestionEvents(
	PostgresConnTrans &trans, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &taskOrGroupOfTasksRoot,
	vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
	vector<int64_t> dependOnIngestionJobKeysOverallInputOnError, vector<int64_t> &referencesOutputIngestionJobKeys,
	unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
	/* string& responseBody, */ json &responseBodyTasksRoot
)
#else
void API::ingestionEvents(
	shared_ptr<MySQLConnection> conn, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
	json &taskOrGroupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
	vector<int64_t> dependOnIngestionJobKeysOverallInputOnError, vector<int64_t> &referencesOutputIngestionJobKeys,
	unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
	/* string& responseBody, */ json &responseBodyTasksRoot
)
#endif
{

	string field = "onSuccess";
	if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
	{
		json &onSuccessRoot = taskOrGroupOfTasksRoot[field];

		field = "task";
		if (!JSONUtils::isMetadataPresent(onSuccessRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onSuccessRoot[field];

		string field = "type";
		if (!JSONUtils::isMetadataPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::asString(taskRoot, field, "");

		field = "label";
		string taskLabel = JSONUtils::asString(taskRoot, field, "");

		vector<int64_t> localIngestionJobKeys;
		if (taskType == "GroupOfTasks")
		{
			int localDependOnSuccess = 1;
#ifdef __POSTGRES__
			localIngestionJobKeys = ingestionGroupOfTasks(
				trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			localIngestionJobKeys = ingestionGroupOfTasks(
				conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}
		else
		{
			/*
			// just logs
			{
					string sDependOnIngestionJobKeysForStarting;
					for (int64_t key: dependOnIngestionJobKeysForStarting)
							sDependOnIngestionJobKeysForStarting += (string(",")
			+ to_string(key)); string sDependOnIngestionJobKeysOverallInput; for
			(int64_t key: dependOnIngestionJobKeysOverallInput)
							sDependOnIngestionJobKeysOverallInput +=
			(string(",") + to_string(key)); _logger->error(__FILEREF__ +
			"ingestionSingleTask (OnSuccess)"
							+ ", ingestionRootKey: " +
			to_string(ingestionRootKey)
							+ ", taskType: " + taskType
							+ ", taskLabel: " + taskLabel
							+ ", sDependOnIngestionJobKeysForStarting: " +
			sDependOnIngestionJobKeysForStarting
							+ ", sDependOnIngestionJobKeysOverallInput: " +
			sDependOnIngestionJobKeysOverallInput
					);
			}
			*/
			int localDependOnSuccess = 1;
#ifdef __POSTGRES__
			localIngestionJobKeys = ingestionSingleTask(
				trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			localIngestionJobKeys = ingestionSingleTask(
				conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}

		// to understand the reason I'm adding these dependencies, look at the
		// comment marked as '2019-10-01' inside the ingestionGroupOfTasks
		// method
		{
			int dependOnSuccess = -1; // OnComplete
			int orderNumber = -1;
			bool referenceOutputDependency = true;

			for (int64_t localIngestionJobKey : localIngestionJobKeys)
			{
				for (int64_t localReferenceOutputIngestionJobKey : referencesOutputIngestionJobKeys)
				{
#ifdef __POSTGRES__
					_mmsEngineDBFacade->addIngestionJobDependency(
						trans, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber, referenceOutputDependency
					);
#else
					_mmsEngineDBFacade->addIngestionJobDependency(
						conn, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber, referenceOutputDependency
					);
#endif
				}
			}
		}
	}

	field = "onError";
	if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
	{
		json &onErrorRoot = taskOrGroupOfTasksRoot[field];

		field = "task";
		if (!JSONUtils::isMetadataPresent(onErrorRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onErrorRoot[field];

		string field = "type";
		if (!JSONUtils::isMetadataPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::asString(taskRoot, field, "");

		field = "label";
		string taskLabel = JSONUtils::asString(taskRoot, field, "");

		vector<int64_t> localIngestionJobKeys;
		if (taskType == "GroupOfTasks")
		{
			int localDependOnSuccess = 0;
#ifdef __POSTGRES__
			localIngestionJobKeys = ingestionGroupOfTasks(
				trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				// dependOnIngestionJobKeysOverallInput,
				// mapLabelAndIngestionJobKey,
				dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			localIngestionJobKeys = ingestionGroupOfTasks(
				conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				// dependOnIngestionJobKeysOverallInput,
				// mapLabelAndIngestionJobKey,
				dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}
		else
		{
			/*
			// just logs
			{
					string sDependOnIngestionJobKeysForStarting;
					for (int64_t key: dependOnIngestionJobKeysForStarting)
							sDependOnIngestionJobKeysForStarting += (string(",")
			+ to_string(key)); string
			sDependOnIngestionJobKeysOverallInputOnError; for (int64_t key:
			dependOnIngestionJobKeysOverallInputOnError)
							sDependOnIngestionJobKeysOverallInputOnError +=
			(string(",") + to_string(key)); _logger->error(__FILEREF__ +
			"ingestionSingleTask (OnError)"
							+ ", ingestionRootKey: " +
			to_string(ingestionRootKey)
							+ ", taskType: " + taskType
							+ ", taskLabel: " + taskLabel
							+ ", sDependOnIngestionJobKeysForStarting: " +
			sDependOnIngestionJobKeysForStarting
							+ ", sDependOnIngestionJobKeysOverallInputOnError: "
			+ sDependOnIngestionJobKeysOverallInputOnError
					);
			}
			*/
			int localDependOnSuccess = 0;
#ifdef __POSTGRES__
			localIngestionJobKeys = ingestionSingleTask(
				trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				// dependOnIngestionJobKeysOverallInput,
				// mapLabelAndIngestionJobKey,
				dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			localIngestionJobKeys = ingestionSingleTask(
				conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				// dependOnIngestionJobKeysOverallInput,
				// mapLabelAndIngestionJobKey,
				dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}

		// to understand the reason I'm adding these dependencies, look at the
		// comment marked as '2019-10-01' inside the ingestionGroupOfTasks
		// method
		{
			int dependOnSuccess = -1; // OnComplete
			int orderNumber = -1;
			bool referenceOutputDependency = true;

			for (int64_t localIngestionJobKey : localIngestionJobKeys)
			{
				for (int64_t localReferenceOutputIngestionJobKey : referencesOutputIngestionJobKeys)
				{
#ifdef __POSTGRES__
					_mmsEngineDBFacade->addIngestionJobDependency(
						trans, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber, referenceOutputDependency
					);
#else
					_mmsEngineDBFacade->addIngestionJobDependency(
						conn, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber, referenceOutputDependency
					);
#endif
				}
			}
		}
	}

	field = "onComplete";
	if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
	{
		json &onCompleteRoot = taskOrGroupOfTasksRoot[field];

		field = "task";
		if (!JSONUtils::isMetadataPresent(onCompleteRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onCompleteRoot[field];

		string field = "type";
		if (!JSONUtils::isMetadataPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::asString(taskRoot, field, "");

		vector<int64_t> localIngestionJobKeys;
		if (taskType == "GroupOfTasks")
		{
			int localDependOnSuccess = -1;
#ifdef __POSTGRES__
			localIngestionJobKeys = ingestionGroupOfTasks(
				trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			localIngestionJobKeys = ingestionGroupOfTasks(
				conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}
		else
		{
			int localDependOnSuccess = -1;
#ifdef __POSTGRES__
			localIngestionJobKeys = ingestionSingleTask(
				trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#else
			localIngestionJobKeys = ingestionSingleTask(
				conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
				dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
				/* responseBody, */ responseBodyTasksRoot
			);
#endif
		}

		// to understand the reason I'm adding these dependencies, look at the
		// comment marked as '2019-10-01' inside the ingestionGroupOfTasks
		// method
		{
			int dependOnSuccess = -1; // OnComplete
			int orderNumber = -1;
			bool referenceOutputDependency = true;

			for (int64_t localIngestionJobKey : localIngestionJobKeys)
			{
				for (int64_t localReferenceOutputIngestionJobKey : referencesOutputIngestionJobKeys)
				{
#ifdef __POSTGRES__
					_mmsEngineDBFacade->addIngestionJobDependency(
						trans, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber, referenceOutputDependency
					);
#else
					_mmsEngineDBFacade->addIngestionJobDependency(
						conn, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber, referenceOutputDependency
					);
#endif
				}
			}
		}
	}
}

void API::uploadedBinary(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "uploadedBinary";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	try
	{
		if (_noFileSystemAccess)
		{
			string errorMessage = string("no rights to execute this method") + ", _noFileSystemAccess: " + to_string(_noFileSystemAccess);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		// sourceBinaryPathFile will be something like:
		// /var/catramms/storage/nginxWorkingAreaRepository/0000001023
		string sourceBinaryPathFile = requestData.getHeaderParameter("x-file", string(""), true);

		// Content-Range: bytes 0-99999/100000
		bool contentRangePresent = false;
		uint64_t contentRangeStart = -1;
		uint64_t contentRangeEnd = -1;
		uint64_t contentRangeSize = -1;
		double uploadingProgress = 0.0;
		string contentRange = requestData.getHeaderParameter("content-range", string(""));
		if (!contentRange.empty())
		{
			try
			{
				FCGIRequestData::parseContentRange(contentRange, contentRangeStart, contentRangeEnd, contentRangeSize);

				// X : 100 = contentRangeEnd : contentRangeSize
				uploadingProgress = 100 * contentRangeEnd / contentRangeSize;

				contentRangePresent = true;
			}
			catch (exception &e)
			{
				string errorMessage = string("Content-Range is not well done. Expected format: "
											 "'Content-Range: bytes <start>-<end>/<size>'") +
									  ", contentRange: " + contentRange;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(apiAuthorizationDetails->workspace);
		string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
		bool segmentedContent = false;
		try
		{
			json parametersRoot = _mmsEngineDBFacade->ingestionJob_columnAsJson(
				apiAuthorizationDetails->workspace->_workspaceKey, "metaDataContent", ingestionJobKey,
				// 2022-12-18: l'ingestionJob potrebbe essere stato
				// appena aggiunto
				true
			);

			string field = "fileFormat";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string fileFormat = JSONUtils::asString(parametersRoot, field, "");
				// 2022-08-11: I guess the correct fileFormat is m3u8-tar.gz and
				// not m3u8 if (fileFormat == "m3u8")
				if (fileFormat == "m3u8-tar.gz")
					segmentedContent = true;
			}
		}
		catch (DBRecordNotFound &e)
		{
			string errorMessage = string("ingestionJob_MetadataContent failed") +
								  ", workspace->_workspaceKey: " + to_string(apiAuthorizationDetails->workspace->_workspaceKey) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
								  ", destBinaryPathName: " + destBinaryPathName + ", e.what: " + e.what();
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("ingestionJob_MetadataContent failed") +
								  ", workspace->_workspaceKey: " + to_string(apiAuthorizationDetails->workspace->_workspaceKey) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
								  ", destBinaryPathName: " + destBinaryPathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		if (segmentedContent)
			destBinaryPathName = destBinaryPathName + ".tar.gz";

		if (!contentRangePresent)
		{
			try
			{
				SPDLOG_INFO(
					"Moving file from nginx area to ingestion user area"
					", ingestionJobKey: {}"
					", sourceBinaryPathFile: {}"
					", destBinaryPathName: {}",
					ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
				);

				MMSStorage::move(ingestionJobKey, sourceBinaryPathFile, destBinaryPathName);
			}
			catch (exception &e)
			{
				string errorMessage = std::format(
					"Error to move file"
					", ingestionJobKey: {}"
					", sourceBinaryPathFile: {}"
					", destBinaryPathName: {}",
					ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			/* 2023-03-19: manageTarFileInCaseOfIngestionOfSegments viene fatto
					da MMSEngineProcessor::handleLocalAssetIngestionEventThread
					per evitare che questa API impiega minuti a terminare
			if (segmentedContent)
			{
					try
					{
							// by a convention, the directory inside the tar
		file has to be named as 'content' string localSourceBinaryPathFile =
		"/content.tar.gz";

							_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(ingestionJobKey,
											destBinaryPathName,
		workspaceIngestionRepository, localSourceBinaryPathFile);
					}
					catch(runtime_error& e)
					{
							string errorMessage =
		string("manageTarFileInCaseOfIngestionOfSegments failed")
									+ ", ingestionJobKey: " +
		to_string(ingestionJobKey)
							;
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
					}
			}
			*/

			bool sourceBinaryTransferred = true;
			SPDLOG_INFO(
				"Update IngestionJob"
				", ingestionJobKey: {}"
				", sourceBinaryTransferred: {}",
				ingestionJobKey, sourceBinaryTransferred
			);
			_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, sourceBinaryTransferred);
		}
		else
		{
			//  Content-Range is present

			if (fs::exists(destBinaryPathName))
			{
				if (contentRangeStart == 0)
				{
					// content is reset
					ofstream osDestStream(destBinaryPathName.c_str(), ofstream::binary | ofstream::trunc);

					osDestStream.close();
				}

#ifdef SAFEFILESYSTEMTHREAD
				unsigned long destBinaryPathNameSizeInBytes =
					SafeFileSystem::fileSizeThread(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
				unsigned long sourceBinaryPathFileSizeInBytes =
					SafeFileSystem::fileSizeThread(sourceBinaryPathFile, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
				unsigned long destBinaryPathNameSizeInBytes =
					SafeFileSystem::fileSizeProcess(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
				unsigned long sourceBinaryPathFileSizeInBytes =
					SafeFileSystem::fileSizeProcess(sourceBinaryPathFile, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
				unsigned long destBinaryPathNameSizeInBytes = fs::file_size(destBinaryPathName);
				unsigned long sourceBinaryPathFileSizeInBytes = fs::file_size(sourceBinaryPathFile);
#endif

				SPDLOG_INFO(
					"Content-Range before concat"
					", ingestionJobKey: {}"
					", contentRangeStart: {}"
					", contentRangeEnd: {}"
					", contentRangeSize: {}"
					", segmentedContent: {}"
					", destBinaryPathName: {}"
					", destBinaryPathNameSizeInBytes: {}"
					", sourceBinaryPathFile: {}"
					", sourceBinaryPathFileSizeInBytes: {}",
					ingestionJobKey, contentRangeStart, contentRangeEnd, contentRangeSize, segmentedContent, destBinaryPathName,
					destBinaryPathNameSizeInBytes, sourceBinaryPathFile, sourceBinaryPathFileSizeInBytes
				);

				// waiting in case of nfs delay
				chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
				while (contentRangeStart != destBinaryPathNameSizeInBytes && chrono::system_clock::now() < end)
				{
					this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));

#ifdef SAFEFILESYSTEMTHREAD
					destBinaryPathNameSizeInBytes =
						SafeFileSystem::fileSizeThread(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
					destBinaryPathNameSizeInBytes =
						SafeFileSystem::fileSizeProcess(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
					destBinaryPathNameSizeInBytes = fs::file_size(destBinaryPathName);
#endif
				}

				if (contentRangeStart != destBinaryPathNameSizeInBytes)
				{
					string errorMessage = std::format(
						"Content-Range. This is NOT the next expected "
						"chunk because Content-Range start is different "
						"from fileSizeInBytes"
						", ingestionJobKey: {}"
						", contentRangeStart: {}"
						", contentRangeEnd: {}"
						", contentRangeSize: {}"
						", segmentedContent: {}"
						", destBinaryPathName: {}"
						", sourceBinaryPathFile: {}"
						", sourceBinaryPathFileSizeInBytes: {}"
						", destBinaryPathNameSizeInBytes (expected): {}",
						ingestionJobKey, contentRangeStart, contentRangeEnd, contentRangeSize, segmentedContent, destBinaryPathName,
						sourceBinaryPathFile, sourceBinaryPathFileSizeInBytes, destBinaryPathNameSizeInBytes
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				try
				{
					// bool removeSrcFileAfterConcat = true;

					// FileIO::concatFile(destBinaryPathName,
					// sourceBinaryPathFile, removeSrcFileAfterConcat);

					// concat files, append a file to another
					chrono::system_clock::time_point start = chrono::system_clock::now();
					{
						ofstream ofDestination(destBinaryPathName, std::ios_base::binary | std::ios_base::app);
						ifstream ifSource(sourceBinaryPathFile, std::ios_base::binary);

						ofDestination << ifSource.rdbuf();

						ofDestination.close();
						ifSource.close();
					}

#ifdef SAFEFILESYSTEMTHREAD
					uintmax_t destBinaryPathNameSize =
						SafeFileSystem::fileSizeThread(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
					uintmax_t destBinaryPathNameSize =
						SafeFileSystem::fileSizeProcess(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
					uintmax_t destBinaryPathNameSize = fs::file_size(destBinaryPathName);
#endif
					SPDLOG_INFO(
						"Content-Range after concat"
						", ingestionJobKey: {}"
						", contentRangeStart: {}"
						", contentRangeEnd: {}"
						", contentRangeSize: {}"
						", segmentedContent: {}"
						", destBinaryPathName: {}"
						", destBinaryPathNameSizeInBytes: {}"
						", sourceBinaryPathFile: {}"
						", sourceBinaryPathFileSizeInBytes: {}"
						", concat elapsed (secs): {}",
						ingestionJobKey, contentRangeStart, contentRangeEnd, contentRangeSize, segmentedContent, destBinaryPathName,
						destBinaryPathNameSize, sourceBinaryPathFile, sourceBinaryPathFileSizeInBytes,
						chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - start).count()
					);

					fs::remove_all(sourceBinaryPathFile);
				}
				catch (exception &e)
				{
					string errorMessage = std::format(
						"Content-Range. Error to concat file"
						", ingestionJobKey: {}"
						", destBinaryPathName: {}"
						", sourceBinaryPathFile: {}",
						ingestionJobKey, destBinaryPathName, sourceBinaryPathFile
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				// binary file does not exist, so this is the first chunk

				if (contentRangeStart != 0)
				{
					string errorMessage = std::format(
						"Content-Range. This is the first chunk of the "
						"file and Content-Range start has to be 0"
						", ingestionJobKey: {}",
						", contentRangeStart: {}", ingestionJobKey, contentRangeStart
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				try
				{
					SPDLOG_INFO(
						"Content-Range. Moving file from nginx area to "
						"ingestion user area"
						", ingestionJobKey: {}"
						", sourceBinaryPathFile: {}"
						", destBinaryPathName: {}",
						ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
					);

					MMSStorage::move(ingestionJobKey, sourceBinaryPathFile, destBinaryPathName);
				}
				catch (exception &e)
				{
					string errorMessage = std::format(
						"Content-Range. Error to move file"
						", ingestionJobKey: {}"
						", sourceBinaryPathFile: {}"
						", destBinaryPathName: {}",
						ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			if (contentRangeEnd + 1 == contentRangeSize)
			{
				/* 2023-03-19: manageTarFileInCaseOfIngestionOfSegments viene
				fatto da
				MMSEngineProcessor::handleLocalAssetIngestionEventThread per
				evitare che questa API impiega minuti a terminare if
				(segmentedContent)
				{
						try
						{
								// by a convention, the directory inside the tar
				file has to be named as 'content' string
				localSourceBinaryPathFile = "/content.tar.gz";

								_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(ingestionJobKey,
										destBinaryPathName,
				workspaceIngestionRepository, localSourceBinaryPathFile);
						}
						catch(runtime_error& e)
						{
								string errorMessage =
				string("manageTarFileInCaseOfIngestionOfSegments failed")
										+ ", ingestionJobKey: " +
				to_string(ingestionJobKey)
								;
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
						}
				}
				*/

				bool sourceBinaryTransferred = true;
				SPDLOG_INFO(
					"Content-Range. Update IngestionJob"
					", ingestionJobKey: {}"
					", sourceBinaryTransferred: {}",
					ingestionJobKey, sourceBinaryTransferred
				);
				_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, sourceBinaryTransferred);
			}
			else
			{
				SPDLOG_INFO(
					"Content-Range. Update IngestionJob (uploading progress)"
					", ingestionJobKey: {}"
					", uploadingProgress: {}",
					ingestionJobKey, uploadingProgress
				);
				_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(ingestionJobKey, uploadingProgress);
			}
		}

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::stopUploadFileProgressThread()
{
	_fileUploadProgressThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
}

void API::fileUploadProgressCheckThread()
{
	while (!_fileUploadProgressThreadShutdown)
	{
		this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));

		lock_guard<mutex> locker(_fileUploadProgressData->_mutex);

		for (auto itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.begin();
			 itr != _fileUploadProgressData->_filesUploadProgressToBeMonitored.end();)
		{
			bool iteratorAlreadyUpdated = false;

			if (itr->_callFailures >= _maxProgressCallFailures)
			{
				SPDLOG_ERROR(
					"fileUploadProgressCheckThread: remove entry because of too many call failures"
					", ingestionJobKey: {}"
					", progressId: {}"
					", binaryVirtualHostName: {}"
					", binaryListenHost: {}"
					", callFailures: {}"
					", _maxProgressCallFailures: {}",
					itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures,
					_maxProgressCallFailures
				);
				itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

				continue;
			}

			try
			{
				string progressURL = std::format("http://:{}:{}{}", itr->_binaryListenHost, _webServerPort, _progressURI);
				string progressIdHeader = std::format("X-Progress-ID: {}", itr->_progressId);
				string hostHeader = std::format("Host: {}", itr->_binaryVirtualHostName);

				SPDLOG_INFO(
					"Call for upload progress"
					", ingestionJobKey: {}"
					", progressId: {}"
					", binaryVirtualHostName: {}"
					", binaryListenHost: {}"
					", callFailures: {}"
					", progressURL: {}"
					", progressIdHeader: {}"
					", hostHeader: {}",
					itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures, progressURL,
					progressIdHeader, hostHeader
				);

				vector<string> otherHeaders;
				otherHeaders.push_back(progressIdHeader);
				otherHeaders.push_back(hostHeader); // important for the nginx virtual host
				int curlTimeoutInSeconds = 120;
				json uploadProgressResponse = CurlWrapper::httpGetJson(
					progressURL, curlTimeoutInSeconds, "", otherHeaders, std::format(", ingestionJobKey: {}", itr->_ingestionJobKey)
				);

				try
				{
					// json uploadProgressResponse =
					// JSONUtils::toJson(-1, -1, sResponse);

					// { "state" : "uploading", "received" : 731195032, "size" :
					// 745871360 } At the end: { "state" : "done" } In case of
					// error: { "state" : "error", "status" : 500 }
					string state = JSONUtils::asString(uploadProgressResponse, "state", "");
					if (state == "done")
					{
						double relativeProgress = 100.0;
						double relativeUploadingPercentage = 100.0;

						int64_t absoluteReceived = -1;
						if (itr->_contentRangePresent)
							absoluteReceived = itr->_contentRangeEnd;
						int64_t absoluteSize = -1;
						if (itr->_contentRangePresent)
							absoluteSize = itr->_contentRangeSize;

						double absoluteProgress;
						if (itr->_contentRangePresent)
							absoluteProgress = (static_cast<double>(absoluteReceived) / static_cast<double>(absoluteSize)) * 100;

						// this is to have one decimal in the percentage
						double absoluteUploadingPercentage;
						if (itr->_contentRangePresent)
							absoluteUploadingPercentage = static_cast<double>(static_cast<int>(absoluteProgress * 10)) / 10;

						if (itr->_contentRangePresent)
						{
							SPDLOG_INFO(
								"Upload just finished"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeProgress: {}"
								", relativeUploadingPercentage: {}"
								", absoluteProgress: {}"
								", absoluteUploadingPercentage: {}"
								", lastPercentageUpdated: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								relativeUploadingPercentage, absoluteProgress, absoluteUploadingPercentage, itr->_lastPercentageUpdated
							);
						}
						else
						{
							SPDLOG_INFO(
								"Upload just finished"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeProgress: {}"
								", relativeUploadingPercentage: {}"
								", lastPercentageUpdated: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								relativeUploadingPercentage, itr->_lastPercentageUpdated
							);
						}

						if (itr->_contentRangePresent)
						{
							SPDLOG_INFO(
								"Update IngestionJob"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", absoluteUploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
								absoluteUploadingPercentage
							);
							_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, absoluteUploadingPercentage);
						}
						else
						{
							SPDLOG_INFO(
								"Update IngestionJob"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeUploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
								relativeUploadingPercentage
							);
							_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, relativeUploadingPercentage);
						}

						itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

						iteratorAlreadyUpdated = true;
					}
					else if (state == "error")
					{
						SPDLOG_ERROR(
							"fileUploadProgressCheckThread: remove entry because state is 'error'"
							", ingestionJobKey: {}"
							", progressId: {}"
							", binaryVirtualHostName: {}"
							", binaryListenHost: {}"
							", callFailures: {}"
							", _maxProgressCallFailures: {}",
							itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures,
							_maxProgressCallFailures
						);
						itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

						iteratorAlreadyUpdated = true;
					}
					else if (state == "uploading")
					{
						int64_t relativeReceived = JSONUtils::asInt64(uploadProgressResponse, "received", 0);
						int64_t absoluteReceived = -1;
						if (itr->_contentRangePresent)
							absoluteReceived = relativeReceived + itr->_contentRangeStart;
						int64_t relativeSize = JSONUtils::asInt64(uploadProgressResponse, "size", 0);
						int64_t absoluteSize = -1;
						if (itr->_contentRangePresent)
							absoluteSize = itr->_contentRangeSize;

						double relativeProgress = ((double)relativeReceived / (double)relativeSize) * 100;
						double absoluteProgress;
						if (itr->_contentRangePresent)
							absoluteProgress = ((double)absoluteReceived / (double)absoluteSize) * 100;

						// this is to have one decimal in the percentage
						double relativeUploadingPercentage = ((double)((int)(relativeProgress * 10))) / 10;
						double absoluteUploadingPercentage;
						if (itr->_contentRangePresent)
							absoluteUploadingPercentage = ((double)((int)(absoluteProgress * 10))) / 10;

						if (itr->_contentRangePresent)
						{
							SPDLOG_INFO(
								"Upload still running"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeProgress: {}"
								", absoluteProgress: {}"
								", lastPercentageUpdated: {}"
								", relativeReceived: {}"
								", absoluteReceived: {}"
								", relativeSize: {}"
								", absoluteSize: {}"
								", relativeUploadingPercentage: {}"
								", absoluteUploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								absoluteProgress, itr->_lastPercentageUpdated, relativeReceived, absoluteReceived, relativeSize, absoluteSize,
								relativeUploadingPercentage, absoluteUploadingPercentage
							);
						}
						else
						{
							SPDLOG_INFO(
								"Upload still running"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", progress: {}"
								", lastPercentageUpdated: {}"
								", received: {}"
								", size: {}"
								", uploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								itr->_lastPercentageUpdated, relativeReceived, relativeSize, relativeUploadingPercentage
							);
						}

						if (itr->_contentRangePresent)
						{
							if (itr->_lastPercentageUpdated != absoluteUploadingPercentage)
							{
								SPDLOG_INFO(
									"Update IngestionJob"
									", ingestionJobKey: {}"
									", progressId: {}"
									", binaryVirtualHostName: {}"
									", binaryListenHost: {}"
									", absoluteUploadingPercentage: {}",
									itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
									absoluteUploadingPercentage
								);
								_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, absoluteUploadingPercentage);

								itr->_lastPercentageUpdated = absoluteUploadingPercentage;
							}
						}
						else
						{
							if (itr->_lastPercentageUpdated != relativeUploadingPercentage)
							{
								SPDLOG_INFO(
									"Update IngestionJob"
									", ingestionJobKey: {}"
									", progressId: {}"
									", binaryVirtualHostName: {}"
									", binaryListenHost: {}"
									", uploadingPercentage: {}",
									itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
									relativeUploadingPercentage
								);
								_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, relativeUploadingPercentage);

								itr->_lastPercentageUpdated = relativeUploadingPercentage;
							}
						}
					}
					else
					{
						string errorMessage = std::format(
							"file upload progress. State is wrong"
							", state: {}"
							", ingestionJobKey: {}"
							", progressId: {}"
							", binaryVirtualHostName: {}"
							", binaryListenHost: {}"
							", callFailures: {}"
							", progressURL: {}"
							", progressIdHeader: {}",
							state, itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures,
							progressURL, progressIdHeader
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				catch (...)
				{
					string errorMessage = "response Body json is not well format"
						// + ", sResponse: " + sResponse
						;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch (exception& e)
			{
				SPDLOG_ERROR(
					"Call for upload progress failed"
					", ingestionJobKey: {}"
					", progressId: {}"
					", binaryVirtualHostName: {}"
					", binaryListenHost: {}"
					", callFailures: {}"
					", exception: {}",
					itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures, e.what()
				);

				itr->_callFailures = itr->_callFailures + 1;
			}

			if (!iteratorAlreadyUpdated)
				++itr;
		}
	}
}

void API::ingestionRootsStatus(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestionRootsStatus";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t ingestionRootKey = requestData.getQueryParameter("ingestionRootKey", static_cast<int64_t>(-1), false);

		int64_t mediaItemKey = requestData.getQueryParameter("mediaItemKey", static_cast<int64_t>(-1), false);

		int32_t start = requestData.getQueryParameter("start", static_cast<int64_t>(0), false);

		int32_t rows = requestData.getQueryParameter("rows", static_cast<int64_t>(10), false);
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return
			// is much less

			// rows = _maxPageSize;

			string errorMessage = std::format(
				"rows parameter too big"
				", rows: {}"
				", _maxPageSize: {}",
				rows, _maxPageSize
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string startIngestionDate = requestData.getQueryParameter("startIngestionDate", "", false);

		string endIngestionDate = requestData.getQueryParameter("endIngestionDate", "", false);

		string label = requestData.getQueryParameter("label", "", false);

		string status = requestData.getQueryParameter("status", "all", false);

		bool hiddenToo = requestData.getQueryParameter("hiddenToo", true, false);

		bool asc = requestData.getQueryParameter("asc", true, false);

		bool ingestionJobOutputs = requestData.getQueryParameter("ingestionJobOutputs", true, false);

		bool dependencyInfo = requestData.getQueryParameter("dependencyInfo", true, false);

		{
			json ingestionStatusRoot = _mmsEngineDBFacade->getIngestionRootsStatus(
				apiAuthorizationDetails->workspace, ingestionRootKey, mediaItemKey, start, rows,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate, label, status, asc, dependencyInfo, ingestionJobOutputs, hiddenToo,
				// 2022-12-18: IngestionRoot dovrebbe essere stato aggiunto
				// da tempo
				false
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::ingestionRootMetaDataContent(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestionRootMetaDataContent";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t ingestionRootKey = requestData.getQueryParameter("ingestionRootKey", static_cast<int64_t>(-1), true);

		bool processedMetadata = requestData.getQueryParameter("processedMetadata", false);

		{
			string ingestionRootMetaDataContent;
			if (processedMetadata)
				ingestionRootMetaDataContent = _mmsEngineDBFacade->ingestionRoot_columnAsString(
					apiAuthorizationDetails->workspace->_workspaceKey, "processedMetaDataContent", ingestionRootKey,
					// 2022-12-18: IngestionJobKey dovrebbe essere stato
					// aggiunto da tempo
					false
				);
			else
				ingestionRootMetaDataContent = _mmsEngineDBFacade->ingestionRoot_columnAsString(
					apiAuthorizationDetails->workspace->_workspaceKey, "metaDataContent", ingestionRootKey,
					// 2022-12-18: IngestionJobKey dovrebbe essere stato
					// aggiunto da tempo
					false
				);

			/*
			string ingestionRootMetaDataContent = _mmsEngineDBFacade->getIngestionRootMetaDataContent(
				workspace, ingestionRootKey, processedMetadata,
				// 2022-12-18: IngestionJobKey dovrebbe essere stato
				// aggiunto da tempo
				false
			);
			*/

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, ingestionRootMetaDataContent);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::ingestionJobsStatus(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	SPDLOG_INFO("CCCCCCCCCCC");
	string api = "ingestionJobsStatus";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1));

		int32_t start = requestData.getQueryParameter("start", static_cast<int64_t>(0));

		int32_t rows = requestData.getQueryParameter("rows", static_cast<int64_t>(10));
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return
			// is much less

			// rows = _maxPageSize;

			string errorMessage = std::format(
				"rows parameter too big"
				", rows: {}"
				", _maxPageSize: {}",
				rows, _maxPageSize
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string label = requestData.getQueryParameter("label", "");

		bool labelLike = requestData.getQueryParameter("labelLike", true);

		string startIngestionDate = requestData.getQueryParameter("startIngestionDate", "");
		string endIngestionDate = requestData.getQueryParameter("endIngestionDate", "");

		string startScheduleDate = requestData.getQueryParameter("startScheduleDate", "");

		string ingestionType = requestData.getQueryParameter("ingestionType", "");

		bool asc = requestData.getQueryParameter("asc", true);

		bool ingestionJobOutputs = requestData.getQueryParameter("ingestionJobOutputs", true);

		bool dependencyInfo = requestData.getQueryParameter("dependencyInfo", true);

		// used in case of live-proxy
		string configurationLabel = requestData.getQueryParameter("configurationLabel", "");

		// used in case of live-grid
		string outputChannelLabel = requestData.getQueryParameter("outputChannelLabel", "");

		// used in case of live-recorder
		int64_t recordingCode = requestData.getQueryParameter("recordingCode", static_cast<int64_t>(-1));

		// used in case of broadcaster
		bool broadcastIngestionJobKeyNotNull = requestData.getQueryParameter("broadcastIngestionJobKeyNotNull", false);

		string jsonParametersCondition = requestData.getQueryParameter("jsonParametersCondition", "");

		string status = requestData.getQueryParameter("status", "all");

		bool fromMaster = requestData.getQueryParameter("fromMaster", false);

		{
			json ingestionStatusRoot = _mmsEngineDBFacade->getIngestionJobsStatus(
				apiAuthorizationDetails->workspace, ingestionJobKey, start, rows, label, labelLike,
				/* startAndEndIngestionDatePresent, */ startIngestionDate, endIngestionDate, startScheduleDate, ingestionType, configurationLabel,
				outputChannelLabel, recordingCode, broadcastIngestionJobKeyNotNull, jsonParametersCondition, asc, status, dependencyInfo,
				ingestionJobOutputs, fromMaster
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::cancelIngestionJob(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "API::cancelIngestionJob";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCancelIngestionJob)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", cancelIngestionJob: {}",
			apiAuthorizationDetails->canCancelIngestionJob
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		/*
		 * This forceCancel parameter was added because of this Scenario:
		 *	1. Live proxy ingestion job.
		 *	2. the ffmpeg command will never start, may be because of a
		 *wrong url In this scenario there is no way to cancel the job because:
		 *		1. The EncoderVideoAudioProxy thread will never exit
		 *from his loop because it is a Live Proxy. The only way to exit is
		 *through the kill of the encoding job (kill of the ffmpeg command)
		 *		2. The encoding job cannot be killed because the ffmpeg
		 *process will never start
		 *
		 *  Really I discovered later that the above scenario was already
		 *managed by the kill encoding job method. In fact, this method set the
		 *encodingStatusFailures into DB to -100. The EncoderVideoAudioProxy
		 *thread checks this number and, if it is negative, exit for his
		 *  internal look
		 *
		 *  For this reason, it would be better to avoid to use the forceCancel
		 *parameter because it is set the ingestionJob status to
		 *End_CanceledByUser but it could leave the EncoderVideoAudioProxy
		 *thread allocated and/or the ffmpeg process running.
		 *
		 * This forceCancel parameter is useful in scenarios where we have to
		 *force the status of the IngestionJob to End_CanceledByUser status. In
		 *this case it is important to check if there are active associated
		 *EncodingJob (i.e. ToBeProcessed or Processing) and set them to
		 *End_CanceledByUser.
		 *
		 * Otherwise the EncodingJob, orphan of the IngestionJob, will remain
		 *definitevely in this 'active' state creating problems to the Engine.
		 * Also, these EncodingJobs may have also the processor field set to
		 *NULL (specially in case of ToBeProcessed) and therefore they will not
		 *managed by the reset procedure called when the Engine start.
		 *
		 *
		 */
		bool forceCancel = requestData.getQueryParameter("forceCancel", false);

		MMSEngineDBFacade::IngestionStatus ingestionStatus = _mmsEngineDBFacade->ingestionJob_Status(
			apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
			// 2022-12-18: meglio avere una info sicura
			true
		);

		if (!forceCancel && ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
		{
			string errorMessage = std::format(
				"The IngestionJob cannot be removed because of his Status"
				", ingestionJobKey: {}"
				", ingestionStatus: {}",
				ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			"Update IngestionJob"
			", ingestionJobKey: {}"
			", IngestionStatus: End_CanceledByUser"
			", errorMessage: ",
			ingestionJobKey
		);
		_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_CanceledByUser, "");

		if (forceCancel)
			_mmsEngineDBFacade->forceCancelEncodingJob(ingestionJobKey);

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::updateIngestionJob(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updateIngestionJob";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		try
		{
			SPDLOG_INFO(
				"ingestionJob_IngestionTypeStatus"
				", workspace->_workspaceKey: {}"
				", ingestionJobKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey
			);

			auto [ingestionType, ingestionStatus] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatus(
				apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
				// 2022-12-18: meglio avere una informazione sicura
				true
			);

			if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
			{
				string errorMessage = std::format(
					"It is not possible to update an IngestionJob that "
					"it is not in Start_TaskQueued status"
					", ingestionJobKey: {}"
					", ingestionStatus: {}",
					ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			json metadataRoot = JSONUtils::toJson(requestData.requestBody);

			string field = "IngestionType";
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = std::format(
					"IngestionType field is missing"
					", ingestionJobKey: {}",
					ingestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sIngestionType = JSONUtils::asString(metadataRoot, "IngestionType", "");

			if (sIngestionType == MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionType::LiveRecorder))
			{
				if (ingestionType != MMSEngineDBFacade::IngestionType::LiveRecorder)
				{
					string errorMessage = std::format(
						"It was requested an Update of Live-Recorder "
						"but IngestionType is not a LiveRecorder"
						", ingestionJobKey: {}"
						", ingestionType: {}",
						ingestionJobKey, MMSEngineDBFacade::toString(ingestionType)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				{
					bool ingestionJobLabelModified = false;
					string newIngestionJobLabel;
					bool channelLabelModified = false;
					string newChannelLabel;
					bool recordingPeriodStartModified = false;
					string newRecordingPeriodStart;
					bool recordingPeriodEndModified = false;
					string newRecordingPeriodEnd;
					bool recordingVirtualVODModified = false;
					bool newRecordingVirtualVOD;

					{
						field = "IngestionJobLabel";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							ingestionJobLabelModified = true;
							newIngestionJobLabel = JSONUtils::asString(metadataRoot, "IngestionJobLabel", "");
						}

						field = "ChannelLabel";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							channelLabelModified = true;
							newChannelLabel = JSONUtils::asString(metadataRoot, "ChannelLabel", "");
						}

						field = "scheduleStart";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							recordingPeriodStartModified = true;
							newRecordingPeriodStart = JSONUtils::asString(metadataRoot, "scheduleStart", "");
						}

						field = "scheduleEnd";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							recordingPeriodEndModified = true;
							newRecordingPeriodEnd = JSONUtils::asString(metadataRoot, "scheduleEnd", "");
						}

						field = "RecordingVirtualVOD";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							recordingVirtualVODModified = true;
							newRecordingVirtualVOD = JSONUtils::asBool(metadataRoot, "RecordingVirtualVOD", false);
						}
					}

					if (recordingPeriodStartModified)
					{
						// Validator validator(_logger, _mmsEngineDBFacade,
						// _configuration);
						Datetime::parseUtcStringToUtcInSecs(newRecordingPeriodStart);
					}

					if (recordingPeriodEndModified)
					{
						// Validator validator(_logger, _mmsEngineDBFacade,
						// _configuration);
						Datetime::parseUtcStringToUtcInSecs(newRecordingPeriodEnd);
					}

					SPDLOG_INFO(
						"Update IngestionJob"
						", workspaceKey: {}"
						", ingestionJobKey: {}",
						apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey
					);

					_mmsEngineDBFacade->updateIngestionJob_LiveRecorder(
						apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey, ingestionJobLabelModified, newIngestionJobLabel, channelLabelModified,
						newChannelLabel, recordingPeriodStartModified, newRecordingPeriodStart, recordingPeriodEndModified, newRecordingPeriodEnd,
						recordingVirtualVODModified, newRecordingVirtualVOD, apiAuthorizationDetails->admin
					);

					SPDLOG_INFO(
						"IngestionJob updated"
						", workspaceKey: {}"
						", ingestionJobKey: {}",
						apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey
					);
				}
			}

			json responseRoot;
			responseRoot["status"] = string("success");

			string responseBody = JSONUtils::toString(responseRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", e.what(): {}",
				api, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::ingestionJobSwitchToEncoder(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestionJobSwitchToEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		// mandatory nel caso di broadcaster, servono:
		// 	- newPushEncoderKey e newPushPublicEncoderName per lo switch del broadcaster
		// 	- newEncodersPoolLabel per lo switch del broadcast
		// mandatory nel caso di Live-Proxy or VOD-Proxy or CountdownProxy, serve:
		// 	- newEncodersPoolLabel
		int64_t newPushEncoderKey = requestData.getQueryParameter("newPushEncoderKey", static_cast<int64_t>(-1), false);
		// newPushPublicEncoderName: indica se bisogna usare l'IP pubblico o quello interno/privato
		bool newPushPublicEncoderName = requestData.getQueryParameter("newPushPublicEncoderName", false, false);
		string newEncodersPoolLabel = requestData.getQueryParameter("newEncodersPoolLabel", string(""), false);

		SPDLOG_INFO(
			"ingestionJobSwitchToEncoder"
			", workspace->_workspaceKey: {}"
			", ingestionJobKey: {}"
			", newPushEncoderKey: {}"
			", newPushPublicEncoderName: {}"
			", newEncodersPoolLabel: {}",
			apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey, newPushEncoderKey, newPushPublicEncoderName, newEncodersPoolLabel
		);

		auto [ingestionType, ingestionStatus, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
			apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
			// 2022-12-18: meglio avere una informazione sicura
			true
		);

		if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::EncodingQueued)
		{
			string errorMessage = std::format(
				"It is not possible to switch to a new encoder when "
				"ingestionJob is not in EncodingQueued status"
				", ingestionJobKey: {}"
				", ingestionStatus: {}",
				ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// NOTA BENE: QUESTO METODO POTREBBE NON ESSERE COMPLETO

		if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy)
		{
			json broadcasterRoot = JSONUtils::asJson(metadataContentRoot["internalMMS"], "broadcaster");
			if (broadcasterRoot != nullptr)
			{
				// ingestionJobKey is referring a Broadcaster / Live Channel

				// verifica se newPushEncoderKey sia uno degli encoder gestiti dal workspace
				if (!_mmsEngineDBFacade->encoderWorkspaceMapping_isPresent(apiAuthorizationDetails->workspace->_workspaceKey, newPushEncoderKey))
				{
					string errorMessage = std::format(
						"EncoderKey is not managed by the workspaceKey"
						", ingestionJobKey: {}"
						", workspaceKey: {}"
						", newPushEncoderKey: {}",
						ingestionJobKey, apiAuthorizationDetails->workspace->_workspaceKey, newPushEncoderKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				// modifica broadcasterIngestionJob->metadataContentRoot in modo che l'engine faccia partire l'encodingJob su newPushEncoderKey
				{
					json internalMMSRoot = JSONUtils::asJson(metadataContentRoot, "internalMMS");
					json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails");
					if (encodersDetailsRoot == nullptr)
					{
						string errorMessage = std::format(
							"No encodersDetails json found"
							", ingestionJobKey: {}",
							ingestionJobKey
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					encodersDetailsRoot["pushEncoderKey"] = newPushEncoderKey;
					encodersDetailsRoot["pushPublicEncoderName"] = newPushPublicEncoderName;
					internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
					metadataContentRoot["internalMMS"] = internalMMSRoot;

					_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(metadataContentRoot));

					auto [broadcasterEncodingJobKey, broadcasterEncoderKey] =
						_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKey(ingestionJobKey, true);
					// dopo aver modificato il pushEncoderKey, killo l'encodingjob del broadcaster
					// solo per farlo ripartire in modo da usare il nuovo encoder
					try
					{
						killEncodingJob(broadcasterEncoderKey, ingestionJobKey, broadcasterEncodingJobKey, "killToRestartByEngine");
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"killEncodingJob (killToRestartByEngine) failed"
							", broadcasterEncoderKey: {}"
							", ingestionJobKey: {}"
							", broadcasterEncodingJobKey: {}",
							broadcasterEncoderKey, ingestionJobKey, broadcasterEncodingJobKey
						);
					}
				}

				int64_t broadcastIngestionJobKey = JSONUtils::asInt64(broadcasterRoot, "broadcastIngestionJobKey", -1);

				// 1. modifica broadcastIngestionJob->metadataContentRoot in modo che l'engine faccia partire l'encodingJob su newEncodersPoolLabel
				// 2. modifica broadcastEncodingJob->outputsRoot[0]->udpUrl per farlo puntare al nuovo encoder/server su cui ascolta il broadcaster
				{
					auto [broadcastIngestionType, broadcastIngestionStatus, broadcastMetadataContentRoot] =
						_mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
							apiAuthorizationDetails->workspace->_workspaceKey, broadcastIngestionJobKey,
							// 2022-12-18: meglio avere una informazione sicura
							true
						);

					if (broadcastIngestionStatus != MMSEngineDBFacade::IngestionStatus::EncodingQueued)
					{
						string errorMessage = std::format(
							"It is not possible to switch to a new encoder when "
							"ingestionJob is not in EncodingQueued status"
							", broadcastIngestionJobKey: {}"
							", broadcastIngestionStatus: {}",
							broadcastIngestionJobKey, MMSEngineDBFacade::toString(broadcastIngestionStatus)
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					json internalMMSRoot = JSONUtils::asJson(broadcastMetadataContentRoot, "internalMMS");
					json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails");
					if (encodersDetailsRoot == nullptr)
					{
						string errorMessage = std::format(
							"No encodersDetails json found"
							", ingestionJobKey: {}",
							ingestionJobKey
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					encodersDetailsRoot["encodersPoolLabel"] = newEncodersPoolLabel;
					internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
					broadcastMetadataContentRoot["internalMMS"] = internalMMSRoot;

					_mmsEngineDBFacade->updateIngestionJobMetadataContent(
						broadcastIngestionJobKey, JSONUtils::toString(broadcastMetadataContentRoot)
					);

					auto [broadcastEncodingJobKey, broadcastEncoderKey, broadcastEncodingJobParametersRoot] =
						_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKeyParameters(broadcastIngestionJobKey, true);

					// nel caso del broadcast, è necessario anche aggiornare outputsRoot[0]->udpUrl
					// per farlo puntare al nuovo server su cui ascolta il broadcaster
					{
						string broadcasterStreamConfigurationLabel = JSONUtils::asString(metadataContentRoot, "configurationLabel");

						string newOutputUdpUrl = _mmsEngineDBFacade->getStreamPushServerUrl(
							apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey, broadcasterStreamConfigurationLabel, newPushEncoderKey,
							newPushPublicEncoderName, false
						);

						json outputsRoot = broadcastEncodingJobParametersRoot["outputsRoot"];
						json outputRoot = outputsRoot[0];
						outputRoot["udpUrl"] = newOutputUdpUrl;
						outputsRoot[0] = outputRoot;
						broadcastEncodingJobParametersRoot["outputsRoot"] = outputsRoot;
						_mmsEngineDBFacade->updateEncodingJobParameters(
							broadcastEncodingJobKey, JSONUtils::toString(broadcastEncodingJobParametersRoot)
						);
					}

					// dopo aver modificato il pushEncoderKey, killo l'encodingjob del broadcaster
					// solo per farlo ripartire in modo da usare il nuovo encoder
					try
					{
						killEncodingJob(broadcastEncoderKey, broadcastIngestionJobKey, broadcastEncodingJobKey, "killToRestartByEngine");
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"killEncodingJob (killToRestartByEngine) failed"
							", broadcastEncoderKey: {}"
							", broadcastIngestionJobKey: {}"
							", broadcastEncodingJobKey: {}",
							broadcastEncoderKey, broadcastIngestionJobKey, broadcastEncodingJobKey
						);
					}
				}
			}
			else
			{
				// modifica ingestionJob->metadataContentRoot in modo che l'engine faccia partire l'encodingJob su newEncodersPoolLabel

				auto [ingestionType, ingestionStatus, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
					apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
					// 2022-12-18: meglio avere una informazione sicura
					true
				);

				if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::EncodingQueued)
				{
					string errorMessage = std::format(
						"It is not possible to switch to a new encoder when "
						"ingestionJob is not in EncodingQueued status"
						", ingestionJobKey: {}"
						", ingestionStatus: {}",
						ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				json internalMMSRoot = JSONUtils::asJson(metadataContentRoot, "internalMMS");
				json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails");
				if (encodersDetailsRoot == nullptr)
				{
					string errorMessage = std::format(
						"No encodersDetails json found"
						", ingestionJobKey: {}",
						ingestionJobKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				encodersDetailsRoot["encodersPoolLabel"] = newEncodersPoolLabel;
				internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
				metadataContentRoot["internalMMS"] = internalMMSRoot;

				_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(metadataContentRoot));

				auto [encodingJobKey, encoderKey] = _mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKey(ingestionJobKey, true);
				// dopo aver modificato il pushEncoderKey, killo l'encodingjob del broadcaster
				// solo per farlo ripartire in modo da usare il nuovo encoder
				try
				{
					killEncodingJob(encoderKey, ingestionJobKey, encodingJobKey, "killToRestartByEngine");
				}
				catch (...)
				{
					SPDLOG_ERROR(
						"killEncodingJob (killToRestartByEngine) failed"
						", encoderKey: {}"
						", ingestionJobKey: {}"
						", encodingJobKey: {}",
						encoderKey, ingestionJobKey, encodingJobKey
					);
				}
			}
		}
		else
		{
			string errorMessage = std::format(
				"ingestionJobSwitchToEncoder. switch cannot be managed"
				", ingestionJobKey: {}"
				", ingestionType: {}",
				ingestionJobKey, MMSEngineDBFacade::toString(ingestionType)
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::changeLiveProxyPlaylist(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "changeLiveProxyPlaylist";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t broadcasterIngestionJobKey = requestData.getQueryParameter("ingestionJobKey", -1, true, nullptr);
		bool interruptPlaylist = requestData.getQueryParameter("interruptPlaylist", false, false, nullptr);

		SPDLOG_INFO(
			"Received {}"
			", broadcasterIngestionJobKey: {}"
			", interruptPlaylist: {}"
			", requestData.requestBody: {}",
			api, broadcasterIngestionJobKey, interruptPlaylist, requestData.requestBody
		);

		// next try/catch initialize the belows parameters using the broadcaster info

		// check of ingestion job and retrieve some fields
		int64_t utcBroadcasterStart;
		int64_t utcBroadcasterEnd;

		int64_t broadcastIngestionJobKey;
		string broadcastDefaultMediaType; // options: Stream, Media, Countdown,
										  // Direct URL
		// used in case mediaType is Stream
		json broadcastDefaultStreamInputRoot = nullptr;
		// used in case mediaType is Media
		json broadcastDefaultVodInputRoot = nullptr;
		// used in case mediaType is Countdown
		json broadcastDefaultCountdownInputRoot = nullptr;
		// used in case mediaType is Direct URL
		json broadcastDefaultDirectURLInputRoot = nullptr;
		try
		{
			SPDLOG_INFO(
				"ingestionJob_IngestionTypeStatusMetadataContent"
				", workspace->_workspaceKey: {}"
				", broadcasterIngestionJobKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey
			);

			auto [ingestionType, ingestionStatus, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
				apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey,
				// 2022-12-18: meglio avere una informazione sicura
				true
			);

			if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy)
			{
				string errorMessage = std::format(
					"Ingestion type is not a Live/VODProxy"
					", broadcasterIngestionJobKey: {}"
					", ingestionType: {}",
					broadcasterIngestionJobKey, MMSEngineDBFacade::toString(ingestionType)
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
			string prefixIngestionStatus = "End_";
			if (sIngestionStatus.starts_with(prefixIngestionStatus))
			{
				string errorMessage = std::format(
					"Ingestion job is already finished"
					", broadcasterIngestionJobKey: {}"
					", sIngestionStatus: {}"
					", ingestionType: {}",
					broadcasterIngestionJobKey, sIngestionStatus, MMSEngineDBFacade::toString(ingestionType)
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string field = "internalMMS";
			json internalMMSRoot = JSONUtils::asJson(metadataContentRoot, field, json(), true);

			field = "broadcaster";
			json broadcasterRoot = JSONUtils::asJson(internalMMSRoot, field, json(), true);

			field = "broadcastIngestionJobKey";
			broadcastIngestionJobKey = JSONUtils::asInt64(broadcasterRoot, field, 0, true);

			field = "schedule";
			json proxyPeriodRoot = JSONUtils::asJson(metadataContentRoot, field, json(), true);

			field = "timePeriod";
			bool timePeriod = JSONUtils::asBool(metadataContentRoot, field, false, true);

			field = "start";
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcBroadcasterStart = Datetime::parseUtcStringToUtcInSecs(proxyPeriodStart);

			field = "end";
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcBroadcasterEnd = Datetime::parseUtcStringToUtcInSecs(proxyPeriodEnd);

			field = "broadcastDefaultPlaylistItem";
			if (JSONUtils::isMetadataPresent(broadcasterRoot, field))
			{
				json broadcastDefaultPlaylistItemRoot = broadcasterRoot[field];

				field = "mediaType";
				if (JSONUtils::isMetadataPresent(broadcastDefaultPlaylistItemRoot, field))
				{
					broadcastDefaultMediaType = JSONUtils::asString(broadcastDefaultPlaylistItemRoot, field, "");

					if (broadcastDefaultMediaType == "Stream")
					{
						field = "streamConfigurationLabel";
						string broadcastDefaultConfigurationLabel = JSONUtils::asString(broadcastDefaultPlaylistItemRoot, field, "");
						int maxWidth = -1;
						string userAgent;
						string otherInputOptions;

						field = "filters";
						json filtersRoot;
						if (JSONUtils::isMetadataPresent(broadcastDefaultPlaylistItemRoot, field))
							filtersRoot = broadcastDefaultPlaylistItemRoot[field];

						broadcastDefaultStreamInputRoot = _mmsEngineDBFacade->getStreamInputRoot(
							apiAuthorizationDetails->workspace, broadcasterIngestionJobKey, broadcastDefaultConfigurationLabel, "",
							"", // useVideoTrackFromPhysicalPathName,
								// useVideoTrackFromPhysicalDeliveryURL
							maxWidth, userAgent, otherInputOptions, "", filtersRoot
						);
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						vector<tuple<int64_t, string, string, string>> sources;

						MMSEngineDBFacade::ContentType vodContentType;

						field = "referencePhysicalPathKeys";
						if (JSONUtils::isMetadataPresent(broadcastDefaultPlaylistItemRoot, field))
						{
							json referencePhysicalPathKeysRoot = broadcastDefaultPlaylistItemRoot[field];

							int64_t firstMediaEncodingProfileKey = -2;
							for (int referencePhysicalPathKeyIndex = 0; referencePhysicalPathKeyIndex < referencePhysicalPathKeysRoot.size();
								 referencePhysicalPathKeyIndex++)
							{
								json referencePhysicalPathKeyRoot = referencePhysicalPathKeysRoot[referencePhysicalPathKeyIndex];

								int64_t broadcastDefaultPhysicalPathKey = JSONUtils::asInt64(referencePhysicalPathKeyRoot, "physicalPathKey", -1);
								string broadcastDefaultTitle = JSONUtils::asString(referencePhysicalPathKeyRoot, "mediaItemTitle", "");

								// controllo che tutti i media usano lo stesso encoding profile
								{
									int64_t currentEncodingProfileKey = _mmsEngineDBFacade->physicalPath_columnAsInt64(
										"encodingprofilekey", broadcastDefaultPhysicalPathKey, nullptr, false
									);
									if (firstMediaEncodingProfileKey == -2) // primo media
										firstMediaEncodingProfileKey = currentEncodingProfileKey;
									else if (firstMediaEncodingProfileKey != currentEncodingProfileKey)
									{
										string errorMessage = std::format(
											"Media are not using the same encoding profile"
											", broadcasterIngestionJobKey: {}"
											", firstMediaEncodingProfileKey: {}"
											", currentEncodingProfileKey: {}",
											broadcasterIngestionJobKey, firstMediaEncodingProfileKey, currentEncodingProfileKey
										);
										SPDLOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
								}

								string sourcePhysicalPathName;
								{
									tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = _mmsStorage->getPhysicalPathDetails(
										broadcastDefaultPhysicalPathKey,
										// 2022-12-18: MIK dovrebbe
										// essere stato aggiunto da
										// tempo
										false
									);

									bool warningIfMissing = false;
									tie(ignore, vodContentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
										_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
											apiAuthorizationDetails->workspace->_workspaceKey, broadcastDefaultPhysicalPathKey, warningIfMissing,
											// 2022-12-18: MIK dovrebbe
											// essere stato aggiunto da
											// tempo
											false
										);
								}

								// int64_t durationInMilliSeconds =
								// 	_mmsEngineDBFacade->getMediaDurationInMilliseconds(
								// 	-1, broadcastDefaultPhysicalPathKey);

								// calculate delivery URL in case of an external
								// encoder
								string sourcePhysicalDeliveryURL;
								{
									int64_t utcNow;
									{
										chrono::system_clock::time_point now = chrono::system_clock::now();
										utcNow = chrono::system_clock::to_time_t(now);
									}

									tie(sourcePhysicalDeliveryURL, ignore) = _mmsDeliveryAuthorization->createDeliveryAuthorization(
										-1, // userKey,
										apiAuthorizationDetails->workspace,
										"", // clientIPAddress,

										-1, // mediaItemKey,
										"", // uniqueName,
										-1, // encodingProfileKey,
										"", // encodingProfileLabel,

										broadcastDefaultPhysicalPathKey,

										-1, // ingestionJobKey,	(in case of live)
										-1, // deliveryCode,

										abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
										999999,							 // maxRetries,
										false,							 // reuseAuthIfPresent
										false,							 // playerIPToBeAuthorized
										"",								 // playerCountry
										"",								 // playerRegion
										false,							 // save,
										"MMS_SignedURL",				 // deliveryType,

										false, // warningIfMissingMediaItemKey,
										true,  // filteredByStatistic
										""	   // userId (it is not needed
											   // it filteredByStatistic is
											   // true
									);
								}

								sources.emplace_back(
									broadcastDefaultPhysicalPathKey, broadcastDefaultTitle, sourcePhysicalPathName, sourcePhysicalDeliveryURL
								);
							}
						}

						json filtersRoot = JSONUtils::asJson(broadcastDefaultPlaylistItemRoot, "filters", json());

						string otherInputOptions = JSONUtils::asString(broadcastDefaultPlaylistItemRoot, "otherInputOptions");

						/*
						if (JSONUtils::isMetadataPresent(broadcastDefaultPlaylistItemRoot, field))
							filtersRoot = broadcastDefaultPlaylistItemRoot[field];
						*/

						// the same json structure is used in
						// MMSEngineProcessor::manageVODProxy
						broadcastDefaultVodInputRoot = _mmsEngineDBFacade->getVodInputRoot(vodContentType, sources, filtersRoot, otherInputOptions);
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						field = "physicalPathKey";
						int64_t broadcastDefaultPhysicalPathKey = JSONUtils::asInt64(broadcastDefaultPlaylistItemRoot, field, -1);
						field = "text";
						string broadcastDefaultText = JSONUtils::asString(broadcastDefaultPlaylistItemRoot, field, "");
						field = "textPosition_X_InPixel";
						string broadcastDefaultTextPosition_X_InPixel = JSONUtils::asString(broadcastDefaultPlaylistItemRoot, field, "");
						field = "textPosition_Y_InPixel";
						string broadcastDefaultTextPosition_Y_InPixel = JSONUtils::asString(broadcastDefaultPlaylistItemRoot, field, "");

						MMSEngineDBFacade::ContentType vodContentType;
						string sourcePhysicalPathName;
						string sourcePhysicalDeliveryURL;
						int64_t videoDurationInMilliSeconds;
						{
							tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
								broadcastDefaultPhysicalPathKey,
								// 2022-12-18: MIK dovrebbe essere stato
								// aggiunto da tempo
								false
							);
							tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

							int64_t sourceMediaItemKey = -1;
							videoDurationInMilliSeconds = _mmsEngineDBFacade->getMediaDurationInMilliseconds(
								sourceMediaItemKey, broadcastDefaultPhysicalPathKey,
								// 2022-12-18: MIK dovrebbe essere stato
								// aggiunto da tempo
								false
							);

							// calculate delivery URL in case of an external
							// encoder
							{
								int64_t utcNow;
								{
									chrono::system_clock::time_point now = chrono::system_clock::now();
									utcNow = chrono::system_clock::to_time_t(now);
								}

								pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
									-1, // userKey,
									apiAuthorizationDetails->workspace,
									"", // clientIPAddress,

									-1, // mediaItemKey,
									"", // uniqueName,
									-1, // encodingProfileKey,
									"", // encodingProfileLabel,

									broadcastDefaultPhysicalPathKey,

									-1, // ingestionJobKey,	(in case of live)
									-1, // deliveryCode,

									abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
									999999,							 // maxRetries,
									false,							 // reuseAuthIfPresent
									false,							 // playerIPToBeAuthorized
									"",								 // playerCountry
									"",								 // playerRegion
									false,							 // save,
									"MMS_SignedURL",				 // deliveryType,

									false, // warningIfMissingMediaItemKey,
									true,  // filteredByStatistic
									""	   // userId (it is not needed it
										   // filteredByStatistic is true
								);

								tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
							}
						}

						// inizializza filtersRoot e verifica se drawtext is present
						bool isDrawTextFilterPresent = false;
						field = "filters";
						json filtersRoot;
						if (JSONUtils::isMetadataPresent(broadcastDefaultPlaylistItemRoot, field))
						{
							filtersRoot = broadcastDefaultPlaylistItemRoot["filters"];
							field = "video";
							if (JSONUtils::isMetadataPresent(filtersRoot, field))
							{
								json videoFiltersRoot = broadcastDefaultPlaylistItemRoot["video"];
								for (int videoFilterIndex = 0; videoFilterIndex < videoFiltersRoot.size(); videoFilterIndex++)
								{
									json videoFilterRoot = videoFiltersRoot[videoFilterIndex];
									field = "type";
									if (JSONUtils::isMetadataPresent(videoFilterRoot, field) && videoFilterRoot[field] == "drawtext")
										isDrawTextFilterPresent = true;
								}
							}
						}
						if (!isDrawTextFilterPresent)
						{
							string errorMessage = std::format(
								"Countdown has to have the drawText filter"
								", broadcasterIngestionJobKey: {}"
								", broadcastDefaultPlaylistItemRoot: {}",
								broadcasterIngestionJobKey, JSONUtils::toString(broadcastDefaultPlaylistItemRoot)
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						// the same json structure is used in
						// MMSEngineProcessor::manageVODProxy
						broadcastDefaultCountdownInputRoot = _mmsEngineDBFacade->getCountdownInputRoot(
							sourcePhysicalPathName, sourcePhysicalDeliveryURL, broadcastDefaultPhysicalPathKey, videoDurationInMilliSeconds,
							filtersRoot
						);
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						field = "url";
						string broadcastDefaultURL = JSONUtils::asString(broadcastDefaultPlaylistItemRoot, field, "");

						field = "filters";
						json filtersRoot = JSONUtils::asJson(broadcastDefaultPlaylistItemRoot, field, json());

						broadcastDefaultDirectURLInputRoot = _mmsEngineDBFacade->getDirectURLInputRoot(broadcastDefaultURL, filtersRoot);
					}
					else
					{
						string errorMessage = std::format(
							"Broadcaster data: unknown MediaType"
							", broadcasterIngestionJobKey: {}"
							", broadcastDefaultMediaType: {}",
							broadcasterIngestionJobKey, broadcastDefaultMediaType
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				else
				{
					string errorMessage = std::format(
						"Broadcaster data: no mediaType is present"
						", broadcasterIngestionJobKey: {}",
						broadcasterIngestionJobKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = std::format(
					"Broadcaster data: no broadcastDefaultPlaylistItem is present"
					", broadcasterIngestionJobKey: {}",
					broadcasterIngestionJobKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", broadcasterIngestionJobKey: {}"
				", e.what(): {}",
				api, broadcasterIngestionJobKey, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// check/build the new playlist
		json newPlaylistRoot = json::array();
		try
		{
			json newReceivedPlaylistRoot = JSONUtils::toJson(requestData.requestBody);

			// check the received playlist
			// in case of vodInput/countdownInput, the physicalPathKey is
			// received but we need to set vodContentType and
			// sourcePhysicalPathName
			{
				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < newReceivedPlaylistRoot.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = newReceivedPlaylistRoot[newReceivedPlaylistIndex];

					// aggiungo sUtcScheduleStart/End in modo da capire le date per chi vede la playlist (info di debug)
					{
						string sUtcScheduleStart;
						string sUtcScheduleEnd;
						if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "timePeriod") && newReceivedPlaylistItemRoot["timePeriod"])
						{
							if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "utcScheduleStart"))
							{
								sUtcScheduleStart = Datetime::utcToUtcString(newReceivedPlaylistItemRoot["utcScheduleStart"]);
								newReceivedPlaylistItemRoot["sUtcScheduleStart"] = sUtcScheduleStart;
							}
							if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "utcScheduleEnd"))
							{
								sUtcScheduleEnd = Datetime::utcToUtcString(newReceivedPlaylistItemRoot["utcScheduleEnd"]);
								newReceivedPlaylistItemRoot["sUtcScheduleEnd"] = sUtcScheduleEnd;
							}
						}
						SPDLOG_INFO(
							"Processing newReceivedPlaylistRoot (the received one)"
							", broadcasterIngestionJobKey: {}"
							", newReceivedPlaylistRoot: {}/{}"
							", sUtcScheduleStart: {}"
							", sUtcScheduleEnd: {}",
							broadcasterIngestionJobKey, newReceivedPlaylistIndex, newReceivedPlaylistRoot.size(), sUtcScheduleStart, sUtcScheduleEnd
						);
					}
					{
						if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "streamInput"))
						{
							json streamInputRoot = newReceivedPlaylistItemRoot["streamInput"];

							streamInputRoot["filters"] = getReviewedFiltersRoot(streamInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							newReceivedPlaylistItemRoot["streamInput"] = streamInputRoot;
						}
						else if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "vodInput"))
						{
							json vodInputRoot = newReceivedPlaylistItemRoot["vodInput"];

							vodInputRoot["filters"] = getReviewedFiltersRoot(vodInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							// field = "sources";
							json sourcesRoot = JSONUtils::asJson(vodInputRoot, "sources", json(), true);

							if (sourcesRoot.size() == 0)
							{
								string errorMessage = std::format(
									"No source is present"
									", broadcasterIngestionJobKey: {}"
									", json data: {}",
									broadcasterIngestionJobKey, requestData.requestBody
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							MMSEngineDBFacade::ContentType vodContentType;

							// viene creato uno nuovo perchè in caso di errori (es physicalPath che non esiste) qualche item di sourcesRoot potrebbe
							// essere eliminato
							json newSourcesRoot = json::array();

							int64_t firstMediaEncodingProfileKey = -2;
							for (int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
							{
								json sourceRoot = sourcesRoot[sourceIndex];

								// field = "physicalPathKey";
								if (!JSONUtils::isMetadataPresent(sourceRoot, "physicalPathKey"))
								{
									string errorMessage = std::format("physicalPathKey is missing, json data: {}", requestData.requestBody);
									SPDLOG_ERROR(errorMessage);

									throw runtime_error(errorMessage);
								}
								int64_t physicalPathKey = JSONUtils::asInt64(sourceRoot, "physicalPathKey", -1);

								// controllo che tutti i media usano lo stesso encoding profile
								try
								{
									int64_t currentEncodingProfileKey =
										_mmsEngineDBFacade->physicalPath_columnAsInt64("encodingprofilekey", physicalPathKey, nullptr, false);
									if (firstMediaEncodingProfileKey == -2) // primo media
										firstMediaEncodingProfileKey = currentEncodingProfileKey;
									else if (firstMediaEncodingProfileKey != currentEncodingProfileKey)
									{
										SPDLOG_ERROR(
											"PhysicalPath not using the same encoding profile, just skip it"
											", broadcasterIngestionJobKey: {}"
											", physicalPathKey: {}"
											", firstMediaEncodingProfileKey: {}"
											", currentEncodingProfileKey: {}",
											broadcasterIngestionJobKey, physicalPathKey, firstMediaEncodingProfileKey, currentEncodingProfileKey
										);
										continue;
									}
								}
								catch (DBRecordNotFound &e)
								{
									SPDLOG_ERROR(
										"PhysicalPath not found, just skip it"
										", broadcasterIngestionJobKey: {}"
										", physicalPathKey: {}"
										", e.what(): {}",
										api, broadcasterIngestionJobKey, physicalPathKey, e.what()
									);
									continue;
								}

								string sourcePhysicalPathName;
								{
									tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
										physicalPathKey,
										// 2022-12-18: MIK dovrebbe
										// essere stato aggiunto da
										// tempo
										false
									);
									tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

									bool warningIfMissing = false;
									tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
										mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
											apiAuthorizationDetails->workspace->_workspaceKey, physicalPathKey, warningIfMissing,
											// 2022-12-18: MIK dovrebbe
											// essere stato aggiunto da
											// tempo
											false
										);

									tie(ignore, vodContentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemKeyDetails;
								}
								// field = "sourcePhysicalPathName";
								sourceRoot["sourcePhysicalPathName"] = sourcePhysicalPathName;

								// calculate delivery URL in case of an external
								// encoder
								string sourcePhysicalDeliveryURL;
								{
									int64_t utcNow;
									{
										chrono::system_clock::time_point now = chrono::system_clock::now();
										utcNow = chrono::system_clock::to_time_t(now);
									}

									pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
										-1, // userKey,
										apiAuthorizationDetails->workspace,
										"", // clientIPAddress,

										-1, // mediaItemKey,
										"", // uniqueName,
										-1, // encodingProfileKey,
										"", // encodingProfileLabel,

										physicalPathKey,

										-1, // ingestionJobKey,
											// (in case of live)
										-1, // deliveryCode,

										abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
										999999,							 // maxRetries,
										false,							 // reuseAuthIfPresent
										false,							 // playerIPToBeAuthorized
										"",								 // playerCountry
										"",								 // playerRegion
										false,							 // save,
										"MMS_SignedURL",				 // deliveryType,

										false, // warningIfMissingMediaItemKey,
										true,  // filteredByStatistic
										""	   // userId (it is not
											   // needed it
											   // filteredByStatistic is
											   // true
									);

									tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
								}
								// field = "sourcePhysicalDeliveryURL";
								sourceRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;

								// sourcesRoot[sourceIndex] = sourceRoot;
								newSourcesRoot.push_back(sourceRoot);
							}

							// vodInputRoot["sources"] = sourcesRoot;
							vodInputRoot["sources"] = newSourcesRoot;

							// field = "vodContentType";
							vodInputRoot["vodContentType"] = MMSEngineDBFacade::toString(vodContentType);

							// field = "vodInput";
							newReceivedPlaylistItemRoot["vodInput"] = vodInputRoot;
						}
						else if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "countdownInput"))
						{
							json countdownInputRoot = newReceivedPlaylistItemRoot["countdownInput"];

							countdownInputRoot["filters"] = getReviewedFiltersRoot(countdownInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							// field = "physicalPathKey";
							if (!JSONUtils::isMetadataPresent(countdownInputRoot, "physicalPathKey"))
							{
								string errorMessage = std::format("physicalPathKey is missing, json data: {}", requestData.requestBody);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
							int64_t physicalPathKey = JSONUtils::asInt64(countdownInputRoot, "physicalPathKey", -1);

							MMSEngineDBFacade::ContentType vodContentType;
							string sourcePhysicalPathName;
							int64_t videoDurationInMilliSeconds;
							{
								tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
									physicalPathKey,
									// 2022-12-18: MIK dovrebbe essere
									// stato aggiunto da tempo
									false
								);
								tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

								bool warningIfMissing = false;
								tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
									mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
										apiAuthorizationDetails->workspace->_workspaceKey, physicalPathKey, warningIfMissing,
										// 2022-12-18: MIK dovrebbe
										// essere stato aggiunto da
										// tempo
										false
									);
								tie(ignore, vodContentType, ignore, ignore, ignore, ignore, ignore, ignore, videoDurationInMilliSeconds) =
									mediaItemKeyDetails;

								// videoDurationInMilliSeconds =
								// _mmsEngineDBFacade->getMediaDurationInMilliseconds(
								// 	-1, physicalPathKey);
							}

							// field = "mmsSourceVideoAssetPathName";
							countdownInputRoot["mmsSourceVideoAssetPathName"] = sourcePhysicalPathName;

							// field = "videoDurationInMilliSeconds";
							countdownInputRoot["videoDurationInMilliSeconds"] = videoDurationInMilliSeconds;

							// field = "vodContentType";
							countdownInputRoot["vodContentType"] = MMSEngineDBFacade::toString(vodContentType);

							// field = "countdownInput";
							newReceivedPlaylistItemRoot["countdownInput"] = countdownInputRoot;
						}
						else if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "directURLInput"))
						{
							json directURLInputRoot = newReceivedPlaylistItemRoot["directURLInput"];

							directURLInputRoot["filters"] = getReviewedFiltersRoot(directURLInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							newReceivedPlaylistItemRoot["directURLInput"] = directURLInputRoot;
						}

						newReceivedPlaylistRoot[newReceivedPlaylistIndex] = newReceivedPlaylistItemRoot;
					}
				}
			}

			// 2023-02-26: Probabilmente l'array di Json ricevuto è già
			// ordinato,
			//		per sicurezza ordiniamo in base al campo start
			//		Per utilizzare 'sort' inizializziamo un vector
			vector<json> vNewReceivedPlaylist;
			{
				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < newReceivedPlaylistRoot.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = newReceivedPlaylistRoot[newReceivedPlaylistIndex];
					vNewReceivedPlaylist.push_back(newReceivedPlaylistItemRoot);
				}

				sort(
					vNewReceivedPlaylist.begin(), vNewReceivedPlaylist.end(),
					[](json aRoot, json bRoot)
					{
						int64_t aUtcProxyPeriodStart = JSONUtils::asInt64(aRoot, "utcScheduleStart", -1);
						int64_t bUtcProxyPeriodStart = JSONUtils::asInt64(bRoot, "utcScheduleStart", -1);

						return aUtcProxyPeriodStart < bUtcProxyPeriodStart;
					}
				);

				SPDLOG_INFO(
					"Sort playlist items"
					", broadcasterIngestionJobKey: {}"
					", vNewReceivedPlaylist.size: {}",
					broadcasterIngestionJobKey, vNewReceivedPlaylist.size()
				);
			}
			// 2023-02-26: ora che il vettore è ordinato, elimino gli elementi
			// precedenti a 'now - X days' (just a retention)
			{
				int32_t playlistItemsRetentionInHours = 3 * 24;
				if (apiAuthorizationDetails->workspace->_preferences != nullptr
					&& apiAuthorizationDetails->workspace->_preferences.contains("api") && apiAuthorizationDetails->workspace->_preferences["api"].is_object())
				{
					const json &apiRoot = apiAuthorizationDetails->workspace->_preferences["api"];
					if (apiRoot.contains("liveProxy") && apiRoot["liveProxy"].is_object())
					{
						const json &liveProxyRoot = apiRoot["liveProxy"];
						if (liveProxyRoot.contains("playlist") && liveProxyRoot["playlist"].is_object())
						{
							const json &playlistRoot = liveProxyRoot["playlist"];
							playlistItemsRetentionInHours = JSONUtils::asInt(playlistRoot, "retentionInHours", playlistItemsRetentionInHours);
						}
					}
				}
				chrono::system_clock::time_point now = chrono::system_clock::now();
				chrono::system_clock::time_point retention = now - chrono::hours(playlistItemsRetentionInHours);

				time_t utcRetention = chrono::system_clock::to_time_t(retention);

				int currentPlaylistIndex = -1;
				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < vNewReceivedPlaylist.size(); newReceivedPlaylistIndex++)
				{
					const json& newReceivedPlaylistItemRoot = vNewReceivedPlaylist[newReceivedPlaylistIndex];

					int64_t utcProxyPeriodStart = JSONUtils::asInt64(newReceivedPlaylistItemRoot, "utcScheduleStart", -1);
					// int64_t utcProxyPeriodEnd = JSONUtils::asInt64(newReceivedPlaylistItemRoot, "utcScheduleEnd", -1);

					if (newReceivedPlaylistIndex != 0 && utcProxyPeriodStart >= utcRetention)
					{
						currentPlaylistIndex = newReceivedPlaylistIndex - 1;

						break;
					}
					/*
					if (utcProxyPeriodStart <= utcRetention && utcRetention < utcProxyPeriodEnd)
					{
						currentPlaylistIndex = newReceivedPlaylistIndex;

						break;
					}
					*/
				}
				int leavePastEntriesNumber = 1;
				if (currentPlaylistIndex - leavePastEntriesNumber > 0)
				{
					SPDLOG_INFO(
						"Erase playlist items in the past: {} items"
						", broadcasterIngestionJobKey: {}"
						", playlistItemsRetentionInHours: {}"
						", currentPlaylistIndex: {}"
						", leavePastEntriesNumber: {}"
						", vNewReceivedPlaylist.size: {}",
						currentPlaylistIndex - leavePastEntriesNumber, broadcasterIngestionJobKey, playlistItemsRetentionInHours,
						currentPlaylistIndex, leavePastEntriesNumber, vNewReceivedPlaylist.size()
					);

					vNewReceivedPlaylist.erase(
						vNewReceivedPlaylist.begin(), vNewReceivedPlaylist.begin() + (currentPlaylistIndex - leavePastEntriesNumber)
					);
				}
				else
				{
					SPDLOG_INFO(
						"Erase playlist items in the past: nothing"
						", broadcasterIngestionJobKey: {}"
						", playlistItemsRetentionInHours: {}"
						", currentPlaylistIndex: {}"
						", leavePastEntriesNumber: {}"
						", vNewReceivedPlaylist.size: {}",
						broadcasterIngestionJobKey, playlistItemsRetentionInHours, currentPlaylistIndex, leavePastEntriesNumber,
						vNewReceivedPlaylist.size()
					);
				}
			}

			// build the new playlist
			// add the default media in case of hole filling newPlaylistRoot
			// genero un errore in caso di sovrapposizioni tra gli items della playlist
			{
				int64_t utcCurrentBroadcasterStart = utcBroadcasterStart;

				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < vNewReceivedPlaylist.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = vNewReceivedPlaylist[newReceivedPlaylistIndex];

					// correct values have to be:
					//	utcCurrentBroadcasterStart <= utcProxyPeriodStart < utcProxyPeriodEnd
					// the last utcProxyPeriodEnd has to be equal to utcBroadcasterEnd
					string field = "utcScheduleStart";
					int64_t utcProxyPeriodStart = JSONUtils::asInt64(newReceivedPlaylistItemRoot, field, -1);
					field = "utcScheduleEnd";
					int64_t utcProxyPeriodEnd = JSONUtils::asInt64(newReceivedPlaylistItemRoot, field, -1);

					SPDLOG_INFO(
						"Processing newReceivedPlaylistRoot"
						", broadcasterIngestionJobKey: {}"
						", newReceivedPlaylistRoot: {}/{}"
						", utcCurrentBroadcasterStart: {} ({})"
						", utcProxyPeriodStart: {} ({})"
						", utcProxyPeriodEnd: {} ({})",
						broadcasterIngestionJobKey, newReceivedPlaylistIndex, newReceivedPlaylistRoot.size(), utcCurrentBroadcasterStart,
						Datetime::utcToUtcString(utcCurrentBroadcasterStart), utcProxyPeriodStart, Datetime::utcToUtcString(utcProxyPeriodStart),
						utcProxyPeriodEnd, Datetime::utcToUtcString(utcProxyPeriodEnd)
					);

					if (utcCurrentBroadcasterStart > utcProxyPeriodStart || utcProxyPeriodStart >= utcProxyPeriodEnd ||
						utcProxyPeriodEnd > utcBroadcasterEnd)
					{
						string partialMessage;

						if (utcCurrentBroadcasterStart > utcProxyPeriodStart)
							partialMessage = std::format(
								"utcCurrentBroadcasterStart {} ({}) > utcProxyPeriodStart {} ({})", utcCurrentBroadcasterStart,
								Datetime::utcToUtcString(utcCurrentBroadcasterStart), utcProxyPeriodStart,
								Datetime::utcToUtcString(utcProxyPeriodStart)
							);
						else if (utcProxyPeriodStart >= utcProxyPeriodEnd)
							partialMessage = std::format(
								"utcProxyPeriodStart {} ({}) >= utcProxyPeriodEnd {} ({})", utcProxyPeriodStart,
								Datetime::utcToUtcString(utcProxyPeriodStart), utcProxyPeriodEnd, Datetime::utcToUtcString(utcProxyPeriodEnd)
							);
						else if (utcProxyPeriodEnd > utcBroadcasterEnd)
							partialMessage = std::format(
								"utcProxyPeriodEnd {} ({}) > utcBroadcasterEnd {} ({})", utcProxyPeriodEnd,
								Datetime::utcToUtcString(utcProxyPeriodEnd), utcBroadcasterEnd, Datetime::utcToUtcString(utcBroadcasterEnd)
							);

						string errorMessage = std::format(
							"Wrong dates ({})"
							", newReceivedPlaylistIndex: {}",
							partialMessage, newReceivedPlaylistIndex
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					if (utcProxyPeriodStart == utcCurrentBroadcasterStart)
						newPlaylistRoot.push_back(newReceivedPlaylistItemRoot);
					else // if (utcCurrentBroadcasterStart <
						 // utcProxyPeriodStart)
					{
						json newdPlaylistItemToBeAddedRoot;

						field = "defaultBroadcast";
						newdPlaylistItemToBeAddedRoot[field] = true;

						field = "timePeriod";
						newdPlaylistItemToBeAddedRoot[field] = true;

						field = "utcScheduleStart";
						newdPlaylistItemToBeAddedRoot[field] = utcCurrentBroadcasterStart;

						field = "utcScheduleEnd";
						newdPlaylistItemToBeAddedRoot[field] = utcProxyPeriodStart;

						if (broadcastDefaultMediaType == "Stream")
						{
							if (broadcastDefaultStreamInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["streamInput"] = broadcastDefaultStreamInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default Stream present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Media")
						{
							if (broadcastDefaultVodInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default Media present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Countdown")
						{
							if (broadcastDefaultCountdownInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default Countdown present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Direct URL")
						{
							if (broadcastDefaultDirectURLInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default DirectURL present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: unknown MediaType"
								", broadcasterIngestionJobKey: {}"
								", broadcastDefaultMediaType: {}",
								broadcasterIngestionJobKey, broadcastDefaultMediaType
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						newPlaylistRoot.push_back(newdPlaylistItemToBeAddedRoot);

						newPlaylistRoot.push_back(newReceivedPlaylistItemRoot);
					}
					utcCurrentBroadcasterStart = utcProxyPeriodEnd;
				}

				if (vNewReceivedPlaylist.size() == 0)
				{
					// no items inside the playlist

					json newdPlaylistItemToBeAddedRoot;

					string field = "defaultBroadcast";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "timePeriod";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "utcScheduleStart";
					newdPlaylistItemToBeAddedRoot[field] = utcBroadcasterStart;

					field = "utcScheduleEnd";
					newdPlaylistItemToBeAddedRoot[field] = utcBroadcasterEnd;

					if (broadcastDefaultMediaType == "Stream")
					{
						if (broadcastDefaultStreamInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["streamInput"] = broadcastDefaultStreamInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Stream present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						if (broadcastDefaultVodInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Media present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						if (broadcastDefaultCountdownInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Countdown present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						if (broadcastDefaultDirectURLInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Direct URL present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						string errorMessage = std::format(
							"Broadcaster data: unknown MediaType"
							", broadcasterIngestionJobKey: {}"
							", broadcastDefaultMediaType: {}",
							broadcasterIngestionJobKey, broadcastDefaultMediaType
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					newPlaylistRoot.push_back(newdPlaylistItemToBeAddedRoot);
				}
				else if (utcCurrentBroadcasterStart < utcBroadcasterEnd)
				{
					// last period has to be added

					json newdPlaylistItemToBeAddedRoot;

					string field = "defaultBroadcast";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "timePeriod";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "utcScheduleStart";
					newdPlaylistItemToBeAddedRoot[field] = utcCurrentBroadcasterStart;

					field = "utcScheduleEnd";
					newdPlaylistItemToBeAddedRoot[field] = utcBroadcasterEnd;

					if (broadcastDefaultMediaType == "Stream")
					{
						if (broadcastDefaultStreamInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["streamInput"] = broadcastDefaultStreamInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Stream present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						if (broadcastDefaultVodInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Media present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						if (broadcastDefaultCountdownInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Countdown present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						if (broadcastDefaultDirectURLInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Direct URL present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						string errorMessage = std::format(
							"Broadcaster data: unknown MediaType"
							", broadcasterIngestionJobKey: {}"
							", broadcastDefaultMediaType: {}",
							broadcasterIngestionJobKey, broadcastDefaultMediaType
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					newPlaylistRoot.push_back(newdPlaylistItemToBeAddedRoot);
				}
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", broadcasterIngestionJobKey: {}"
				", e.what(): {}",
				api, broadcasterIngestionJobKey, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2021-12-22: For sure we will have the BroadcastIngestionJob.
		//		We may not have the EncodingJob in case it has to be
		// executed in the future 		In this case we will save the playlist into the
		// broadcast ingestion job
		string ffmpegEncoderURL;
		ostringstream response;
		try
		{
			SPDLOG_INFO(
				"ingestionJob_IngestionTypeMetadataContent"
				", workspace->_workspaceKey: {}"
				", broadcasterIngestionJobKey: {}"
				", broadcastIngestionJobKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey, broadcastIngestionJobKey
			);

			auto [ingestionType, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeMetadataContent(
				apiAuthorizationDetails->workspace->_workspaceKey, broadcastIngestionJobKey,
				// 2022-12-18: meglio avere una informazione sicura
				true
			);

			if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy && ingestionType != MMSEngineDBFacade::IngestionType::VODProxy &&
				ingestionType != MMSEngineDBFacade::IngestionType::Countdown)
			{
				string errorMessage = std::format(
					"Ingestion type is not a LiveProxy-VODProxy-Countdown"
					", broadcasterIngestionJobKey: {}"
					", broadcastIngestionJobKey: {}"
					", ingestionType: {}",
					broadcasterIngestionJobKey, broadcastIngestionJobKey, MMSEngineDBFacade::toString(ingestionType)
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string newPlaylist = JSONUtils::toString(newPlaylistRoot);

			json broadcastParametersRoot;
			int64_t broadcastEncodingJobKey = -1;
			int64_t broadcastEncoderKey = -1;
			try
			{
				tie(broadcastEncodingJobKey, broadcastEncoderKey, broadcastParametersRoot) =
					_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKeyParameters(
						broadcastIngestionJobKey,
						// 2022-12-18: l'IngestionJob potrebbe essere stato
						// appena aggiunto
						true
					);
			}
			catch (exception &e)
			{
				SPDLOG_WARN(e.what());

				// throw;
			}

			// we may have the scenario where the encodingJob is not present
			// because will be executed in the future
			if (broadcastEncodingJobKey != -1)
			{
				// update of the parameters
				string field = "inputsRoot";
				broadcastParametersRoot[field] = newPlaylistRoot;

				string newBroadcastParameters = JSONUtils::toString(broadcastParametersRoot);

				_mmsEngineDBFacade->updateEncodingJobParameters(broadcastEncodingJobKey, newBroadcastParameters);

				// we may have the scenario where the encodingJob is present but
				// it is not running (timing to be run in the future). In this
				// case broadcastEncoderKey will be -1
				if (broadcastEncoderKey > 0)
				{
					pair<string, bool> encoderDetails = _mmsEngineDBFacade->getEncoderURL(broadcastEncoderKey);
					string transcoderHost;
					tie(transcoderHost, ignore) = encoderDetails;

					ffmpegEncoderURL = transcoderHost + _ffmpegEncoderChangeLiveProxyPlaylistURI + "/" + to_string(broadcastEncodingJobKey) +
									   "?interruptPlaylist=" + to_string(interruptPlaylist);

					vector<string> otherHeaders;
					json encoderResponse = CurlWrapper::httpPutStringAndGetJson(
						ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword),
						newPlaylist,
						"application/json", // contentType
						otherHeaders, std::format(", ingestionJobKey: {}", broadcasterIngestionJobKey)
					);
				}
			}
			else
			{
				SPDLOG_INFO(
					"The Broadcast EncodingJob was not found, the IngestionJob is updated"
					", broadcasterIngestionJobKey: {}"
					", broadcastIngestionJobKey: {}"
					", broadcastEncodingJobKey: {}",
					broadcasterIngestionJobKey, broadcastIngestionJobKey, broadcastEncodingJobKey
				);

				// update of the parameters
				json mmsInternalRoot;
				json broadcasterRoot;

				string field = "internalMMS";
				if (JSONUtils::isMetadataPresent(metadataContentRoot, field))
					mmsInternalRoot = metadataContentRoot[field];

				field = "broadcaster";
				if (JSONUtils::isMetadataPresent(mmsInternalRoot, field))
					broadcasterRoot = mmsInternalRoot[field];

				field = "broadcasterInputsRoot";
				broadcasterRoot[field] = newPlaylistRoot;

				field = "broadcaster";
				mmsInternalRoot[field] = broadcasterRoot;

				field = "internalMMS";
				metadataContentRoot[field] = mmsInternalRoot;

				string newMetadataContentRoot = JSONUtils::toString(metadataContentRoot);

				_mmsEngineDBFacade->updateIngestionJobMetadataContent(broadcastIngestionJobKey, newMetadataContentRoot);
			}

			string responseBody;
			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", broadcasterIngestionJobKey: {}"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				api, broadcasterIngestionJobKey, ffmpegEncoderURL, response.str(), e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"{} failed"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::changeLiveProxyOverlayText(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "changeLiveProxyOverlayText";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t broadcasterIngestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		SPDLOG_INFO("{}, broadcasterIngestionJobKey: {}", api, broadcasterIngestionJobKey);

		try
		{
			{
				SPDLOG_INFO(
					"ingestionJobQuery"
					", workspace->_workspaceKey: {}"
					", broadcasterIngestionJobKey: {}",
					apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey
				);

				auto [ingestionType, ingestionStatus] =
					_mmsEngineDBFacade->ingestionJob_IngestionTypeStatus(apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey, false);

				if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy)
				{
					string errorMessage = std::format(
						"Ingestion type is not a Live/VODProxy"
						", broadcasterIngestionJobKey: {}"
						", ingestionType: {}",
						broadcasterIngestionJobKey, MMSEngineDBFacade::toString(ingestionType)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
				string prefixIngestionStatus = "End_";
				if (sIngestionStatus.starts_with("End_"))
				{
					string errorMessage = std::format(
						"Ingestion job is already finished"
						", broadcasterIngestionJobKey: {}"
						", sIngestionStatus: {}"
						", ingestionType: {}",
						broadcasterIngestionJobKey, sIngestionStatus, MMSEngineDBFacade::toString(ingestionType)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			int64_t broadcasterEncodingJobKey;
			int64_t broadcasterEncoderKey;
			{
				SPDLOG_INFO(
					"encodingJobQuery"
					", broadcasterIngestionJobKey: {}",
					broadcasterIngestionJobKey
				);

				tie(broadcasterEncodingJobKey, broadcasterEncoderKey) =
					_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKey(broadcasterIngestionJobKey, false);

				if (broadcasterEncodingJobKey == -1 || broadcasterEncoderKey == -1)
				{
					string errorMessage = std::format(
						"encodingJobKey and/or encoderKey not found"
						", broadcasterEncodingJobKey: {}",
						", broadcasterEncoderKey: {}", ", broadcasterIngestionJobKey: {}", broadcasterEncodingJobKey, broadcasterEncoderKey,
						broadcasterIngestionJobKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			{
				string encoderURL;
				tie(encoderURL, ignore) = _mmsEngineDBFacade->getEncoderURL(broadcasterEncoderKey);

				string ffmpegEncoderURL = std::format("{}{}/{}", encoderURL, _ffmpegEncoderChangeLiveProxyOverlayTextURI, broadcasterEncodingJobKey);

				vector<string> otherHeaders;
				CurlWrapper::httpPutStringAndGetJson(
					ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword),
					string(requestData.requestBody),
					"text/plain", // contentType
					otherHeaders, std::format(", ingestionJobKey: {}", broadcasterIngestionJobKey)
				);
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

// LO STESSO METODO E' IN MMSEngineProcessor.cpp
json API::getReviewedFiltersRoot(json filtersRoot, const shared_ptr<Workspace>& workspace, int64_t ingestionJobKey)
{
	if (filtersRoot == nullptr)
		return filtersRoot;

	/*
	SPDLOG_INFO(
		"getReviewedFiltersRoot in"
		", filters: {}",
		JSONUtils::toString(filtersRoot)
	);
	*/

	// se viene usato il filtro imageoverlay, è necessario recuperare sourcePhysicalPathName e sourcePhysicalDeliveryURL
	if (JSONUtils::isMetadataPresent(filtersRoot, "complex"))
	{
		json complexFiltersRoot = filtersRoot["complex"];
		for (int complexFilterIndex = 0; complexFilterIndex < complexFiltersRoot.size(); complexFilterIndex++)
		{
			json complexFilterRoot = complexFiltersRoot[complexFilterIndex];
			if (JSONUtils::isMetadataPresent(complexFilterRoot, "type") && complexFilterRoot["type"] == "imageoverlay")
			{
				if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalPathKey"))
				{
					string errorMessage = std::format(
						"imageoverlay filter without imagePhysicalPathKey"
						", ingestionJobKey: {}"
						", imageoverlay filter: {}",
						ingestionJobKey, JSONUtils::toString(complexFilterRoot)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				string sourcePhysicalPathName;
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails =
						_mmsStorage->getPhysicalPathDetails(complexFilterRoot["imagePhysicalPathKey"], false);
					tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}

				// calculate delivery URL in case of an external encoder
				string sourcePhysicalDeliveryURL;
				{
					int64_t utcNow;
					{
						chrono::system_clock::time_point now = chrono::system_clock::now();
						utcNow = chrono::system_clock::to_time_t(now);
					}

					pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
						-1, // userKey,
						workspace,
						"", // clientIPAddress,

						-1, // mediaItemKey,
						"", // uniqueName,
						-1, // encodingProfileKey,
						"", // encodingProfileLabel,

						complexFilterRoot["imagePhysicalPathKey"],

						-1, // ingestionJobKey,	(in case of live)
						-1, // deliveryCode,

						365 * 24 * 60 * 60, // ttlInSeconds, 365 days!!!
						999999,				// maxRetries,
						false,				// reuseAuthIfPresent
						false,				// playerIPToBeAuthorized
						"",					// playerCountry
						"",					// playerRegion
						false,				// save,
						"MMS_SignedURL",	// deliveryType,

						false, // warningIfMissingMediaItemKey,
						true,  // filteredByStatistic
						""	   // userId (it is not needed it
							   // filteredByStatistic is true
					);

					tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
				}

				complexFilterRoot["imagePhysicalPathName"] = sourcePhysicalPathName;
				complexFilterRoot["imagePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
				complexFiltersRoot[complexFilterIndex] = complexFilterRoot;
			}
		}
		filtersRoot["complex"] = complexFiltersRoot;
	}

	/*
	SPDLOG_INFO(
		"getReviewedFiltersRoot out"
		", filters: {}",
		JSONUtils::toString(filtersRoot)
	);
	*/

	return filtersRoot;
}
