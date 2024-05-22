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
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "PersistenceLock.h"
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fstream>
#include <regex>
#include <sstream>

void API::ingestion(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, string apiKey,
	shared_ptr<Workspace> workspace, unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "ingestion";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();

		json requestBodyRoot = manageWorkflowVariables(requestBody, nullptr);

		// string responseBody;
		json responseBodyRoot;
		json responseBodyTasksRoot = json::array();

#ifdef __POSTGRES__
		shared_ptr<PostgresConnection> conn = _mmsEngineDBFacade->beginIngestionJobs();
		work trans{*(conn->_sqlConnection)};
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

			Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
			// it starts from the root and validate recursively the entire body
			validator.validateIngestedRootMetadata(workspace->_workspaceKey, requestBodyRoot);

			string field = "type";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string rootType = JSONUtils::asString(requestBodyRoot, field, "");

			string rootLabel;
			field = "label";
			rootLabel = JSONUtils::asString(requestBodyRoot, field, "");

#ifdef __POSTGRES__
			int64_t ingestionRootKey =
				_mmsEngineDBFacade->addIngestionRoot(conn, trans, workspace->_workspaceKey, userKey, rootType, rootLabel, requestBody.c_str());
#else
			int64_t ingestionRootKey =
				_mmsEngineDBFacade->addIngestionRoot(conn, workspace->_workspaceKey, userKey, rootType, rootLabel, requestBody.c_str());
#endif
			field = "ingestionRootKey";
			requestBodyRoot[field] = ingestionRootKey;

			field = "task";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			json &taskRoot = requestBodyRoot[field];

			field = "type";
			if (!JSONUtils::isMetadataPresent(taskRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string taskType = JSONUtils::asString(taskRoot, field, "");

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
					conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysForStarting, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#else
				ingestionGroupOfTasks(
					conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
					conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
					dependOnIngestionJobKeysForStarting, mapLabelAndIngestionJobKey,
					/* responseBody, */ responseBodyTasksRoot
				);
#else
				ingestionSingleTask(
					conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
			_mmsEngineDBFacade->endIngestionJobs(conn, trans, commit, ingestionRootKey, processedMetadataContent);
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
		catch (AlreadyLocked &e)
		{
			bool commit = false;
#ifdef __POSTGRES__
			_mmsEngineDBFacade->endIngestionJobs(conn, trans, commit, -1, string());
#else
			_mmsEngineDBFacade->endIngestionJobs(conn, commit, -1, string());
#endif

			_logger->error(__FILEREF__ + "Ingestion locked" + ", e.what(): " + e.what());

			throw e;
		}
		catch (runtime_error &e)
		{
			bool commit = false;
#ifdef __POSTGRES__
			_mmsEngineDBFacade->endIngestionJobs(conn, trans, commit, -1, string());
#else
			_mmsEngineDBFacade->endIngestionJobs(conn, commit, -1, string());
#endif

			_logger->error(__FILEREF__ + "request body parsing failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			bool commit = false;
#ifdef __POSTGRES__
			_mmsEngineDBFacade->endIngestionJobs(conn, trans, commit, -1, string());
#else
			_mmsEngineDBFacade->endIngestionJobs(conn, commit, -1, string());
#endif

			_logger->error(__FILEREF__ + "request body parsing failed" + ", e.what(): " + e.what());

			throw e;
		}

		string responseBody = JSONUtils::toString(responseBodyRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		_logger->info(
			__FILEREF__ + "Ingestion" + ", @MMS statistics@ - elapsed (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
		);
	}
	catch (AlreadyLocked &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

json API::manageWorkflowVariables(string requestBody, json variablesValuesToBeUsedRoot)
{
	json requestBodyRoot;

	try
	{
		_logger->info(__FILEREF__ + "manageWorkflowVariables" + ", requestBody: " + requestBody);

		if (variablesValuesToBeUsedRoot == nullptr)
		{
			_logger->info(__FILEREF__ + "manageWorkflowVariables" + ", there are no variables");
		}
		else
		{
			string sVariablesValuesToBeUsedRoot = JSONUtils::toString(variablesValuesToBeUsedRoot);

			_logger->info(__FILEREF__ + "manageWorkflowVariables" + ", sVariablesValuesToBeUsedRoot: " + sVariablesValuesToBeUsedRoot);
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
				string localRequestBody = requestBody;

				_logger->info(__FILEREF__ + "variables processing...");

				for (auto &[keyRoot, valRoot] : variablesRoot.items())
				{
					string sKey = JSONUtils::toString(keyRoot);
					if (sKey.length() > 2)
						sKey = sKey.substr(1, sKey.length() - 2);

					_logger->info(__FILEREF__ + "variable processing" + ", sKey: " + sKey);

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
								string errorMessage = __FILEREF__ + "Wrong Variable Type parsing RequestBody" + ", variableType: " + variableType +
													  ", requestBody: " + requestBody;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}

							_logger->info(
								__FILEREF__ + "variable information" + ", sKey: " + sKey + ", variableType: " + variableType +
								", variableIsNull: " + to_string(variableIsNull) + ", sValue: " + sValue
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
								string errorMessage = __FILEREF__ + "Wrong Variable Type parsing RequestBody" + ", variableType: " + variableType +
													  ", requestBody: " + requestBody;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}

							_logger->info(
								__FILEREF__ + "variable information" + ", sKey: " + sKey + ", variableType: " + variableType +
								", variableIsNull: " + to_string(variableIsNull) + ", sValue: " + sValue
							);
						}
					}

					_logger->info(__FILEREF__ + "requestBody, replace" + ", variableToBeReplaced: " + variableToBeReplaced + ", sValue: " + sValue);
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

				_logger->info(__FILEREF__ + "requestBody after the replacement of the variables" + ", localRequestBody: " + localRequestBody);

				requestBodyRoot = JSONUtils::toJson(localRequestBody);
			}
		}
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
		_logger->error(__FILEREF__ + errorMessage);

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

		_logger->info(
			__FILEREF__ + "manageReferencesInput" + ", taskOrGroupOfTasksLabel: " + taskOrGroupOfTasksLabel + ", IngestionType: " + ingestionType +
			", parametersSectionPresent: " + to_string(parametersSectionPresent) +
			", sDependOnIngestionJobKeysOverallInput: " + sDependOnIngestionJobKeysOverallInput
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
			if (dependenciesToBeAddedToReferencesAt != "")
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
					catch (exception e)
					{
						string errorMessage = string("dependenciesToBeAddedToReferencesAt is not "
													 "well format") +
											  ", dependenciesToBeAddedToReferencesAt: " + dependenciesToBeAddedToReferencesAt;
						_logger->error(__FILEREF__ + errorMessage);

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
		bool referencesChanged = false;

		for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); ++referenceIndex)
		{
			json referenceRoot = referencesRoot[referenceIndex];

			field = "label";
			if (JSONUtils::isMetadataPresent(referenceRoot, field))
			{
				string referenceLabel = JSONUtils::asString(referenceRoot, field, "");

				if (referenceLabel == "")
				{
					string errorMessage = __FILEREF__ + "The 'label' value cannot be empty" + ", processing label: " + taskOrGroupOfTasksLabel +
										  ", referenceLabel: " + referenceLabel;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

				if (ingestionJobKeys.size() == 0)
				{
					string errorMessage = __FILEREF__ + "The 'label' value is not found" + ", processing label: " + taskOrGroupOfTasksLabel +
										  ", referenceLabel: " + referenceLabel;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (ingestionJobKeys.size() > 1)
				{
					string errorMessage = __FILEREF__ +
										  "The 'label' value cannot be used in more than one "
										  "Task" +
										  ", referenceLabel: " + referenceLabel + ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size());
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				field = "ingestionJobKey";
				referenceRoot[field] = ingestionJobKeys.back();

				referencesRoot[referenceIndex] = referenceRoot;

				field = "references";
				parametersRoot[field] = referencesRoot;

				referencesChanged = true;

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

	_logger->info(
		__FILEREF__ + "add to referencesRoot all the inherited references?" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
		", taskOrGroupOfTasksLabel: " + taskOrGroupOfTasksLabel + ", IngestionType: " + ingestionType +
		", parametersSectionPresent: " + to_string(parametersSectionPresent) + ", referencesSectionPresent: " + to_string(referencesSectionPresent) +
		", dependenciesToBeAddedToReferencesAtIndex: " + to_string(dependenciesToBeAddedToReferencesAtIndex) +
		", dependOnIngestionJobKeysOverallInput.size(): " + to_string(dependOnIngestionJobKeysOverallInput.size())
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

				_logger->info(
					__FILEREF__ + "add to referencesRoot all the inherited references" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
					", taskOrGroupOfTasksLabel: " + taskOrGroupOfTasksLabel + ", previousReferencesRootSize: " +
					to_string(previousReferencesRootSize) + ", dependOnIngestionJobKeysSize: " + to_string(dependOnIngestionJobKeysSize) +
					", dependenciesToBeAddedToReferencesAtIndex: " + to_string(dependenciesToBeAddedToReferencesAtIndex)
				);

				// referencesRoot.resize(
				//   previousReferencesRootSize + dependOnIngestionJobKeysSize
				// );
				for (int index = previousReferencesRootSize - 1; index >= dependenciesToBeAddedToReferencesAtIndex; index--)
				{
					_logger->info(
						__FILEREF__ + "making 'space' in referencesRoot" + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", from " +
						to_string(index) + " to " + to_string(index + dependOnIngestionJobKeysSize)
					);

					referencesRoot[index + dependOnIngestionJobKeysSize] = referencesRoot[index];
				}

				for (int index = dependenciesToBeAddedToReferencesAtIndex;
					 index < dependenciesToBeAddedToReferencesAtIndex + dependOnIngestionJobKeysSize; index++)
				{
					_logger->info(
						__FILEREF__ + "fill in dependOnIngestionJobKey" + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", from " +
						to_string(index - dependenciesToBeAddedToReferencesAtIndex) + " to " + to_string(index)
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
			for (int referenceIndex = 0; referenceIndex < dependOnIngestionJobKeysOverallInput.size(); ++referenceIndex)
			{
				json referenceRoot;
				string addedField = "ingestionJobKey";
				referenceRoot[addedField] = dependOnIngestionJobKeysOverallInput.at(referenceIndex);

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

		_logger->info(
			__FILEREF__ + "testttttttt" + ", taskMetadata: " + taskMetadata + ", referencesSectionPresent: " + to_string(referencesSectionPresent) +
			", dependenciesToBeAddedToReferencesAtIndex: " + to_string(dependenciesToBeAddedToReferencesAtIndex) +
			", dependOnIngestionJobKeysOverallInput.size: " + to_string(dependOnIngestionJobKeysOverallInput.size())
		);
	}
}

// return: ingestionJobKey associated to this task
#ifdef __POSTGRES__
vector<int64_t> API::ingestionSingleTask(
	shared_ptr<PostgresConnection> conn, work &trans, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
	json &taskRoot,

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

	_logger->info(
		__FILEREF__ + "Processing SingleTask..." + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", type: " + type +
		", taskLabel: " + taskLabel
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

				if (encodingProfilesSetKeys.size() == 0)
				{
					string errorMessage = __FILEREF__ +
										  "No EncodingProfileKey into the "
										  "encodingProfilesSetKey" +
										  ", encodingProfilesSetKey/encodingProfilesSetLabel: " + encodingProfilesSetReference +
										  ", ingestionRootKey: " + to_string(ingestionRootKey) + ", type: " + type + ", taskLabel: " + taskLabel;
					_logger->error(errorMessage);

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
				string errorMessage = __FILEREF__ + "It's not possible to be here" + ", type: " + type + ", taskLabel: " + taskLabel;
				_logger->error(errorMessage);

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
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
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
			_logger->info(
				__FILEREF__ + "No special management for Encode" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
				", taskLabel: " + taskLabel + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
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
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
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
			_logger->info(
				__FILEREF__ + "No special management for Face-Recognition" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
				", taskLabel: " + taskLabel + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
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
				string errorMessage = __FILEREF__ + "The 'label' value cannot be empty" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
									  ", type: " + type + ", taskLabel: " + taskLabel + ", referenceLabel: " + referenceLabel;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

			if (ingestionJobKeys.size() == 0)
			{
				string errorMessage = __FILEREF__ + "The 'label' value is not found" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
									  ", type: " + type + ", taskLabel: " + taskLabel + ", referenceLabel: " + referenceLabel;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			else if (ingestionJobKeys.size() > 1)
			{
				string errorMessage = __FILEREF__ + "The 'label' value cannot be used in more than one Task" +
									  ", ingestionRootKey: " + to_string(ingestionRootKey) + ", type: " + type + ", taskLabel: " + taskLabel +
									  ", referenceLabel: " + referenceLabel + ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size());
				_logger->error(errorMessage);

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
				_logger->error(errorMessage);

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
				_logger->error(errorMessage);

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
			conn, trans, userKey, apiKey, workspace, ingestionRootKey, newGroupOfTasksRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
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
			_logger->info(__FILEREF__ + "IngestionJob to be added"
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

					_mmsEngineDBFacade->getIngestionJobsKeyByGlobalLabel(
						workspace->_workspaceKey, waitForGlobalIngestionLabel,
						// 2022-12-18: true perchè IngestionJob dovrebbe essere
						// stato appena aggiunto
						true, waitForGlobalIngestionJobKeys
					);
					_logger->info(
						__FILEREF__ + "getIngestionJobsKeyByGlobalLabel" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
						", taskLabel: " + taskLabel + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) +
						", waitForGlobalIngestionLabel: " + waitForGlobalIngestionLabel +
						", waitForGlobalIngestionJobKeys.size(): " + to_string(waitForGlobalIngestionJobKeys.size())
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

		if (processingStartingFrom == "")
		{
			tm tmUTCDateTime;
			char sProcessingStartingFrom[64];

			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcNow = chrono::system_clock::to_time_t(now);

			gmtime_r(&utcNow, &tmUTCDateTime);
			sprintf(
				sProcessingStartingFrom, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmUTCDateTime.tm_year + 1900, tmUTCDateTime.tm_mon + 1,
				tmUTCDateTime.tm_mday, tmUTCDateTime.tm_hour, tmUTCDateTime.tm_min, tmUTCDateTime.tm_sec
			);

			processingStartingFrom = sProcessingStartingFrom;
		}
	}

	_logger->info(
		__FILEREF__ + "add IngestionJob" + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", taskLabel: " + taskLabel +
		", taskMetadata: " + taskMetadata + ", IngestionType: " + type + ", processingStartingFrom: " + processingStartingFrom +
		", dependOnIngestionJobKeysForStarting.size(): " + to_string(dependOnIngestionJobKeysForStarting.size()) + ", dependOnSuccess: " +
		to_string(dependOnSuccess) + ", waitForGlobalIngestionJobKeys.size(): " + to_string(waitForGlobalIngestionJobKeys.size())
	);

#ifdef __POSTGRES__
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(
		conn, trans, workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type),
		processingStartingFrom, dependOnIngestionJobKeysForStarting, dependOnSuccess, waitForGlobalIngestionJobKeys
	);
#else
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(
		conn, workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type), processingStartingFrom,
		dependOnIngestionJobKeysForStarting, dependOnSuccess, waitForGlobalIngestionJobKeys
	);
#endif
	field = "ingestionJobKey";
	taskRoot[field] = localDependOnIngestionJobKeyExecution;

	_logger->info(
		__FILEREF__ + "Save Label..." + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", taskLabel: " + taskLabel +
		", localDependOnIngestionJobKeyExecution: " + to_string(localDependOnIngestionJobKeyExecution)
	);
	if (taskLabel != "")
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
		conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, localDependOnIngestionJobKeysForStarting,
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
	shared_ptr<PostgresConnection> conn, work &trans, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
	json &groupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
	vector<int64_t> dependOnIngestionJobKeysOverallInput, unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
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

	_logger->info(
		__FILEREF__ + "Processing GroupOfTasks..." + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", groupOfTaskLabel: " + groupOfTaskLabel
	);

	// initialize parametersRoot
	field = "parameters";
	if (!JSONUtils::isMetadataPresent(groupOfTasksRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	json &parametersRoot = groupOfTasksRoot[field];

	bool parallelTasks;

	field = "executionType";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		_logger->error(errorMessage);

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
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	field = "tasks";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	json &tasksRoot = parametersRoot[field];

	/* 2021-02-20: A group that does not have any Task couls be a scenario,
	 * so we do not have to raise an error. Same check commented in
Validation.cpp if (tasksRoot.size() == 0)
{
	string errorMessage = __FILEREF__ + "No Tasks are present inside the
GroupOfTasks item"; _logger->error(errorMessage);

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
			_logger->error(errorMessage);

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
					conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
					conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
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
						conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting,
						localDependOnSuccess, dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
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
						conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, dependOnSuccess,
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
						conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, lastDependOnIngestionJobKeysForStarting,
						localDependOnSuccess, dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
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
						conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, lastDependOnIngestionJobKeysForStarting,
						localDependOnSuccess, dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
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

			referencesOutputPresent = referencesOutputRoot.size() > 0;
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

					if (referenceLabel == "")
					{
						string errorMessage = __FILEREF__ + "The 'label' value cannot be empty" + ", referenceLabel: " + referenceLabel;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

					if (ingestionJobKeys.size() == 0)
					{
						string errorMessage = __FILEREF__ + "The 'label' value is not found" + ", referenceLabel: " + referenceLabel +
											  ", groupOfTasksRoot: " + JSONUtils::toString(groupOfTasksRoot);
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					else if (ingestionJobKeys.size() > 1)
					{
						string errorMessage = __FILEREF__ +
											  "The 'label' value cannot be used in more than one "
											  "Task" +
											  ", referenceLabel: " + referenceLabel +
											  ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size());
						_logger->error(errorMessage);

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
			_logger->info(
				__FILEREF__ + "add to referencesOutputRoot all the inherited references?" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
				", groupOfTaskLabel: " + groupOfTaskLabel + ", referencesOutputPresent: " + to_string(referencesOutputPresent) +
				", "
				"newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size():"
				" " +
				to_string(newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size())
			);

			// Enter here if No ReferencesOutput tag is present (so we have to
			// add the inherit input) OR we want to add dependOnReferences to
			// the Raferences tag

			for (int referenceIndex = 0; referenceIndex < newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(); ++referenceIndex)
			{
				json referenceOutputRoot;
				field = "ingestionJobKey";
				referenceOutputRoot[field] = newDependOnIngestionJobKeysOverallInputBecauseOfTasks.at(referenceIndex);

				referencesOutputRoot.push_back(referenceOutputRoot);

				referencesOutputIngestionJobKeys.push_back(newDependOnIngestionJobKeysOverallInputBecauseOfTasks.at(referenceIndex));
			}

			_logger->info(
				__FILEREF__ +
				"Since ReferencesOutput is not present, set automatically the "
				"ReferencesOutput array tag using the ingestionJobKey of the "
				"Tasks" +
				", ingestionRootKey: " + to_string(ingestionRootKey) + ", groupOfTaskLabel: " + groupOfTaskLabel +
				", "
				"newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size():"
				" " +
				to_string(newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size()) +
				", referencesOutputRoot.size: " + to_string(referencesOutputRoot.size())
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

		if (processingStartingFrom == "")
		{
			tm tmUTCDateTime;
			char sProcessingStartingFrom[64];

			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcNow = chrono::system_clock::to_time_t(now);

			gmtime_r(&utcNow, &tmUTCDateTime);
			sprintf(
				sProcessingStartingFrom, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmUTCDateTime.tm_year + 1900, tmUTCDateTime.tm_mon + 1,
				tmUTCDateTime.tm_mday, tmUTCDateTime.tm_hour, tmUTCDateTime.tm_min, tmUTCDateTime.tm_sec
			);

			processingStartingFrom = sProcessingStartingFrom;
		}
	}

	string taskMetadata;
	{
		taskMetadata = JSONUtils::toString(parametersRoot);
	}

	_logger->info(
		__FILEREF__ + "add IngestionJob (Group of Tasks)" + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", groupOfTaskLabel: " +
		groupOfTaskLabel + ", taskMetadata: " + taskMetadata + ", IngestionType: " + type + ", processingStartingFrom: " + processingStartingFrom +
		", newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(): " + to_string(newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size()) +
		", "
		"newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.size("
		"): " +
		to_string(newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.size()) + ", dependOnSuccess: " + to_string(dependOnSuccess) +
		", referencesOutputPresent: " + to_string(referencesOutputPresent)
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
		conn, trans, workspace->_workspaceKey, ingestionRootKey, groupOfTaskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type),
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
			_mmsEngineDBFacade->updateIngestionJobParentGroupOfTasks(conn, trans, childIngestionJobKey, parentGroupOfTasksIngestionJobKey);
#else
			_mmsEngineDBFacade->updateIngestionJobParentGroupOfTasks(conn, childIngestionJobKey, parentGroupOfTasksIngestionJobKey);
#endif
		}
	}

	_logger->info(
		__FILEREF__ + "Save Label..." + ", ingestionRootKey: " + to_string(ingestionRootKey) + ", groupOfTaskLabel: " + groupOfTaskLabel +
		", localDependOnIngestionJobKeyExecution: " + to_string(localDependOnIngestionJobKeyExecution)
	);
	if (groupOfTaskLabel != "")
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
		conn, trans, userKey, apiKey, workspace, ingestionRootKey, groupOfTasksRoot, localDependOnIngestionJobKeysForStarting,
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
	shared_ptr<PostgresConnection> conn, work &trans, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
	json &taskOrGroupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
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
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onSuccessRoot[field];

		string field = "type";
		if (!JSONUtils::isMetadataPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

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
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
						conn, trans, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber,
						referenceOutputDependency
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
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onErrorRoot[field];

		string field = "type";
		if (!JSONUtils::isMetadataPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

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
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
						conn, trans, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber,
						referenceOutputDependency
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
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		json &taskRoot = onCompleteRoot[field];

		string field = "type";
		if (!JSONUtils::isMetadataPresent(taskRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string taskType = JSONUtils::asString(taskRoot, field, "");

		vector<int64_t> localIngestionJobKeys;
		if (taskType == "GroupOfTasks")
		{
			int localDependOnSuccess = -1;
#ifdef __POSTGRES__
			localIngestionJobKeys = ingestionGroupOfTasks(
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
				conn, trans, userKey, apiKey, workspace, ingestionRootKey, taskRoot, dependOnIngestionJobKeysForStarting, localDependOnSuccess,
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
						conn, trans, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber,
						referenceOutputDependency
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
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestMethod,
	unordered_map<string, string> queryParameters, shared_ptr<Workspace> workspace,
	// unsigned long contentLength,
	unordered_map<string, string> &requestDetails
)
{
	string api = "uploadedBinary";

	// char* buffer = nullptr;

	try
	{
		if (_noFileSystemAccess)
		{
			string errorMessage = string("no rights to execute this method") + ", _noFileSystemAccess: " + to_string(_noFileSystemAccess);
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("'ingestionJobKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

		auto binaryPathFileIt = requestDetails.find("HTTP_X_FILE");
		if (binaryPathFileIt == requestDetails.end())
		{
			string errorMessage = string("'HTTP_X_FILE' item is missing");
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		// sourceBinaryPathFile will be something like:
		// /var/catramms/storage/nginxWorkingAreaRepository/0000001023
		string sourceBinaryPathFile = binaryPathFileIt->second;

		// Content-Range: bytes 0-99999/100000
		bool contentRangePresent = false;
		long long contentRangeStart = -1;
		long long contentRangeEnd = -1;
		long long contentRangeSize = -1;
		double uploadingProgress = 0.0;
		auto contentRangeIt = requestDetails.find("HTTP_CONTENT_RANGE");
		if (contentRangeIt != requestDetails.end())
		{
			string contentRange = contentRangeIt->second;
			try
			{
				parseContentRange(contentRange, contentRangeStart, contentRangeEnd, contentRangeSize);

				// X : 100 = contentRangeEnd : contentRangeSize
				uploadingProgress = 100 * contentRangeEnd / contentRangeSize;

				contentRangePresent = true;
			}
			catch (exception &e)
			{
				string errorMessage = string("Content-Range is not well done. Expected format: "
											 "'Content-Range: bytes <start>-<end>/<size>'") +
									  ", contentRange: " + contentRange;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
		string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
		bool segmentedContent = false;
		try
		{
			tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
				_mmsEngineDBFacade->getIngestionJobDetails(
					workspace->_workspaceKey, ingestionJobKey,
					// 2022-12-18: l'ingestionJob potrebbe essere stato
					// appena aggiunto
					true
				);

			string parameters;
			tie(ignore, ignore, ignore, parameters, ignore) = ingestionJobDetails;

			json parametersRoot = JSONUtils::toJson(parameters);

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
		catch (runtime_error &e)
		{
			string errorMessage = string("mmsEngineDBFacade->getIngestionJobDetails failed") +
								  ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
								  ", destBinaryPathName: " + destBinaryPathName + ", e.what: " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("mmsEngineDBFacade->getIngestionJobDetails failed") +
								  ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
								  ", destBinaryPathName: " + destBinaryPathName;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		if (segmentedContent)
			destBinaryPathName = destBinaryPathName + ".tar.gz";

		if (!contentRangePresent)
		{
			try
			{
				_logger->info(
					__FILEREF__ + "Moving file from nginx area to ingestion user area" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", sourceBinaryPathFile: " + sourceBinaryPathFile + ", destBinaryPathName: " + destBinaryPathName
				);

				MMSStorage::move(ingestionJobKey, sourceBinaryPathFile, destBinaryPathName, _logger);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string("Error to move file") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", sourceBinaryPathFile: " + sourceBinaryPathFile + ", destBinaryPathName: " + destBinaryPathName +
									  ", e.what: " + e.what();
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			catch (exception &e)
			{
				string errorMessage = string("Error to move file") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", sourceBinaryPathFile: " + sourceBinaryPathFile + ", destBinaryPathName: " + destBinaryPathName;
				_logger->error(__FILEREF__ + errorMessage);

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
							_logger->error(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
					}
			}
			*/

			bool sourceBinaryTransferred = true;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
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

				unsigned long destBinaryPathNameSizeInBytes = fs::file_size(destBinaryPathName);
				unsigned long sourceBinaryPathFileSizeInBytes = fs::file_size(sourceBinaryPathFile);

				_logger->info(
					__FILEREF__ + "Content-Range before concat" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", contentRangeStart: " + to_string(contentRangeStart) + ", contentRangeEnd: " + to_string(contentRangeEnd) +
					", contentRangeSize: " + to_string(contentRangeSize) + ", segmentedContent: " + to_string(segmentedContent) +
					", destBinaryPathName: " + destBinaryPathName + ", destBinaryPathNameSizeInBytes: " + to_string(destBinaryPathNameSizeInBytes) +
					", sourceBinaryPathFile: " + sourceBinaryPathFile +
					", sourceBinaryPathFileSizeInBytes: " + to_string(sourceBinaryPathFileSizeInBytes)
				);

				// waiting in case of nfs delay
				chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
				while (contentRangeStart != destBinaryPathNameSizeInBytes && chrono::system_clock::now() < end)
				{
					this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));

					destBinaryPathNameSizeInBytes = fs::file_size(destBinaryPathName);
				}

				if (contentRangeStart != destBinaryPathNameSizeInBytes)
				{
					string errorMessage = string("Content-Range. This is NOT the next expected "
												 "chunk because Content-Range start is different "
												 "from fileSizeInBytes") +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", contentRangeStart: " + to_string(contentRangeStart) +
										  ", contentRangeEnd: " + to_string(contentRangeEnd) + ", contentRangeSize: " + to_string(contentRangeSize) +
										  ", segmentedContent: " + to_string(segmentedContent) + ", destBinaryPathName: " + destBinaryPathName +
										  ", sourceBinaryPathFile: " + sourceBinaryPathFile +
										  ", sourceBinaryPathFileSizeInBytes: " + to_string(sourceBinaryPathFileSizeInBytes) +
										  ", destBinaryPathNameSizeInBytes (expected): " + to_string(destBinaryPathNameSizeInBytes);
					_logger->error(__FILEREF__ + errorMessage);

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

					_logger->info(
						__FILEREF__ + "Content-Range after concat" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", contentRangeStart: " + to_string(contentRangeStart) + ", contentRangeEnd: " + to_string(contentRangeEnd) +
						", contentRangeSize: " + to_string(contentRangeSize) + ", segmentedContent: " + to_string(segmentedContent) +
						", destBinaryPathName: " + destBinaryPathName + ", destBinaryPathNameSizeInBytes: " +
						to_string(fs::file_size(destBinaryPathName)) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
						", sourceBinaryPathFileSizeInBytes: " + to_string(sourceBinaryPathFileSizeInBytes) +
						", concat elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - start).count())
					);

					fs::remove_all(sourceBinaryPathFile);
				}
				catch (exception &e)
				{
					string errorMessage = string("Content-Range. Error to concat file") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", destBinaryPathName: " + destBinaryPathName + ", sourceBinaryPathFile: " + sourceBinaryPathFile;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				// binary file does not exist, so this is the first chunk

				if (contentRangeStart != 0)
				{
					string errorMessage = string("Content-Range. This is the first chunk of the "
												 "file and Content-Range start has to be 0") +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", contentRangeStart: " + to_string(contentRangeStart);
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}

				try
				{
					_logger->info(
						__FILEREF__ +
						"Content-Range. Moving file from nginx area to "
						"ingestion user area" +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
						", destBinaryPathName: " + destBinaryPathName
					);

					MMSStorage::move(ingestionJobKey, sourceBinaryPathFile, destBinaryPathName, _logger);
				}
				catch (exception &e)
				{
					string errorMessage = string("Content-Range. Error to move file") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", sourceBinaryPathFile: " + sourceBinaryPathFile + ", destBinaryPathName: " + destBinaryPathName;
					_logger->error(__FILEREF__ + errorMessage);

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
								_logger->error(__FILEREF__ + errorMessage);

								throw runtime_error(errorMessage);
						}
				}
				*/

				bool sourceBinaryTransferred = true;
				_logger->info(
					__FILEREF__ + "Content-Range. Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
				);
				_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, sourceBinaryTransferred);
			}
			else
			{
				_logger->info(
					__FILEREF__ + "Content-Range. Update IngestionJob (uploading progress)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", uploadingProgress: " + to_string(uploadingProgress)
				);
				_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(ingestionJobKey, uploadingProgress);
			}
		}

		string responseBody;
		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::stopUploadFileProgressThread()
{
	_fileUploadProgressThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
}

void API::fileUploadProgressCheck()
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
				_logger->error(
					__FILEREF__ +
					"fileUploadProgressCheck: remove entry because of too many "
					"call failures" +
					", ingestionJobKey: " + to_string(itr->_ingestionJobKey) + ", progressId: " + itr->_progressId +
					", binaryVirtualHostName: " + itr->_binaryVirtualHostName + ", binaryListenHost: " + itr->_binaryListenHost +
					", callFailures: " + to_string(itr->_callFailures) + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
				);
				itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

				continue;
			}

			try
			{
				string progressURL = string("http://:") + itr->_binaryListenHost + ":" + to_string(_webServerPort) + _progressURI;
				string progressIdHeader = string("X-Progress-ID: ") + itr->_progressId;
				string hostHeader = string("Host: ") + itr->_binaryVirtualHostName;

				_logger->info(
					__FILEREF__ + "Call for upload progress" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
					", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
					", binaryListenHost: " + itr->_binaryListenHost + ", callFailures: " + to_string(itr->_callFailures) +
					", progressURL: " + progressURL + ", progressIdHeader: " + progressIdHeader + ", hostHeader: " + hostHeader
				);

				vector<string> otherHeaders;
				otherHeaders.push_back(progressIdHeader);
				otherHeaders.push_back(hostHeader); // important for the nginx virtual host
				int curlTimeoutInSeconds = 120;
				json uploadProgressResponse =
					MMSCURL::httpGetJson(_logger, itr->_ingestionJobKey, progressURL, curlTimeoutInSeconds, "", "", otherHeaders);

				/*
curlpp::Cleanup cleaner;
curlpp::Easy request;
ostringstream response;

list<string> header;
header.push_back(progressIdHeader);
header.push_back(hostHeader);	// important for the nginx virtual host

// Setting the URL to retrive.
request.setOpt(new curlpp::options::Url(progressURL));

				int curlTimeoutInSeconds = 120;
				request.setOpt(new
curlpp::options::Timeout(curlTimeoutInSeconds));

request.setOpt(new curlpp::options::HttpHeader(header));
request.setOpt(new curlpp::options::WriteStream(&response));
request.perform();

string sResponse = response.str();

// LF and CR create problems to the json parser...
while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() ==
13)) sResponse.pop_back();

_logger->info(__FILEREF__ + "Call for upload progress response"
	+ ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
	+ ", progressId: " + itr->_progressId
	+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
	+ ", binaryListenHost: " + itr->_binaryListenHost
	+ ", callFailures: " + to_string(itr->_callFailures)
	+ ", sResponse: " + sResponse
);
				*/

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
							absoluteProgress = ((double)absoluteReceived / (double)absoluteSize) * 100;

						// this is to have one decimal in the percentage
						double absoluteUploadingPercentage;
						if (itr->_contentRangePresent)
							absoluteUploadingPercentage = ((double)((int)(absoluteProgress * 10))) / 10;

						if (itr->_contentRangePresent)
						{
							_logger->info(
								__FILEREF__ + "Upload just finished" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
								", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
								", binaryListenHost: " + itr->_binaryListenHost + ", relativeProgress: " + to_string(relativeProgress) +
								", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage) + ", absoluteProgress: " +
								to_string(absoluteProgress) + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage) +
								", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
							);
						}
						else
						{
							_logger->info(
								__FILEREF__ + "Upload just finished" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
								", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
								", binaryListenHost: " + itr->_binaryListenHost + ", relativeProgress: " + to_string(relativeProgress) +
								", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage) +
								", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
							);
						}

						if (itr->_contentRangePresent)
						{
							_logger->info(
								__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
								", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
								", binaryListenHost: " + itr->_binaryListenHost +
								", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
							);
							_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, absoluteUploadingPercentage);
						}
						else
						{
							_logger->info(
								__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
								", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
								", binaryListenHost: " + itr->_binaryListenHost +
								", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
							);
							_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, relativeUploadingPercentage);
						}

						itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

						iteratorAlreadyUpdated = true;
					}
					else if (state == "error")
					{
						_logger->error(
							__FILEREF__ +
							"fileUploadProgressCheck: remove entry because "
							"state is 'error'" +
							", ingestionJobKey: " + to_string(itr->_ingestionJobKey) + ", progressId: " + itr->_progressId +
							", binaryVirtualHostName: " + itr->_binaryVirtualHostName + ", binaryListenHost: " + itr->_binaryListenHost +
							", callFailures: " + to_string(itr->_callFailures) + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
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
							_logger->info(
								__FILEREF__ + "Upload still running" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
								", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
								", binaryListenHost: " + itr->_binaryListenHost + ", relativeProgress: " + to_string(relativeProgress) +
								", absoluteProgress: " + to_string(absoluteProgress) +
								", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated) +
								", relativeReceived: " + to_string(relativeReceived) + ", absoluteReceived: " + to_string(absoluteReceived) +
								", relativeSize: " + to_string(relativeSize) + ", absoluteSize: " + to_string(absoluteSize) +
								", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage) +
								", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
							);
						}
						else
						{
							_logger->info(
								__FILEREF__ + "Upload still running" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
								", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
								", binaryListenHost: " + itr->_binaryListenHost + ", progress: " + to_string(relativeProgress) +
								", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated) + ", received: " + to_string(relativeReceived) +
								", size: " + to_string(relativeSize) + ", uploadingPercentage: " + to_string(relativeUploadingPercentage)
							);
						}

						if (itr->_contentRangePresent)
						{
							if (itr->_lastPercentageUpdated != absoluteUploadingPercentage)
							{
								_logger->info(
									__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
									", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
									", binaryListenHost: " + itr->_binaryListenHost +
									", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
								);
								_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, absoluteUploadingPercentage);

								itr->_lastPercentageUpdated = absoluteUploadingPercentage;
							}
						}
						else
						{
							if (itr->_lastPercentageUpdated != relativeUploadingPercentage)
							{
								_logger->info(
									__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
									", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
									", binaryListenHost: " + itr->_binaryListenHost +
									", uploadingPercentage: " + to_string(relativeUploadingPercentage)
								);
								_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, relativeUploadingPercentage);

								itr->_lastPercentageUpdated = relativeUploadingPercentage;
							}
						}
					}
					else
					{
						string errorMessage = string("file upload progress. State is wrong") + ", state: " + state +
											  ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) + ", progressId: " + itr->_progressId +
											  ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
											  ", binaryListenHost: " + itr->_binaryListenHost + ", callFailures: " + to_string(itr->_callFailures) +
											  ", progressURL: " + progressURL + ", progressIdHeader: " + progressIdHeader;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				catch (...)
				{
					string errorMessage = string("response Body json is not well format")
						// + ", sResponse: " + sResponse
						;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch (curlpp::LogicError &e)
			{
				_logger->error(
					__FILEREF__ + "Call for upload progress failed (LogicError)" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
					", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
					", binaryListenHost: " + itr->_binaryListenHost + ", callFailures: " + to_string(itr->_callFailures) + ", exception: " + e.what()
				);

				itr->_callFailures = itr->_callFailures + 1;
			}
			catch (curlpp::RuntimeError &e)
			{
				_logger->error(
					__FILEREF__ + "Call for upload progress failed (RuntimeError)" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
					", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
					", binaryListenHost: " + itr->_binaryListenHost + ", callFailures: " + to_string(itr->_callFailures) + ", exception: " + e.what()
				);

				itr->_callFailures = itr->_callFailures + 1;
			}
			catch (runtime_error e)
			{
				_logger->error(
					__FILEREF__ + "Call for upload progress failed (runtime_error)" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
					", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
					", binaryListenHost: " + itr->_binaryListenHost + ", callFailures: " + to_string(itr->_callFailures) + ", exception: " + e.what()
				);

				itr->_callFailures = itr->_callFailures + 1;
			}
			catch (exception e)
			{
				_logger->error(
					__FILEREF__ + "Call for upload progress failed (exception)" + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey) +
					", progressId: " + itr->_progressId + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName +
					", binaryListenHost: " + itr->_binaryListenHost + ", callFailures: " + to_string(itr->_callFailures) + ", exception: " + e.what()
				);

				itr->_callFailures = itr->_callFailures + 1;
			}

			if (!iteratorAlreadyUpdated)
				itr++;
		}
	}
}

void API::ingestionRootsStatus(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "ingestionRootsStatus";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int64_t ingestionRootKey = -1;
		auto ingestionRootKeyIt = queryParameters.find("ingestionRootKey");
		if (ingestionRootKeyIt != queryParameters.end() && ingestionRootKeyIt->second != "")
		{
			ingestionRootKey = stoll(ingestionRootKeyIt->second);
		}

		int64_t mediaItemKey = -1;
		auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
		if (mediaItemKeyIt != queryParameters.end() && mediaItemKeyIt->second != "")
		{
			mediaItemKey = stoll(mediaItemKeyIt->second);
		}

		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 10;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
		{
			rows = stoll(rowsIt->second);
			if (rows > _maxPageSize)
			{
				// 2022-02-13: changed to return an error otherwise the user
				//	think to ask for a huge number of items while the return
				// is much less

				// rows = _maxPageSize;

				string errorMessage =
					__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string startIngestionDate;
		auto startIngestionDateIt = queryParameters.find("startIngestionDate");
		if (startIngestionDateIt != queryParameters.end())
			startIngestionDate = startIngestionDateIt->second;

		string endIngestionDate;
		auto endIngestionDateIt = queryParameters.find("endIngestionDate");
		if (endIngestionDateIt != queryParameters.end())
			endIngestionDate = endIngestionDateIt->second;

		string label;
		auto labelIt = queryParameters.find("label");
		if (labelIt != queryParameters.end() && labelIt->second != "")
		{
			label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then
			// apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string),
			// and we do the replace 	after curlpp::unescape, this char will be
			// changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = curlpp::unescape(firstDecoding);
		}

		string status = "all";
		auto statusIt = queryParameters.find("status");
		if (statusIt != queryParameters.end() && statusIt->second != "")
		{
			status = statusIt->second;
		}

		bool asc = true;
		auto ascIt = queryParameters.find("asc");
		if (ascIt != queryParameters.end() && ascIt->second != "")
		{
			if (ascIt->second == "true")
				asc = true;
			else
				asc = false;
		}

		bool ingestionJobOutputs = true;
		auto ingestionJobOutputsIt = queryParameters.find("ingestionJobOutputs");
		if (ingestionJobOutputsIt != queryParameters.end() && ingestionJobOutputsIt->second != "")
		{
			if (ingestionJobOutputsIt->second == "true")
				ingestionJobOutputs = true;
			else
				ingestionJobOutputs = false;
		}

		bool dependencyInfo = true;
		auto dependencyInfoIt = queryParameters.find("dependencyInfo");
		if (dependencyInfoIt != queryParameters.end() && dependencyInfoIt->second != "")
		{
			if (dependencyInfoIt->second == "true")
				dependencyInfo = true;
			else
				dependencyInfo = false;
		}

		{
			json ingestionStatusRoot = _mmsEngineDBFacade->getIngestionRootsStatus(
				workspace, ingestionRootKey, mediaItemKey, start, rows,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate, label, status, asc, dependencyInfo, ingestionJobOutputs,
				// 2022-12-18: IngestionRoot dovrebbe essere stato aggiunto
				// da tempo
				false
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::ingestionRootMetaDataContent(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "ingestionRootMetaDataContent";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int64_t ingestionRootKey = -1;
		auto ingestionRootKeyIt = queryParameters.find("ingestionRootKey");
		if (ingestionRootKeyIt == queryParameters.end() || ingestionRootKeyIt->second == "")
		{
			string errorMessage = string("The 'ingestionRootKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		ingestionRootKey = stoll(ingestionRootKeyIt->second);

		bool processedMetadata = false;
		auto processedMetadataIt = queryParameters.find("processedMetadata");
		if (processedMetadataIt != queryParameters.end() && processedMetadataIt->second != "")
			processedMetadata = (processedMetadataIt->second == "true" ? true : false);

		{
			string ingestionRootMetaDataContent = _mmsEngineDBFacade->getIngestionRootMetaDataContent(
				workspace, ingestionRootKey, processedMetadata,
				// 2022-12-18: IngestionJobKey dovrebbe essere stato
				// aggiunto da tempo
				false
			);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, ingestionRootMetaDataContent);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::ingestionJobsStatus(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "ingestionJobsStatus";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int64_t ingestionJobKey = -1;
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt != queryParameters.end() && ingestionJobKeyIt->second != "")
		{
			ingestionJobKey = stoll(ingestionJobKeyIt->second);
		}

		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 10;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
		{
			rows = stoll(rowsIt->second);
			if (rows > _maxPageSize)
			{
				// 2022-02-13: changed to return an error otherwise the user
				//	think to ask for a huge number of items while the return
				// is much less

				// rows = _maxPageSize;

				string errorMessage =
					__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string label;
		auto labelIt = queryParameters.find("label");
		if (labelIt != queryParameters.end() && labelIt->second != "")
		{
			label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then
			// apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string),
			// and we do the replace 	after curlpp::unescape, this char will be
			// changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = curlpp::unescape(firstDecoding);
		}

		bool labelLike = true;
		auto labelLikeIt = queryParameters.find("labelLike");
		if (labelLikeIt != queryParameters.end() && labelLikeIt->second != "")
		{
			labelLike = (labelLikeIt->second == "true" ? true : false);
		}

		string startIngestionDate;
		auto startIngestionDateIt = queryParameters.find("startIngestionDate");
		if (startIngestionDateIt != queryParameters.end())
			startIngestionDate = startIngestionDateIt->second;

		string endIngestionDate;
		auto endIngestionDateIt = queryParameters.find("endIngestionDate");
		if (endIngestionDateIt != queryParameters.end())
			endIngestionDate = endIngestionDateIt->second;

		string startScheduleDate;
		auto startScheduleDateIt = queryParameters.find("startScheduleDate");
		if (startScheduleDateIt != queryParameters.end())
			startScheduleDate = startScheduleDateIt->second;

		string ingestionType;
		auto ingestionTypeIt = queryParameters.find("ingestionType");
		if (ingestionTypeIt != queryParameters.end() && ingestionTypeIt->second != "")
		{
			ingestionType = ingestionTypeIt->second;
		}

		bool asc = true;
		auto ascIt = queryParameters.find("asc");
		if (ascIt != queryParameters.end() && ascIt->second != "")
		{
			if (ascIt->second == "true")
				asc = true;
			else
				asc = false;
		}

		bool ingestionJobOutputs = true;
		auto ingestionJobOutputsIt = queryParameters.find("ingestionJobOutputs");
		if (ingestionJobOutputsIt != queryParameters.end() && ingestionJobOutputsIt->second != "")
		{
			if (ingestionJobOutputsIt->second == "true")
				ingestionJobOutputs = true;
			else
				ingestionJobOutputs = false;
		}

		bool dependencyInfo = true;
		auto dependencyInfoIt = queryParameters.find("dependencyInfo");
		if (dependencyInfoIt != queryParameters.end() && dependencyInfoIt->second != "")
		{
			if (dependencyInfoIt->second == "true")
				dependencyInfo = true;
			else
				dependencyInfo = false;
		}

		// used in case of live-proxy
		string configurationLabel;
		auto configurationLabelIt = queryParameters.find("configurationLabel");
		if (configurationLabelIt != queryParameters.end() && configurationLabelIt->second != "")
		{
			configurationLabel = configurationLabelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then
			// apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string),
			// and we do the replace 	after curlpp::unescape, this char will be
			// changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(configurationLabel, regex(plus), plusDecoded);

			configurationLabel = curlpp::unescape(firstDecoding);
		}

		// used in case of live-grid
		string outputChannelLabel;
		auto outputChannelLabelIt = queryParameters.find("outputChannelLabel");
		if (outputChannelLabelIt != queryParameters.end() && outputChannelLabelIt->second != "")
		{
			outputChannelLabel = outputChannelLabelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then
			// apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string),
			// and we do the replace 	after curlpp::unescape, this char will be
			// changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(outputChannelLabel, regex(plus), plusDecoded);

			outputChannelLabel = curlpp::unescape(firstDecoding);
		}

		// used in case of live-recorder
		int64_t recordingCode = -1;
		auto recordingCodeIt = queryParameters.find("recordingCode");
		if (recordingCodeIt != queryParameters.end() && recordingCodeIt->second != "")
		{
			recordingCode = stoll(recordingCodeIt->second);
		}

		// used in case of broadcaster
		bool broadcastIngestionJobKeyNotNull = false;
		auto broadcastIngestionJobKeyNotNullIt = queryParameters.find("broadcastIngestionJobKeyNotNull");
		if (broadcastIngestionJobKeyNotNullIt != queryParameters.end() && broadcastIngestionJobKeyNotNullIt->second != "")
		{
			if (broadcastIngestionJobKeyNotNullIt->second == "true")
				broadcastIngestionJobKeyNotNull = true;
			else
				broadcastIngestionJobKeyNotNull = false;
		}

		string jsonParametersCondition;
		auto jsonParametersConditionIt = queryParameters.find("jsonParametersCondition");
		if (jsonParametersConditionIt != queryParameters.end() && jsonParametersConditionIt->second != "")
		{
			jsonParametersCondition = jsonParametersConditionIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then
			// apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string),
			// and we do the replace 	after curlpp::unescape, this char will be
			// changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(jsonParametersCondition, regex(plus), plusDecoded);

			jsonParametersCondition = curlpp::unescape(firstDecoding);
		}

		string status = "all";
		auto statusIt = queryParameters.find("status");
		if (statusIt != queryParameters.end() && statusIt->second != "")
		{
			status = statusIt->second;
		}

		bool fromMaster = false;
		auto fromMasterIt = queryParameters.find("fromMaster");
		if (fromMasterIt != queryParameters.end() && fromMasterIt->second != "")
		{
			if (fromMasterIt->second == "true")
				fromMaster = true;
			else
				fromMaster = false;
		}

		{
			json ingestionStatusRoot = _mmsEngineDBFacade->getIngestionJobsStatus(
				workspace, ingestionJobKey, start, rows, label, labelLike,
				/* startAndEndIngestionDatePresent, */ startIngestionDate, endIngestionDate, startScheduleDate, ingestionType, configurationLabel,
				outputChannelLabel, recordingCode, broadcastIngestionJobKeyNotNull, jsonParametersCondition, asc, status, dependencyInfo,
				ingestionJobOutputs, fromMaster
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::cancelIngestionJob(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "API::cancelIngestionJob";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int64_t ingestionJobKey = -1;
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end() || ingestionJobKeyIt->second == "")
		{
			string errorMessage = string("The 'ingestionJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		ingestionJobKey = stoll(ingestionJobKeyIt->second);

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
		bool forceCancel = false;
		auto forceCancelIt = queryParameters.find("forceCancel");
		if (forceCancelIt != queryParameters.end() && forceCancelIt->second != "")
		{
			if (forceCancelIt->second == "true")
				forceCancel = true;
			else
				forceCancel = false;
		}

		MMSEngineDBFacade::IngestionStatus ingestionStatus;

		tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
			_mmsEngineDBFacade->getIngestionJobDetails(
				workspace->_workspaceKey, ingestionJobKey,
				// 2022-12-18: meglio avere una info sicura
				true
			);
		tie(ignore, ignore, ingestionStatus, ignore, ignore) = ingestionJobDetails;

		if (!forceCancel && ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
		{
			string errorMessage = string("The IngestionJob cannot be removed because of his Status") +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus);
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		_logger->info(
			__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_CanceledByUser" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_CanceledByUser, "");

		if (forceCancel)
			_mmsEngineDBFacade->forceCancelEncodingJob(ingestionJobKey);

		string responseBody;
		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::updateIngestionJob(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace, int64_t userKey,
	unordered_map<string, string> queryParameters, string requestBody, bool admin
)
{
	string api = "updateIngestionJob";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int64_t ingestionJobKey = -1;
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end() || ingestionJobKeyIt->second == "")
		{
			string errorMessage = string("'ingestionJobKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		ingestionJobKey = stoll(ingestionJobKeyIt->second);

		try
		{
			_logger->info(
				__FILEREF__ + "getIngestionJobDetails" + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) +
				", ingestionJobKey: " + to_string(ingestionJobKey)
			);

			tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
				_mmsEngineDBFacade->getIngestionJobDetails(
					workspace->_workspaceKey, ingestionJobKey,
					// 2022-12-18: meglio avere una informazione sicura
					true
				);

			string label;
			MMSEngineDBFacade::IngestionType ingestionType;
			MMSEngineDBFacade::IngestionStatus ingestionStatus;
			string metaDataContent;

			tie(label, ingestionType, ingestionStatus, metaDataContent, ignore) = ingestionJobDetails;

			if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
			{
				string errorMessage = string("It is not possible to update an IngestionJob that "
											 "it is not in Start_TaskQueued status") +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			json metadataRoot = JSONUtils::toJson(requestBody);

			string field = "IngestionType";
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = string("IngestionType field is missing") + ", ingestionJobKey: " + to_string(ingestionJobKey);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string sIngestionType = JSONUtils::asString(metadataRoot, "IngestionType", "");

			if (sIngestionType == MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionType::LiveRecorder))
			{
				if (ingestionType != MMSEngineDBFacade::IngestionType::LiveRecorder)
				{
					string errorMessage = string("It was requested an Update of Live-Recorder "
												 "but IngestionType is not a LiveRecorder") +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
					_logger->error(__FILEREF__ + errorMessage);

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
						DateTime::sDateSecondsToUtc(newRecordingPeriodStart);
					}

					if (recordingPeriodEndModified)
					{
						// Validator validator(_logger, _mmsEngineDBFacade,
						// _configuration);
						DateTime::sDateSecondsToUtc(newRecordingPeriodEnd);
					}

					_logger->info(
						__FILEREF__ + "Update IngestionJob" + ", workspaceKey: " + to_string(workspace->_workspaceKey) +
						", ingestionJobKey: " + to_string(ingestionJobKey)
					);

					_mmsEngineDBFacade->updateIngestionJob_LiveRecorder(
						workspace->_workspaceKey, ingestionJobKey, ingestionJobLabelModified, newIngestionJobLabel, channelLabelModified,
						newChannelLabel, recordingPeriodStartModified, newRecordingPeriodStart, recordingPeriodEndModified, newRecordingPeriodEnd,
						recordingVirtualVODModified, newRecordingVirtualVOD, admin
					);

					_logger->info(
						__FILEREF__ + "IngestionJob updated" + ", workspaceKey: " + to_string(workspace->_workspaceKey) +
						", ingestionJobKey: " + to_string(ingestionJobKey)
					);
				}
			}

			json responseRoot;
			responseRoot["status"] = string("success");

			string responseBody = JSONUtils::toString(responseRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error: ") + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::changeLiveProxyPlaylist(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "changeLiveProxyPlaylist";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end() || ingestionJobKeyIt->second == "")
		{
			string errorMessage = string("'ingestionJobKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t broadcasterIngestionJobKey = stoll(ingestionJobKeyIt->second);

		bool interruptPlaylist = false;
		auto interruptPlaylistIt = queryParameters.find("interruptPlaylist");
		if (interruptPlaylistIt != queryParameters.end())
			interruptPlaylist = interruptPlaylistIt->second == "true";

		SPDLOG_INFO(
			"{}, broadcasterIngestionJobKey: {}"
			", interruptPlaylist: {}",
			api, broadcasterIngestionJobKey, interruptPlaylist
		);

		// next try/catch initialize the belows parameters using the broadcaster
		// info

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
			_logger->info(
				__FILEREF__ + "getIngestionJobDetails" + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) +
				", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey)
			);

			tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
				_mmsEngineDBFacade->getIngestionJobDetails(
					workspace->_workspaceKey, broadcasterIngestionJobKey,
					// 2022-12-18: meglio avere una informazione sicura
					true
				);

			MMSEngineDBFacade::IngestionType ingestionType;
			MMSEngineDBFacade::IngestionStatus ingestionStatus;
			string metaDataContent;

			tie(ignore, ingestionType, ingestionStatus, metaDataContent, ignore) = ingestionJobDetails;

			if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy)
			{
				string errorMessage = string("Ingestion type is not a Live/VODProxy") +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
									  ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
			string prefixIngestionStatus = "End_";
			if (sIngestionStatus.size() >= prefixIngestionStatus.size() &&
				0 == sIngestionStatus.compare(0, prefixIngestionStatus.size(), prefixIngestionStatus))
			{
				string errorMessage = string("Ingestion job is already finished") +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
									  ", sIngestionStatus: " + sIngestionStatus + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			json metadataContentRoot = JSONUtils::toJson(metaDataContent);

			string field = "internalMMS";
			if (!JSONUtils::isMetadataPresent(metadataContentRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			json internalMMSRoot = metadataContentRoot[field];

			field = "broadcaster";
			if (!JSONUtils::isMetadataPresent(internalMMSRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			json broadcasterRoot = internalMMSRoot[field];

			field = "broadcastIngestionJobKey";
			broadcastIngestionJobKey = JSONUtils::asInt64(broadcasterRoot, field, 0);
			if (broadcastIngestionJobKey == 0)
			{
				string errorMessage = __FILEREF__ + "No broadcastIngestionJobKey found" +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
									  ", broadcastIngestionJobKey: " + to_string(broadcastIngestionJobKey);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "schedule";
			if (!JSONUtils::isMetadataPresent(metadataContentRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			json proxyPeriodRoot = metadataContentRoot[field];

			field = "timePeriod";
			bool timePeriod = JSONUtils::asBool(metadataContentRoot, field, false);
			if (!timePeriod)
			{
				string errorMessage = __FILEREF__ + "The LiveProxy IngestionJob has to have timePeriod" +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
									  ", timePeriod: " + to_string(timePeriod);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "start";
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcBroadcasterStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);

			field = "end";
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcBroadcasterEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);

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
							workspace, broadcasterIngestionJobKey, broadcastDefaultConfigurationLabel, "",
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

							for (int referencePhysicalPathKeyIndex = 0; referencePhysicalPathKeyIndex < referencePhysicalPathKeysRoot.size();
								 referencePhysicalPathKeyIndex++)
							{
								json referencePhysicalPathKeyRoot = referencePhysicalPathKeysRoot[referencePhysicalPathKeyIndex];

								int64_t broadcastDefaultPhysicalPathKey = JSONUtils::asInt64(referencePhysicalPathKeyRoot, "physicalPathKey", -1);
								string broadcastDefaultTitle = JSONUtils::asString(referencePhysicalPathKeyRoot, "mediaItemTitle", "");

								string sourcePhysicalPathName;
								{
									tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
										broadcastDefaultPhysicalPathKey,
										// 2022-12-18: MIK dovrebbe
										// essere stato aggiunto da
										// tempo
										false
									);
									tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

									bool warningIfMissing = false;
									tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
										mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
											workspace->_workspaceKey, broadcastDefaultPhysicalPathKey, warningIfMissing,
											// 2022-12-18: MIK dovrebbe
											// essere stato aggiunto da
											// tempo
											false
										);
									tie(ignore, vodContentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemKeyDetails;
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

									pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
										-1, // userKey,
										workspace,
										"", // clientIPAddress,

										-1, // mediaItemKey,
										"", // uniqueName,
										-1, // encodingProfileKey,
										"", // encodingProfileLabel,

										broadcastDefaultPhysicalPathKey,

										-1, // ingestionJobKey,	(in case
											// of live)
										-1, // deliveryCode,

										abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
										999999,							 // maxRetries,
										false,							 // save,
										"MMS_SignedToken",				 // deliveryType,

										false, // warningIfMissingMediaItemKey,
										true,  // filteredByStatistic
										""	   // userId (it is not needed
											   // it filteredByStatistic is
											   // true
									);

									tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
								}

								sources.push_back(make_tuple(
									broadcastDefaultPhysicalPathKey, broadcastDefaultTitle, sourcePhysicalPathName, sourcePhysicalDeliveryURL
								));
							}
						}

						field = "filters";
						json filtersRoot;
						if (JSONUtils::isMetadataPresent(broadcastDefaultPlaylistItemRoot, field))
							filtersRoot = broadcastDefaultPlaylistItemRoot[field];

						// the same json structure is used in
						// MMSEngineProcessor::manageVODProxy
						broadcastDefaultVodInputRoot = _mmsEngineDBFacade->getVodInputRoot(vodContentType, sources, filtersRoot);
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
									workspace,
									"", // clientIPAddress,

									-1, // mediaItemKey,
									"", // uniqueName,
									-1, // encodingProfileKey,
									"", // encodingProfileLabel,

									broadcastDefaultPhysicalPathKey,

									-1, // ingestionJobKey,	(in case
										// of live)
									-1, // deliveryCode,

									abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
									999999,							 // maxRetries,
									false,							 // save,
									"MMS_SignedToken",				 // deliveryType,

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
							string errorMessage = __FILEREF__ + "Countdown has to have the drawText filter" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
												  ", broadcastDefaultPlaylistItemRoot: " + JSONUtils::toString(broadcastDefaultPlaylistItemRoot);
							_logger->error(errorMessage);

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
						json filtersRoot;
						if (JSONUtils::isMetadataPresent(broadcastDefaultPlaylistItemRoot, field))
							filtersRoot = broadcastDefaultPlaylistItemRoot[field];

						broadcastDefaultDirectURLInputRoot = _mmsEngineDBFacade->getDirectURLInputRoot(broadcastDefaultURL, filtersRoot);
					}
					else
					{
						string errorMessage = __FILEREF__ + "Broadcaster data: unknown MediaType" +
											  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
											  ", broadcastDefaultMediaType: " + broadcastDefaultMediaType;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				else
				{
					string errorMessage = __FILEREF__ + "Broadcaster data: no mediaType is present" +
										  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = __FILEREF__ +
									  "Broadcaster data: no broadcastDefaultPlaylistItem is "
									  "present" +
									  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error: ") + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		// check/build the new playlist
		json newPlaylistRoot = json::array();
		try
		{
			json newReceivedPlaylistRoot = JSONUtils::toJson(requestBody);

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
						if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "timePeriod") && newReceivedPlaylistItemRoot["timePeriod"])
						{
							if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "utcScheduleStart"))
								newReceivedPlaylistItemRoot["sUtcScheduleStart"] =
									DateTime::utcToUtcString(newReceivedPlaylistItemRoot["utcScheduleStart"]);
							if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "utcScheduleEnd"))
								newReceivedPlaylistItemRoot["sUtcScheduleEnd"] =
									DateTime::utcToUtcString(newReceivedPlaylistItemRoot["utcScheduleEnd"]);
						}
					}
					{
						if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "streamInput"))
						{
							json streamInputRoot = newReceivedPlaylistItemRoot["streamInput"];

							streamInputRoot["filters"] = getReviewedFiltersRoot(streamInputRoot["filters"], workspace, -1);

							newReceivedPlaylistItemRoot["streamInput"] = streamInputRoot;
						}
						else if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "vodInput"))
						{
							json vodInputRoot = newReceivedPlaylistItemRoot["vodInput"];

							vodInputRoot["filters"] = getReviewedFiltersRoot(vodInputRoot["filters"], workspace, -1);

							// field = "sources";
							if (!JSONUtils::isMetadataPresent(vodInputRoot, "sources"))
							{
								string errorMessage = "sources is missing, json data: " + requestBody;
								_logger->error(__FILEREF__ + errorMessage);

								throw runtime_error(errorMessage);
							}

							json sourcesRoot = vodInputRoot["sources"];

							if (sourcesRoot.size() == 0)
							{
								string errorMessage = string(
									"No source is present"
									", json data: " +
									requestBody
								);
								_logger->error(__FILEREF__ + errorMessage);

								throw runtime_error(errorMessage);
							}

							MMSEngineDBFacade::ContentType vodContentType;

							for (int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
							{
								json sourceRoot = sourcesRoot[sourceIndex];

								// field = "physicalPathKey";
								if (!JSONUtils::isMetadataPresent(sourceRoot, "physicalPathKey"))
								{
									string errorMessage = "physicalPathKey is missing, json data: " + requestBody;
									_logger->error(__FILEREF__ + errorMessage);

									throw runtime_error(errorMessage);
								}
								int64_t physicalPathKey = JSONUtils::asInt64(sourceRoot, "physicalPathKey", -1);

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
											workspace->_workspaceKey, physicalPathKey, warningIfMissing,
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
										workspace,
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
										false,							 // save,
										"MMS_SignedToken",				 // deliveryType,

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

								sourcesRoot[sourceIndex] = sourceRoot;
							}

							// field = "sources";
							vodInputRoot["sources"] = sourcesRoot;

							// field = "vodContentType";
							vodInputRoot["vodContentType"] = MMSEngineDBFacade::toString(vodContentType);

							// field = "vodInput";
							newReceivedPlaylistItemRoot["vodInput"] = vodInputRoot;
						}
						else if (JSONUtils::isMetadataPresent(newReceivedPlaylistItemRoot, "countdownInput"))
						{
							json countdownInputRoot = newReceivedPlaylistItemRoot["countdownInput"];

							countdownInputRoot["filters"] = getReviewedFiltersRoot(countdownInputRoot["filters"], workspace, -1);

							// field = "physicalPathKey";
							if (!JSONUtils::isMetadataPresent(countdownInputRoot, "physicalPathKey"))
							{
								string errorMessage = "physicalPathKey is missing, json data: " + requestBody;
								_logger->error(__FILEREF__ + errorMessage);

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
										workspace->_workspaceKey, physicalPathKey, warningIfMissing,
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

							directURLInputRoot["filters"] = getReviewedFiltersRoot(directURLInputRoot["filters"], workspace, -1);

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

				_logger->info(__FILEREF__ + "Sort playlist items" + ", vNewReceivedPlaylist.size: " + to_string(vNewReceivedPlaylist.size()));
			}
			// 2023-02-26: ora che il vettore è ordinato, elimino gli elementi
			// precedenti a 'now'
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				time_t utcNow = chrono::system_clock::to_time_t(now);

				int currentPlaylistIndex = -1;
				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < vNewReceivedPlaylist.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = vNewReceivedPlaylist[newReceivedPlaylistIndex];

					int64_t utcProxyPeriodStart = JSONUtils::asInt64(newReceivedPlaylistItemRoot, "utcScheduleStart", -1);
					int64_t utcProxyPeriodEnd = JSONUtils::asInt64(newReceivedPlaylistItemRoot, "utcScheduleEnd", -1);

					if (utcProxyPeriodStart <= utcNow && utcNow < utcProxyPeriodEnd)
					{
						currentPlaylistIndex = newReceivedPlaylistIndex;

						break;
					}
				}
				int leavePastEntriesNumber = 3;
				if (currentPlaylistIndex - leavePastEntriesNumber > 0)
				{
					_logger->info(
						__FILEREF__ + "Erase playlist items in the past: " + to_string(currentPlaylistIndex - leavePastEntriesNumber) + " items" +
						", currentPlaylistIndex: " + to_string(currentPlaylistIndex) + ", leavePastEntriesNumber: " +
						to_string(leavePastEntriesNumber) + ", vNewReceivedPlaylist.size: " + to_string(vNewReceivedPlaylist.size())
					);

					vNewReceivedPlaylist.erase(
						vNewReceivedPlaylist.begin(), vNewReceivedPlaylist.begin() + (currentPlaylistIndex - leavePastEntriesNumber)
					);
				}
				else
				{
					_logger->info(
						__FILEREF__ + "Erase playlist items in the past: nothing" + ", currentPlaylistIndex: " + to_string(currentPlaylistIndex) +
						", leavePastEntriesNumber: " + to_string(leavePastEntriesNumber) +
						", vNewReceivedPlaylist.size: " + to_string(vNewReceivedPlaylist.size())
					);
				}
			}

			// build the new playlist
			// add the default media in case of hole filling newPlaylistRoot
			{
				int64_t utcCurrentBroadcasterStart = utcBroadcasterStart;

				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < vNewReceivedPlaylist.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = vNewReceivedPlaylist[newReceivedPlaylistIndex];

					// correct values have to be:
					//	utcCurrentBroadcasterStart <= utcProxyPeriodStart <
					// utcProxyPeriodEnd
					// the last utcProxyPeriodEnd has to be equal to
					// utcBroadcasterEnd
					string field = "utcScheduleStart";
					int64_t utcProxyPeriodStart = JSONUtils::asInt64(newReceivedPlaylistItemRoot, field, -1);
					field = "utcScheduleEnd";
					int64_t utcProxyPeriodEnd = JSONUtils::asInt64(newReceivedPlaylistItemRoot, field, -1);

					_logger->info(
						__FILEREF__ + "Processing newReceivedPlaylistRoot" + ", newReceivedPlaylistRoot: " + to_string(newReceivedPlaylistIndex) +
						"/" + to_string(newReceivedPlaylistRoot.size()) + ", utcCurrentBroadcasterStart: " + to_string(utcCurrentBroadcasterStart) +
						" (" + DateTime::utcToUtcString(utcCurrentBroadcasterStart) + ")" +
						", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) + " (" + DateTime::utcToUtcString(utcProxyPeriodStart) + ")" +
						", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) + " (" + DateTime::utcToUtcString(utcProxyPeriodEnd) + ")"
					);

					if (utcCurrentBroadcasterStart > utcProxyPeriodStart || utcProxyPeriodStart >= utcProxyPeriodEnd ||
						utcProxyPeriodEnd > utcBroadcasterEnd)
					{
						string partialMessage;

						if (utcCurrentBroadcasterStart > utcProxyPeriodStart)
							partialMessage = "utcCurrentBroadcasterStart > "
											 "utcProxyPeriodStart";
						else if (utcProxyPeriodEnd >= utcProxyPeriodStart)
							partialMessage = "utcProxyPeriodEnd >= utcProxyPeriodStart";
						else if (utcProxyPeriodEnd > utcBroadcasterEnd)
							partialMessage = "utcProxyPeriodEnd > utcBroadcasterEnd";

						string errorMessage =
							__FILEREF__ + "Wrong dates (" + partialMessage + ")" +
							", newReceivedPlaylistIndex: " + to_string(newReceivedPlaylistIndex) +
							", utcCurrentBroadcasterStart: " + to_string(utcCurrentBroadcasterStart) + " (" +
							DateTime::utcToUtcString(utcCurrentBroadcasterStart) + ")" + ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart) +
							" (" + DateTime::utcToUtcString(utcProxyPeriodStart) + ")" + ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd) +
							" (" + DateTime::utcToUtcString(utcProxyPeriodEnd) + ")" + ", utcBroadcasterEnd: " + to_string(utcBroadcasterEnd) + " (" +
							DateTime::utcToUtcString(utcBroadcasterEnd) + ")";
						_logger->error(errorMessage);

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
								string errorMessage = __FILEREF__ +
													  "Broadcaster data: no default Stream "
													  "present" +
													  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Media")
						{
							if (broadcastDefaultVodInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
							else
							{
								string errorMessage = __FILEREF__ +
													  "Broadcaster data: no default Media "
													  "present" +
													  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Countdown")
						{
							if (broadcastDefaultCountdownInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
							else
							{
								string errorMessage = __FILEREF__ +
													  "Broadcaster data: no default Countdown "
													  "present" +
													  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Direct URL")
						{
							if (broadcastDefaultDirectURLInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
							else
							{
								string errorMessage = __FILEREF__ +
													  "Broadcaster data: no default DirectURL "
													  "present" +
													  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else
						{
							string errorMessage = __FILEREF__ + "Broadcaster data: unknown MediaType" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
												  ", broadcastDefaultMediaType: " + broadcastDefaultMediaType;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						// update the end time of the last entry with the start
						// time of the entry we are adding
						/* 2021-12-19: non ho capito il controllo sotto!!!
								Tra l'altro newReceivedPlaylistRoot è ora in un
						vector if (newPlaylistRoot.size() > 0)
						{
								json newLastPlaylistRoot =
						newReceivedPlaylistRoot[ newPlaylistRoot.size() - 1];

								field = "utcProxyPeriodEnd";
								newLastPlaylistRoot[field] =
						utcCurrentBroadcasterStart;

								newReceivedPlaylistRoot[newPlaylistRoot.size() -
						1] = newLastPlaylistRoot;
						}
						*/

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
							string errorMessage = __FILEREF__ + "Broadcaster data: no default Stream present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						if (broadcastDefaultVodInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
						else
						{
							string errorMessage = __FILEREF__ + "Broadcaster data: no default Media present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						if (broadcastDefaultCountdownInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
						else
						{
							string errorMessage = __FILEREF__ +
												  "Broadcaster data: no default Countdown "
												  "present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						if (broadcastDefaultDirectURLInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
						else
						{
							string errorMessage = __FILEREF__ +
												  "Broadcaster data: no default Direct URL "
												  "present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						string errorMessage = __FILEREF__ + "Broadcaster data: unknown MediaType" +
											  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
											  ", broadcastDefaultMediaType: " + broadcastDefaultMediaType;
						_logger->error(errorMessage);

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
							string errorMessage = __FILEREF__ + "Broadcaster data: no default Stream present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						if (broadcastDefaultVodInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
						else
						{
							string errorMessage = __FILEREF__ + "Broadcaster data: no default Media present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						if (broadcastDefaultCountdownInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
						else
						{
							string errorMessage = __FILEREF__ +
												  "Broadcaster data: no default Countdown "
												  "present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						if (broadcastDefaultDirectURLInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
						else
						{
							string errorMessage = __FILEREF__ +
												  "Broadcaster data: no default Direct URL "
												  "present" +
												  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						string errorMessage = __FILEREF__ + "Broadcaster data: unknown MediaType" +
											  ", broadcasterIngestionJobKey: " + to_string(broadcasterIngestionJobKey) +
											  ", broadcastDefaultMediaType: " + broadcastDefaultMediaType;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					// update the end time of the last entry with the start time
					// of the entry we are adding
					/* 2021-12-19: non ho capito il controllo sotto
									Tra l'altro newReceivedPlaylistRoot è ora in
					un vector if (newPlaylistRoot.size() > 0)
					{
							json newLastPlaylistRoot =
					newReceivedPlaylistRoot[ newPlaylistRoot.size() - 1];

							field = "utcProxyPeriodEnd";
							newLastPlaylistRoot[field] =
					utcCurrentBroadcasterStart;

							newReceivedPlaylistRoot[newPlaylistRoot.size() - 1]
									= newLastPlaylistRoot;
					}
					*/

					newPlaylistRoot.push_back(newdPlaylistItemToBeAddedRoot);
				}
			}
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error: ") + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

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
			_logger->info(
				__FILEREF__ + "getIngestionJobDetails" + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) +
				", broadcastIngestionJobKey: " + to_string(broadcastIngestionJobKey)
			);

			tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string> ingestionJobDetails =
				_mmsEngineDBFacade->getIngestionJobDetails(
					workspace->_workspaceKey, broadcastIngestionJobKey,
					// 2022-12-18: meglio avere una informazione sicura
					true
				);

			MMSEngineDBFacade::IngestionType ingestionType;
			MMSEngineDBFacade::IngestionStatus ingestionStatus;
			string metaDataContent;

			tie(ignore, ingestionType, ingestionStatus, metaDataContent, ignore) = ingestionJobDetails;

			if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy && ingestionType != MMSEngineDBFacade::IngestionType::VODProxy &&
				ingestionType != MMSEngineDBFacade::IngestionType::Countdown)
			{
				string errorMessage = string("Ingestion type is not a LiveProxy-VODProxy-Countdown") +
									  ", broadcastIngestionJobKey: " + to_string(broadcastIngestionJobKey) +
									  ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			string newPlaylist = JSONUtils::toString(newPlaylistRoot);

			string broadcastParameters;
			int64_t broadcastEncodingJobKey = -1;
			int64_t broadcastEncoderKey = -1;
			try
			{
				tuple<int64_t, int64_t, string> encodingJobDetails = _mmsEngineDBFacade->getEncodingJobDetailsByIngestionJobKey(
					broadcastIngestionJobKey,
					// 2022-12-18: l'IngestionJob potrebbe essere stato
					// appena aggiunto
					true
				);

				tie(broadcastEncodingJobKey, broadcastEncoderKey, broadcastParameters) = encodingJobDetails;
			}
			catch (runtime_error &e)
			{
				_logger->warn(__FILEREF__ + e.what());

				// throw runtime_error(errorMessage);
			}

			// we may have the scenario where the encodingJob is not present
			// because will be executed in the future
			if (broadcastEncodingJobKey != -1)
			{
				json broadcastParametersRoot = JSONUtils::toJson(broadcastParameters);

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
					json encoderResponse = MMSCURL::httpPutStringAndGetJson(
						_logger, broadcastIngestionJobKey, ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, _ffmpegEncoderUser,
						_ffmpegEncoderPassword, newPlaylist,
						"application/json", // contentType
						otherHeaders
					);
				}
			}
			else
			{
				_logger->info(
					__FILEREF__ +
					"The Broadcast EncodingJob was not found, the IngestionJob "
					"is updated" +
					", broadcastIngestionJobKey: " + to_string(broadcastIngestionJobKey) +
					", broadcastEncodingJobKey: " + to_string(broadcastEncodingJobKey)
				);

				json metadataContentRoot = JSONUtils::toJson(metaDataContent);

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
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (runtime_error e)
		{
			SPDLOG_ERROR(
				"{} failed"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				api, ffmpegEncoderURL, response.str(), e.what()
			);

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				api, ffmpegEncoderURL, response.str(), e.what()
			);

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		// sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::changeLiveProxyOverlayText(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "changeLiveProxyOverlayText";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end() || ingestionJobKeyIt->second == "")
		{
			string errorMessage = string("'ingestionJobKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t broadcasterIngestionJobKey = stoll(ingestionJobKeyIt->second);

		SPDLOG_INFO("{}, broadcasterIngestionJobKey: {}", api, broadcasterIngestionJobKey);

		try
		{
			{
				SPDLOG_INFO(
					"ingestionJobQuery"
					", workspace->_workspaceKey: {}"
					", broadcasterIngestionJobKey: {}",
					workspace->_workspaceKey, broadcasterIngestionJobKey
				);

				vector<pair<bool, string>> requestedColumns = {{false, "mms_ingestionjob:ij.ingestionType"}, {false, "mms_ingestionjob:ij.status"}};
				shared_ptr<PostgresHelper::SqlResultSetByIndex> sqlResultSet = make_shared<PostgresHelper::SqlResultSetByIndex>();
				_mmsEngineDBFacade->ingestionJobQuery(sqlResultSet, requestedColumns, workspace->_workspaceKey, broadcasterIngestionJobKey, false);

				auto row = *(sqlResultSet->begin());
				MMSEngineDBFacade::IngestionType ingestionType = MMSEngineDBFacade::toIngestionType(row[0].as<string>("null ingestion type!!!"));
				MMSEngineDBFacade::IngestionStatus ingestionStatus =
					MMSEngineDBFacade::toIngestionStatus(row[1].as<string>("null ingestion status!!!"));

				if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy)
				{
					string errorMessage = fmt::format(
						"Ingestion type is not a Live/VODProxy"
						", broadcasterIngestionJobKey: {}"
						", ingestionType: {}",
						broadcasterIngestionJobKey, MMSEngineDBFacade::toString(ingestionType)
					);
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}

				string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
				string prefixIngestionStatus = "End_";
				if (StringUtils::startWith(sIngestionStatus, "End_"))
				{
					string errorMessage = fmt::format(
						"Ingestion job is already finished"
						", broadcasterIngestionJobKey: {}"
						", sIngestionStatus: {}"
						", ingestionType: {}",
						broadcasterIngestionJobKey, sIngestionStatus, MMSEngineDBFacade::toString(ingestionType)
					);
					_logger->error(__FILEREF__ + errorMessage);

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

				vector<pair<bool, string>> requestedColumns = {{false, "mms_encodingjob:.encodingJobKey"}, {false, "mms_encodingjob:.encoderKey"}};
				shared_ptr<PostgresHelper::SqlResultSetByIndex> sqlResultSet = make_shared<PostgresHelper::SqlResultSetByIndex>();
				_mmsEngineDBFacade->encodingJobQuery(sqlResultSet, requestedColumns, -1, broadcasterIngestionJobKey, false);

				auto row = *(sqlResultSet->begin());
				broadcasterEncodingJobKey = row[0].as<int64_t>(-1);
				broadcasterEncoderKey = row[1].as<int64_t>(-1);

				if (broadcasterEncodingJobKey == -1 || broadcasterEncoderKey == -1)
				{
					string errorMessage = fmt::format(
						"encodingJobKey and/or encoderKey not found"
						", broadcasterEncodingJobKey: {}",
						", broadcasterEncoderKey: {}", ", broadcasterIngestionJobKey: {}", broadcasterEncodingJobKey, broadcasterEncoderKey,
						broadcasterIngestionJobKey
					);
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			{
				string encoderURL;
				tie(encoderURL, ignore) = _mmsEngineDBFacade->getEncoderURL(broadcasterEncoderKey);

				string ffmpegEncoderURL = fmt::format("{}{}/{}", encoderURL, _ffmpegEncoderChangeLiveProxyOverlayTextURI, broadcasterEncodingJobKey);

				vector<string> otherHeaders;
				MMSCURL::httpPutStringAndGetJson(
					_logger, broadcasterIngestionJobKey, ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, _ffmpegEncoderUser, _ffmpegEncoderPassword,
					requestBody,
					"text/plain", // contentType
					otherHeaders
				);
			}
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error: ") + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		string responseBody;
		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		throw e;
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		throw e;
	}
}

// LO STESSO METODO E' IN MMSEngineProcessor.cpp
json API::getReviewedFiltersRoot(json filtersRoot, shared_ptr<Workspace> workspace, int64_t ingestionJobKey)
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
					string errorMessage = fmt::format(
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
						false,				// save,
						"MMS_SignedToken",	// deliveryType,

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
