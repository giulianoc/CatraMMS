/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.cpp
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
#include <spdlog/fmt/bundled/ranges.h>
#include <sstream>

using namespace std;
using json = nlohmann::json;

void API::ingestion(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestion";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
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

			if (!JSONUtils::isPresent(requestBodyRoot, "type"))
			{
				string errorMessage = std::format("Field is not present or it is null"
												  ", Field: type");
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string rootType = JSONUtils::as<string>(requestBodyRoot, "type", "");

			string rootLabel = JSONUtils::as<string>(requestBodyRoot, "label", "");
			bool rootHidden = JSONUtils::as<bool>(requestBodyRoot, "hidden", false);

#ifdef __POSTGRES__
			int64_t ingestionRootKey =
				_mmsEngineDBFacade->addWorkflow(trans, apiAuthorizationDetails->workspace->_workspaceKey, apiAuthorizationDetails->userKey, rootType, rootLabel, rootHidden,
					requestData.requestBody);
#else
			int64_t ingestionRootKey =
				_mmsEngineDBFacade->addIngestionRoot(conn, apiAuthorizationDetails->workspace->_workspaceKey, apiAuthorizationDetails->userKey, rootType, rootLabel, requestBody.c_str());
#endif
			requestBodyRoot["ingestionRootKey"] = ingestionRootKey;

			if (!JSONUtils::isPresent(requestBodyRoot, "task"))
			{
				string errorMessage = std::format("Field is not present or it is null"
												  ", Field: task");
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json &taskRoot = requestBodyRoot["task"];

			if (!JSONUtils::isPresent(taskRoot, "type"))
			{
				string errorMessage = std::format("Field is not present or it is null"
												  ", Field: type");
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string taskType = JSONUtils::as<string>(taskRoot, "type", "");

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

			LOG_ERROR(
				"request body parsing failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}
	}
	catch (exception &e)
	{
		LOG_ERROR(
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
		LOG_INFO(
			"Ingestion"
			", @MMS statistics@ - elapsed (secs): @{}@",
			chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()
		);
	}
	catch (exception &e)
	{
		LOG_ERROR(
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
		LOG_INFO(
			"manageWorkflowVariables"
			", requestData.requestBody: {}",
			requestBody
		);

		if (variablesValuesToBeUsedRoot == nullptr)
		{
			LOG_INFO("manageWorkflowVariables"
						", there are no variables");
		}
		else
		{
			string sVariablesValuesToBeUsedRoot = JSONUtils::toString(variablesValuesToBeUsedRoot);

			LOG_INFO(
				"manageWorkflowVariables"
				", sVariablesValuesToBeUsedRoot: {}",
				sVariablesValuesToBeUsedRoot
			);
		}

		requestBodyRoot = JSONUtils::toJson<json>(requestBody);

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
		if (JSONUtils::isPresent(requestBodyRoot, field))
		{
			json variablesRoot = requestBodyRoot[field];
			if (variablesRoot.begin() != variablesRoot.end())
			// if (variablesRoot.size() > 0)
			{
				string localRequestBody(requestBody);

				LOG_INFO("variables processing...");

				for (auto &[keyRoot, valRoot] : variablesRoot.items())
				{
					string sKey = JSONUtils::toString(json(keyRoot));
					if (sKey.length() > 2)
						sKey = sKey.substr(1, sKey.length() - 2);

					LOG_INFO(
						"variable processing"
						", sKey: {}",
						sKey
					);

					string variableToBeReplaced;
					string sValue;
					{
						json variableDetails = valRoot;

						field = "type";
						string variableType = JSONUtils::as<string>(variableDetails, field, "");

						field = "isNull";
						bool variableIsNull = JSONUtils::as<bool>(variableDetails, field, false);

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
									sValue = JSONUtils::as<string>(variableDetails, field, "");

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
									sValue = to_string(JSONUtils::as<int64_t>(variableDetails, field, 0));
							}
							else if (variableType == "decimal")
							{
								if (variableIsNull)
									sValue = "null";
								else
									sValue = to_string(JSONUtils::as<double>(variableDetails, field, 0.0));
							}
							else if (variableType == "boolean")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									bool bValue = JSONUtils::as<bool>(variableDetails, field, false);
									sValue = bValue ? "true" : "false";
								}
							}
							else if (variableType == "datetime")
							{
								if (variableIsNull)
									sValue = "";
								else
									sValue = JSONUtils::as<string>(variableDetails, field, "");
							}
							else if (variableType == "datetime-millisecs")
							{
								if (variableIsNull)
									sValue = "";
								else
									sValue = JSONUtils::as<string>(variableDetails, field, "");
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
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							LOG_INFO(
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
								sValue = JSONUtils::as<string>(variablesValuesToBeUsedRoot, sKey, "");

								// scenario, the json will be: "field":
								// "${var_name}"
								//	so in case the value of the variable
								// contains " we have 	to replace it with \"
								sValue = regex_replace(sValue, regex("\""), "\\\"");
							}
							else if (variableType == "integer")
								sValue = to_string(JSONUtils::as<int64_t>(variablesValuesToBeUsedRoot, sKey, 0));
							else if (variableType == "decimal")
								sValue = to_string(JSONUtils::as<double>(variablesValuesToBeUsedRoot, sKey, 0.0));
							else if (variableType == "boolean")
							{
								bool bValue = JSONUtils::as<bool>(variablesValuesToBeUsedRoot, sKey, false);
								sValue = bValue ? "true" : "false";
							}
							else if (variableType == "datetime")
								sValue = JSONUtils::as<string>(variablesValuesToBeUsedRoot, sKey, "");
							else if (variableType == "datetime-millisecs")
								sValue = JSONUtils::as<string>(variablesValuesToBeUsedRoot, sKey, "");
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
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							LOG_INFO(
								"variable information"
								", sKey: {}"
								", variableType: {}"
								", variableIsNull: {}"
								", sValue: {}",
								sKey, variableType, variableIsNull, sValue
							);
						}
					}

					LOG_INFO(
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

				LOG_INFO(
					"requestBody after the replacement of the variables"
					", localRequestBody: {}",
					localRequestBody
				);

				requestBodyRoot = JSONUtils::toJson<json>(localRequestBody);
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
		LOG_ERROR(errorMessage);

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

	LOG_TRACE(
		"manageReferencesInput (1)"
		", taskOrGroupOfTasksLabel: {}"
		", IngestionType: {}"
		", parametersSectionPresent: {}"
		", taskRoot: {}"
		", dependOnIngestionJobKeysOverallInput: {}",
		taskOrGroupOfTasksLabel, ingestionType, parametersSectionPresent, JSONUtils::toString(taskRoot),
		fmt::join(dependOnIngestionJobKeysOverallInput, ", ")
	);

	// initialize referencesRoot
	bool referencesSectionPresent = false;
	json referencesRoot = json::array();
	if (parametersSectionPresent)
	{
		field = "references";
		if (JSONUtils::isPresent(parametersRoot, field, true))
		{
			referencesRoot = parametersRoot[field];

			referencesSectionPresent = true;
		}
	}

	LOG_TRACE(
		"manageReferencesInput (2) referencesSectionPresent"
		", taskOrGroupOfTasksLabel: {}"
		", IngestionType: {}"
		", parametersSectionPresent: {}"
		", sDependOnIngestionJobKeysOverallInput: {}"
		", referencesSectionPresent: {}"
		", referencesRoot: {}",
		taskOrGroupOfTasksLabel, ingestionType, parametersSectionPresent,
		fmt::join(dependOnIngestionJobKeysOverallInput, ", "),
		referencesSectionPresent, JSONUtils::toString(referencesRoot)
	);

	// Generally if the References tag is present, these will be used as references for the Task.
	// In case the References tag is NOT present, inherited references are used.
	// Sometimes, we want to use both, the references coming from the tag and the inherit references.
	// For example a video is ingested and we want to overlay a logo that is already present into MMS.
	// In this case we add the Reference for the Image and we inherit the video from the Add-Content Task.
	// In these case we use the "dependenciesToBeAddedToReferencesAt" parameter.

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
		if (JSONUtils::isPresent(parametersRoot, field))
		{
			dependenciesToBeAddedToReferencesAt = JSONUtils::as<string>(parametersRoot, field, "");
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
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}
	}

	// manage ReferenceLabel, inside the References Tag, If ReferenceLabel is present, replace it with ReferenceIngestionJobKey
	if (referencesSectionPresent)
	{
		for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); ++referenceIndex)
		{
			json referenceRoot = referencesRoot[referenceIndex];

			field = "label";
			if (JSONUtils::isPresent(referenceRoot, field))
			{
				string referenceLabel = JSONUtils::as<string>(referenceRoot, field, "");

				if (referenceLabel.empty())
				{
					string errorMessage = std::format(
						"The 'label' value cannot be empty"
						", processing label: {}"
						", referenceLabel: {}",
						taskOrGroupOfTasksLabel, referenceLabel
					);
					LOG_ERROR(errorMessage);

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
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				if (ingestionJobKeys.size() > 1)
				{
					string errorMessage = std::format(
						"The 'label' value cannot be used in more than one Task"
						", referenceLabel: {}"
						", ingestionJobKeys.size(): {}",
						referenceLabel, ingestionJobKeys.size()
					);
					LOG_ERROR(errorMessage);

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
				auto itrIngestionJobKey = ranges::find(
					dependOnIngestionJobKeysForStarting, ingestionJobKeys.back());
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
		LOG_TRACE(
			"manageReferencesInput (3) new references because referencesSectionPresent (parametersRoot)"
			", taskOrGroupOfTasksLabel: {}"
			", IngestionType: {}"
			", parametersSectionPresent: {}"
			", sDependOnIngestionJobKeysOverallInput: {}"
			", referencesSectionPresent: {}"
			", parametersRoot: {}",
			taskOrGroupOfTasksLabel, ingestionType, parametersSectionPresent,
			fmt::join(dependOnIngestionJobKeysOverallInput, ", "),
			referencesSectionPresent, JSONUtils::toString(parametersRoot)
		);
	}

	// add to referencesRoot all the inherited references
	if ((!referencesSectionPresent || dependenciesToBeAddedToReferencesAtIndex != -1) && !dependOnIngestionJobKeysOverallInput.empty())
	{
		// Enter here if No References tag is present (so we have to add the
		// inherit input) OR we want to add dependOnReferences to the Raferences
		// tag

		if (dependenciesToBeAddedToReferencesAtIndex != -1)
		{
			{
				int previousReferencesRootSize = referencesRoot.size();
				int dependOnIngestionJobKeysSize = dependOnIngestionJobKeysOverallInput.size();

				LOG_TRACE(
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
					LOG_TRACE(
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
					LOG_TRACE(
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
			taskRoot[field] = parametersRoot;

		LOG_TRACE(
			"manageReferencesInput (4) add to referencesRoot all the inherited references"
			", ingestionRootKey: {}"
			", taskOrGroupOfTasksLabel: {}"
			", IngestionType: {}"
			", parametersSectionPresent: {}"
			", referencesSectionPresent: {}"
			", dependenciesToBeAddedToReferencesAtIndex: {}"
			", dependOnIngestionJobKeysOverallInput: {}"
			", parametersRoot: {}",
			ingestionRootKey, taskOrGroupOfTasksLabel, ingestionType, parametersSectionPresent, referencesSectionPresent,
			dependenciesToBeAddedToReferencesAtIndex, fmt::join(dependOnIngestionJobKeysOverallInput, ", "),
			JSONUtils::toString(parametersRoot)
		);
	}
	else
		LOG_TRACE(
			"manageReferencesInput (4) NO inherited references"
			", ingestionRootKey: {}"
			", taskOrGroupOfTasksLabel: {}"
			", IngestionType: {}"
			", parametersSectionPresent: {}"
			", referencesSectionPresent: {}"
			", dependenciesToBeAddedToReferencesAtIndex: {}"
			", dependOnIngestionJobKeysOverallInput: {}"
			", parametersRoot: {}",
			ingestionRootKey, taskOrGroupOfTasksLabel, ingestionType, parametersSectionPresent, referencesSectionPresent,
			dependenciesToBeAddedToReferencesAtIndex, fmt::join(dependOnIngestionJobKeysOverallInput, ", "),
			JSONUtils::toString(parametersRoot)
		);
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
	string type = JSONUtils::as<string>(taskRoot, field, "");

	string taskLabel;
	field = "label";
	taskLabel = JSONUtils::as<string>(taskRoot, field, "");

	LOG_INFO(
		"Processing SingleTask..."
		", ingestionRootKey: {}"
		", type: {}"
		", taskLabel: {}",
		ingestionRootKey, type, taskLabel
	);

	field = "parameters";
	json parametersRoot;
	bool parametersSectionPresent = false;
	if (JSONUtils::isPresent(taskRoot, field, true))
	{
		parametersRoot = taskRoot[field];

		parametersSectionPresent = true;
	}

	// 2022-11-19: in case of Broadcaster, internalMMS already exist
	field = "internalMMS";
	json internalMMSRoot;
	if (JSONUtils::isPresent(parametersRoot, field))
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
		// we will create a group of tasks and add there the Encode task in two scenarios:
		// case 1. in case of EncodingProfilesSet
		// case 2. in case we will have more than one References

		string encodingProfilesSetKeyField = "encodingProfilesSetKey";
		string encodingProfilesSetLabelField = "encodingProfilesSetLabel";
		string referencesField = "references";

		if (parametersSectionPresent &&
			(
				// case 1
				(JSONUtils::isPresent(parametersRoot, encodingProfilesSetKeyField) ||
				 JSONUtils::isPresent(parametersRoot, encodingProfilesSetLabelField)) ||
				// case 2
				(JSONUtils::isPresent(parametersRoot, referencesField) && parametersRoot[referencesField].size() > 1)
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

			if (JSONUtils::isPresent(parametersRoot, encodingProfilesSetKeyField) ||
				JSONUtils::isPresent(parametersRoot, encodingProfilesSetLabelField))
			{
				// case 1

				profilesSetPresent = true;

				string encodingProfilesSetReference;

				if (JSONUtils::isPresent(parametersRoot, encodingProfilesSetKeyField))
				{
					int64_t encodingProfilesSetKey = JSONUtils::as<int64_t>(parametersRoot, encodingProfilesSetKeyField, 0);

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
				else // if (JSONUtils::isPresent(parametersRoot,
					 // encodingProfilesSetLabelField))
				{
					string encodingProfilesSetLabel = JSONUtils::as<string>(parametersRoot, encodingProfilesSetLabelField, "");

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
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			// both, case 1 and case 2 could have multiple references
			json multiReferencesRoot;
			if (JSONUtils::isPresent(parametersRoot, referencesField) && parametersRoot[referencesField].size() > 1)
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
				LOG_ERROR(errorMessage);

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
			if (JSONUtils::isPresent(taskRoot, field, true))
				newTasksGroupRoot[field] = taskRoot[field];

			field = "onError";
			if (JSONUtils::isPresent(taskRoot, field, true))
				newTasksGroupRoot[field] = taskRoot[field];

			field = "onComplete";
			if (JSONUtils::isPresent(taskRoot, field, true))
				newTasksGroupRoot[field] = taskRoot[field];

#ifdef __POSTGRES__
			return ingestionGroupOfTasks(
				trans, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting,
				dependOnSuccess, dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
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
		LOG_INFO(
			"No special management for Encode"
			", ingestionRootKey: {}"
			", taskLabel: {}"
			", workspace->_workspaceKey: {}",
			ingestionRootKey, taskLabel, workspace->_workspaceKey
		);
	}
	else if (type == "Face-Recognition")
	{
		// In case we will have more than one References, we will create a group of tasks and add there the Face-Recognition task

		string referencesField = "references";

		if (parametersSectionPresent
			&& (JSONUtils::isPresent(parametersRoot, referencesField) && parametersRoot[referencesField].size() > 1))
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

				newTaskRoot["label"] = localLabel;
				newTaskRoot["type"] = "Face-Recognition";

				json newParametersRoot = parametersRoot;

				{
					json newReferencesRoot = json::array();
					newReferencesRoot.push_back(multiReferencesRoot[referenceIndex]);

					newParametersRoot["references"] = newReferencesRoot;
				}

				newTaskRoot["parameters"] = newParametersRoot;

				if (JSONUtils::isPresent(taskRoot, "onSuccess", true))
					newTaskRoot["onSuccess"] = taskRoot["onSuccess"];

				if (JSONUtils::isPresent(taskRoot, "onError", true))
					newTaskRoot["onError"] = taskRoot["onError"];

				if (JSONUtils::isPresent(taskRoot, "onComplete", true))
					newTaskRoot["onComplete"] = taskRoot["onComplete"];

				newTasksRoot.push_back(newTaskRoot);
			}

			json newParametersTasksGroupRoot;

			newParametersTasksGroupRoot["executionType"] = "parallel";
			newParametersTasksGroupRoot["tasks"] = newTasksRoot;

			json newTasksGroupRoot;

			newTasksGroupRoot["type"] = "GroupOfTasks";
			newTasksGroupRoot["parameters"] = newParametersTasksGroupRoot;

#ifdef __POSTGRES__
			return ingestionGroupOfTasks(
				trans, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting,
				dependOnSuccess, dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
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
		LOG_INFO(
			"No special management for Face-Recognition"
			", ingestionRootKey: {}"
			", taskLabel: {}"
			", workspace->_workspaceKey: {}",
			ingestionRootKey, taskLabel, workspace->_workspaceKey
		);
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
			if (JSONUtils::isPresent(taskRoot, onSuccessField, true)
				|| JSONUtils::isPresent(taskRoot, onErrorField, true)
				|| JSONUtils::isPresent(taskRoot, onCompleteField, true))
			{
				if (JSONUtils::isPresent(taskRoot, onSuccessField, true))
				{
					json onSuccessRoot = taskRoot[onSuccessField];

					eventsRoot[onSuccessField] = onSuccessRoot;

					taskRoot.erase(onSuccessField);
				}
				if (JSONUtils::isPresent(taskRoot, onErrorField, true))
				{
					json onErrorRoot = taskRoot[onErrorField];

					eventsRoot[onErrorField] = onErrorRoot;

					taskRoot.erase(onErrorField);
				}
				if (JSONUtils::isPresent(taskRoot, onCompleteField, true))
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
			if (JSONUtils::isPresent(taskRoot, onSuccessField, true)
				|| JSONUtils::isPresent(taskRoot, onErrorField, true)
				|| JSONUtils::isPresent(taskRoot, onCompleteField, true))
			{
				if (JSONUtils::isPresent(taskRoot, onSuccessField, true))
				{
					json onSuccessRoot = taskRoot[onSuccessField];

					eventsRoot[onSuccessField] = onSuccessRoot;

					taskRoot.erase(onSuccessField);
				}
				if (JSONUtils::isPresent(taskRoot, onErrorField, true))
				{
					json onErrorRoot = taskRoot[onErrorField];

					eventsRoot[onErrorField] = onErrorRoot;

					taskRoot.erase(onErrorField);
				}
				if (JSONUtils::isPresent(taskRoot, onCompleteField, true))
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
		if (JSONUtils::isPresent(parametersRoot, field))
		{
			string referenceLabel = JSONUtils::as<string>(parametersRoot, field, "");

			if (referenceLabel.empty())
			{
				string errorMessage = std::format(
					"The 'label' value cannot be empty"
					", ingestionRootKey: {}"
					", type: {}"
					", taskLabel: {}"
					", referenceLabel: {}",
					ingestionRootKey, type, taskLabel, referenceLabel
				);
				LOG_ERROR(errorMessage);

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
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (ingestionJobKeys.size() > 1)
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
				LOG_ERROR(errorMessage);

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
			if (!JSONUtils::isPresent(parametersRoot, workflowAsLibraryTypeField) ||
				!JSONUtils::isPresent(parametersRoot, workflowAsLibraryLabelField))
			{
				string errorMessage = __FILEREF__ +
									  "No workflowAsLibraryType/WorkflowAsLibraryLabel "
									  "parameters into the Workflow-As-Library Task" +
									  ", ingestionRootKey: " + to_string(ingestionRootKey) + ", type: " + type + ", taskLabel: " + taskLabel;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string workflowAsLibraryType = JSONUtils::as<string>(parametersRoot, workflowAsLibraryTypeField, "");
			string workflowAsLibraryLabel = JSONUtils::as<string>(parametersRoot, workflowAsLibraryLabelField, "");

			int64_t workspaceKey;
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
			if (!JSONUtils::isPresent(workflowLibraryRoot, workflowRootTaskField))
			{
				string errorMessage = __FILEREF__ +
									  "Wrong Workflow-As-Library format. Root Task was not "
									  "found" +
									  ", ingestionRootKey: " + to_string(ingestionRootKey) + ", type: " + type + ", taskLabel: " + taskLabel +
									  ", workflowLibraryContent: " + workflowLibraryContent;
				LOG_ERROR(errorMessage);

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
			if (JSONUtils::isPresent(taskRoot, field, true))
				newGroupOfTasksRoot[field] = taskRoot[field];

			field = "onError";
			if (JSONUtils::isPresent(taskRoot, field, true))
				newGroupOfTasksRoot[field] = taskRoot[field];

			field = "onComplete";
			if (JSONUtils::isPresent(taskRoot, field, true))
				newGroupOfTasksRoot[field] = taskRoot[field];
		}

#ifdef __POSTGRES__
		return ingestionGroupOfTasks(
			trans, userKey, apiKey, workspace, ingestionRootKey, newGroupOfTasksRoot, dependOnIngestionJobKeysForStarting,
			dependOnSuccess, dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
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

	manageReferencesInput(
		ingestionRootKey, taskLabel, type, taskRoot, parametersSectionPresent, parametersRoot,
		dependOnIngestionJobKeysForStarting, dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey
	);

	string taskMetadata;

	// if (parametersSectionPresent)
	{
		taskMetadata = JSONUtils::toString(parametersRoot);
	}

	vector<int64_t> waitForGlobalIngestionJobKeys;
	{
		field = "waitFor";
		if (JSONUtils::isPresent(parametersRoot, field))
		{
			json waitForRoot = parametersRoot[field];

			for (const auto& waitForLabelRoot : waitForRoot)
			{
				field = "globalIngestionLabel";
				if (JSONUtils::isPresent(waitForLabelRoot, field))
				{
					string waitForGlobalIngestionLabel = JSONUtils::as<string>(waitForLabelRoot, field, "");

					_mmsEngineDBFacade->ingestionJob_IngestionJobKeys(
						workspace->_workspaceKey, waitForGlobalIngestionLabel,
						// 2022-12-18: true perchè IngestionJob dovrebbe essere
						// stato appena aggiunto
						true, waitForGlobalIngestionJobKeys
					);
					LOG_INFO(
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
		if (JSONUtils::isPresent(parametersRoot, field))
			processingStartingFrom = JSONUtils::as<string>(parametersRoot, field, "");

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

	LOG_INFO(
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
		trans, workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type),
		processingStartingFrom, dependOnIngestionJobKeysForStarting, dependOnSuccess,
		waitForGlobalIngestionJobKeys
	);
#else
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(
		conn, workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type), processingStartingFrom,
		dependOnIngestionJobKeysForStarting, dependOnSuccess, waitForGlobalIngestionJobKeys
	);
#endif
	field = "ingestionJobKey";
	taskRoot[field] = localDependOnIngestionJobKeyExecution;

	LOG_INFO(
		"Save Label..."
		", ingestionRootKey: {}"
		", taskLabel: {}"
		", localDependOnIngestionJobKeyExecution: {}",
		ingestionRootKey, taskLabel, localDependOnIngestionJobKeyExecution
	);
	if (!taskLabel.empty())
		(mapLabelAndIngestionJobKey[taskLabel]).push_back(localDependOnIngestionJobKeyExecution);

	{
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
	groupOfTaskLabel = JSONUtils::as<string>(groupOfTasksRoot, field, "");

	LOG_INFO(
		"Processing GroupOfTasks..."
		", ingestionRootKey: {}"
		", groupOfTaskLabel: {}",
		ingestionRootKey, groupOfTaskLabel
	);

	// initialize parametersRoot
	field = "parameters";
	if (!JSONUtils::isPresent(groupOfTasksRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	json &parametersRoot = groupOfTasksRoot[field];

	bool parallelTasks;

	field = "executionType";
	if (!JSONUtils::isPresent(parametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	string executionType = JSONUtils::as<string>(parametersRoot, field, "");
	if (executionType == "parallel")
		parallelTasks = true;
	else if (executionType == "sequential")
		parallelTasks = false;
	else
	{
		string errorMessage = __FILEREF__ + "executionType field is wrong" + ", executionType: " + executionType;
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	field = "tasks";
	if (!JSONUtils::isPresent(parametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	json &tasksRoot = parametersRoot[field];

	/* 2021-02-20: A group that does not have any Task couls be a scenario,
	 * so we do not have to raise an error. Same check commented in
Validation.cpp if (tasksRoot.size() == 0)
{
	string errorMessage = __FILEREF__ + "No Tasks are present inside the
GroupOfTasks item"; LOG_ERROR(errorMessage);

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
		if (!JSONUtils::isPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::as<string>(taskRoot, field, "");

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
		if (JSONUtils::isPresent(parametersRoot, field))
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
				if (JSONUtils::isPresent(referenceOutputRoot, field))
				{
					string referenceLabel = JSONUtils::as<string>(referenceOutputRoot, field, "");

					if (referenceLabel.empty())
					{
						string errorMessage = __FILEREF__ + "The 'label' value cannot be empty" + ", referenceLabel: " + referenceLabel;
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

					if (ingestionJobKeys.empty())
					{
						string errorMessage = __FILEREF__ + "The 'label' value is not found" + ", referenceLabel: " + referenceLabel +
											  ", groupOfTasksRoot: " + JSONUtils::toString(groupOfTasksRoot);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					else if (ingestionJobKeys.size() > 1)
					{
						string errorMessage = __FILEREF__ +
											  "The 'label' value cannot be used in more than one "
											  "Task" +
											  ", referenceLabel: " + referenceLabel +
											  ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size());
						LOG_ERROR(errorMessage);

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
			LOG_INFO(
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

			LOG_INFO(
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
		processingStartingFrom = JSONUtils::as<string>(parametersRoot, field, "");

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

	LOG_INFO(
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

	LOG_INFO(
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
	if (JSONUtils::isPresent(taskOrGroupOfTasksRoot, field, true))
	{
		json &onSuccessRoot = taskOrGroupOfTasksRoot[field];

		field = "task";
		if (!JSONUtils::isPresent(onSuccessRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onSuccessRoot[field];

		string field = "type";
		if (!JSONUtils::isPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::as<string>(taskRoot, field, "");

		field = "label";
		string taskLabel = JSONUtils::as<string>(taskRoot, field, "");

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
	if (JSONUtils::isPresent(taskOrGroupOfTasksRoot, field, true))
	{
		json &onErrorRoot = taskOrGroupOfTasksRoot[field];

		field = "task";
		if (!JSONUtils::isPresent(onErrorRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onErrorRoot[field];

		string field = "type";
		if (!JSONUtils::isPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::as<string>(taskRoot, field, "");

		field = "label";
		string taskLabel = JSONUtils::as<string>(taskRoot, field, "");

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
	if (JSONUtils::isPresent(taskOrGroupOfTasksRoot, field, true))
	{
		json &onCompleteRoot = taskOrGroupOfTasksRoot[field];

		field = "task";
		if (!JSONUtils::isPresent(onCompleteRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onCompleteRoot[field];

		string field = "type";
		if (!JSONUtils::isPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::as<string>(taskRoot, field, "");

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
