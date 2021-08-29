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

/*
#include "catralibraries/Convert.h"
#include "catralibraries/LdapWrapper.h"
#include "EMailSender.h"
*/
#include "JSONUtils.h"
#include "catralibraries/DateTime.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "catralibraries/ProcessUtility.h"
#include "Validator.h"
#include "PersistenceLock.h"
#include "API.h"


void API::ingestion(
        FCGX_Request& request,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestion";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();

        Json::Value requestBodyRoot = manageWorkflowVariables(requestBody, Json::nullValue);

        string responseBody;
        shared_ptr<MySQLConnection> conn;
		bool dbTransactionStarted = false;

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

            // used to save <label of the task> ---> vector of ingestionJobKey. A vector is used in case the same label is used more times
            // It is used when ReferenceLabel is used.
            unordered_map<string, vector<int64_t>> mapLabelAndIngestionJobKey;

            conn = _mmsEngineDBFacade->beginIngestionJobs();
			dbTransactionStarted = true;

            Validator validator(_logger, _mmsEngineDBFacade, _configuration);
            // it starts from the root and validate recursively the entire body
            validator.validateIngestedRootMetadata(workspace->_workspaceKey, 
				requestBodyRoot);

            string field = "Type";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            string rootType = requestBodyRoot.get(field, "").asString();

            string rootLabel;
            field = "Label";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
                rootLabel = requestBodyRoot.get(field, "").asString();

            int64_t ingestionRootKey = _mmsEngineDBFacade->addIngestionRoot(conn,
                workspace->_workspaceKey, userKey, rootType, rootLabel, requestBody.c_str());
			field = "ingestionRootKey";
			requestBodyRoot[field] = ingestionRootKey;

            field = "Task";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            Json::Value& taskRoot = requestBodyRoot[field];                        

            field = "Type";
            if (!JSONUtils::isMetadataPresent(taskRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            string taskType = taskRoot.get(field, "").asString();
            
            if (taskType == "GroupOfTasks")
            {
                vector<int64_t> dependOnIngestionJobKeysForStarting;
                
				// 2019-01-01: it is not important since dependOnIngestionJobKey is -1
				// int localDependOnSuccess = 0;
				// 2019-07-24: in case of a group of tasks, as it is, this is important
				//	because, otherwise, in case of a group of tasks as first element of the workflow,
				//	it will not work correctly. I saw this for example in the scenario where, using the player,
				//	we do two cuts. The workflow generated than was: two cuts in parallel and then the concat.
				//	This scenario works if localDependOnSuccess is 1
				int localDependOnSuccess = 1;
                ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysForStarting, localDependOnSuccess,
                        dependOnIngestionJobKeysForStarting,
                        mapLabelAndIngestionJobKey, responseBody); 
            }
            else
            {
                vector<int64_t> dependOnIngestionJobKeysForStarting;
                int localDependOnSuccess = 0;   // it is not important since dependOnIngestionJobKey is -1
                ingestionSingleTask(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysForStarting, localDependOnSuccess,
                        dependOnIngestionJobKeysForStarting, mapLabelAndIngestionJobKey,
                        responseBody);            
            }

			string processedMetadataContent;
			{
				Json::StreamWriterBuilder wbuilder;
				processedMetadataContent = Json::writeString(wbuilder, requestBodyRoot);
			}

			bool commit = true;
			_mmsEngineDBFacade->endIngestionJobs(conn, commit,
				ingestionRootKey, processedMetadataContent);

            string beginOfResponseBody = string("{ ")
                + "\"workflow\": { "
                    + "\"ingestionRootKey\": " + to_string(ingestionRootKey)
                    + ", \"label\": \"" + rootLabel + "\" "
                    + "}, "
                    + "\"tasks\": [ ";
            responseBody.insert(0, beginOfResponseBody);
            responseBody += " ] }";
        }
        catch(AlreadyLocked e)
        {
			if (dbTransactionStarted)
			{
				bool commit = false;
				_mmsEngineDBFacade->endIngestionJobs(conn, commit, -1, string());
			}

            _logger->error(__FILEREF__ + "Ingestion locked"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(runtime_error e)
        {
			if (dbTransactionStarted)
			{
				bool commit = false;
				_mmsEngineDBFacade->endIngestionJobs(conn, commit, -1, string());
			}

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
			if (dbTransactionStarted)
			{
				bool commit = false;
				_mmsEngineDBFacade->endIngestionJobs(conn, commit, -1, string());
			}

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
        _logger->info(__FILEREF__ + "Ingestion"
            + ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
        );
    }
	catch(AlreadyLocked e)
	{
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

Json::Value API::manageWorkflowVariables(string requestBody,
	Json::Value variablesValuesToBeUsedRoot)
{
	Json::Value requestBodyRoot;

	try
	{
		_logger->info(__FILEREF__ + "manageWorkflowVariables"
			+ ", requestBody: " + requestBody
		);

		if (variablesValuesToBeUsedRoot == Json::nullValue)
		{
			_logger->info(__FILEREF__ + "manageWorkflowVariables"
				+ ", sVariablesValuesToBeUsedRoot is null"
			);
		}
		else
		{
			Json::StreamWriterBuilder wbuilder;
			string sVariablesValuesToBeUsedRoot = Json::writeString(wbuilder, variablesValuesToBeUsedRoot);

			_logger->info(__FILEREF__ + "manageWorkflowVariables"
				+ ", sVariablesValuesToBeUsedRoot: " + sVariablesValuesToBeUsedRoot
			);
		}

		{
			Json::CharReaderBuilder builder;
			Json::CharReader* reader = builder.newCharReader();
			string errors;

			bool parsingSuccessful = reader->parse(requestBody.c_str(),
				requestBody.c_str() + requestBody.size(), 
				&requestBodyRoot, &errors);
			delete reader;

			if (!parsingSuccessful)
			{
				string errorMessage = __FILEREF__ + "failed to parse the requestBody"
					+ ", errors: " + errors
					+ ", requestBody: " + requestBody
				;
				_logger->error(errorMessage);

				throw runtime_error(errors);
			}
		}

		/*
		 * Definition of the Variables into the Workflow:
		"Variables": {
			"var n. 1": {
				"Type": "int",	// or string
				"IsNull": false,
				"Value": 10,
				"Description": "..."
			},
			"var n. 2": {
				"Type": "string",
				"IsNull": false,
				"Value": "...",
				"Description": "..."
			}
		}

		Workflow instantiated (example):
			"Task": {
				"Label": "Use of a WorkflowAsLibrary",

				"Parameters": {
					"WorkflowAsLibraryLabel": "Best Picture of the Video",
					"WorkflowAsLibraryType": "MMS",

					"ImageRetention": "1d",
					"ImageTags": "FACE",
					"Ingester": "Admin",
					"InitialFramesNumberToBeSkipped": 1500,
					"InstantInSeconds": 60,
					"Label": "Image label",
					"Title": "My Title"
				},
				"Type": "Workflow-As-Library"
			}
		 */
		string field = "Variables";
		if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
		{
			Json::Value variablesRoot = requestBodyRoot[field];
			if (variablesRoot.begin() != variablesRoot.end())
			// if (variablesRoot.size() > 0)
			{
				string localRequestBody = requestBody;

				_logger->info(__FILEREF__ + "variables processing...");

				for(Json::Value::iterator it = variablesRoot.begin(); it != variablesRoot.end(); ++it)
				{
					Json::Value key = it.key();

					Json::StreamWriterBuilder wbuilder;
					string sKey = Json::writeString(wbuilder, key);
					if (sKey.length() > 2)
						sKey = sKey.substr(1, sKey.length() - 2);

					_logger->info(__FILEREF__ + "variable processing"
						+ ", sKey: " + sKey
					);

					string variableToBeReplaced;
					string sValue;
					{
						Json::Value variableDetails = (*it);

						field = "Type";
						string variableType = variableDetails.get(field, "XXX").asString();

						field = "IsNull";
						bool variableIsNull = JSONUtils::asBool(variableDetails, field, false);

						if (variableType != "json")
							variableToBeReplaced = string("${") + sKey + "}";
						else
							variableToBeReplaced = string("\"${") + sKey + "}\"";

						if (variablesValuesToBeUsedRoot == Json::nullValue)
						{
							field = "Value";
							if (variableType == "string")
							{
								if (variableIsNull)
								{
									sValue = "";
								}
								else
								{
									sValue = variableDetails.get(field, "").asString();

									// scenario, the json will be: "field": "${var_name}"
									//	so in case the value of the variable contains " we have
									//	to replace it with \"
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
									sValue = variableDetails.get(field, "").asString();
							}
							else if (variableType == "datetime-millisecs")
							{
								if (variableIsNull)
									sValue = "";
								else
									sValue = variableDetails.get(field, "").asString();
							}
							else if (variableType == "jsonObject")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									Json::StreamWriterBuilder wbuilder;
									sValue = Json::writeString(wbuilder, variableDetails[field]);
								}
							}
							else
							{
								string errorMessage = __FILEREF__ + "Wrong Variable Type parsing RequestBody"
									+ ", variableType: " + variableType
									+ ", requestBody: " + requestBody
								;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}

							_logger->info(__FILEREF__ + "variable information"
								+ ", sKey: " + sKey
								+ ", variableType: " + variableType
								+ ", variableIsNull: " + to_string(variableIsNull)
								+ ", sValue: " + sValue
							);
						}
						else
						{
							if (variableType == "string")
							{
								sValue = variablesValuesToBeUsedRoot.get(sKey, "").asString();

								// scenario, the json will be: "field": "${var_name}"
								//	so in case the value of the variable contains " we have
								//	to replace it with \"
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
								sValue = variablesValuesToBeUsedRoot.get(sKey, "").asString();
							else if (variableType == "datetime-millisecs")
								sValue = variablesValuesToBeUsedRoot.get(sKey, "").asString();
							else if (variableType == "jsonObject")
							{
								if (variableIsNull)
									sValue = "null";
								else
								{
									Json::StreamWriterBuilder wbuilder;
									sValue = Json::writeString(wbuilder, variablesValuesToBeUsedRoot[sKey]);
								}
							}
							else
							{
								string errorMessage = __FILEREF__ + "Wrong Variable Type parsing RequestBody"
									+ ", variableType: " + variableType
									+ ", requestBody: " + requestBody
								;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}

							_logger->info(__FILEREF__ + "variable information"
								+ ", sKey: " + sKey
								+ ", variableType: " + variableType
								+ ", variableIsNull: " + to_string(variableIsNull)
								+ ", sValue: " + sValue
							);
						}
					}

					_logger->info(__FILEREF__ + "requestBody, replace"
						+ ", variableToBeReplaced: " + variableToBeReplaced
						+ ", sValue: " + sValue
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

						// Advance index forward so the next iteration doesn't pick it up as well.
						index += sValue.length();
					}
				}
                    
				_logger->info(__FILEREF__ + "requestBody after the replacement of the variables"
					+ ", localRequestBody: " + localRequestBody
				);
                    
				{
					Json::CharReaderBuilder builder;
					Json::CharReader* reader = builder.newCharReader();
					string errors;

					bool parsingSuccessful = reader->parse(localRequestBody.c_str(),
						localRequestBody.c_str() + localRequestBody.size(), 
						&requestBodyRoot, &errors);
					delete reader;

					if (!parsingSuccessful)
					{
						string errorMessage = __FILEREF__ + "failed to parse the localRequestBody"
							+ ", errors: " + errors
							+ ", localRequestBody: " + localRequestBody
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("requestBody json is not well format")
			+ ", requestBody: " + requestBody
			+ ", e.what(): " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch(exception e)
	{
		string errorMessage = string("requestBody json is not well format")
			+ ", requestBody: " + requestBody
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	return requestBodyRoot;
}

void API::manageReferencesInput(int64_t ingestionRootKey,
	string taskOrGroupOfTasksLabel, string ingestionType,
	Json::Value& taskRoot,	// taskRoot updated with the new parametersRoot
	bool parametersSectionPresent,
	// parametersRoot is changed:
	//	1. added ReferenceIngestionJobKey in case of ReferenceLabel
	//	2. added all the inherited references
	Json::Value& parametersRoot,
	// dependOnIngestionJobKeysForStarting is extended with the ReferenceIngestionJobKey in case of ReferenceLabel
	vector<int64_t>& dependOnIngestionJobKeysForStarting,
	// dependOnIngestionJobKeysOverallInput is extended with the References present into the Task
	vector<int64_t>& dependOnIngestionJobKeysOverallInput,
	// mapLabelAndIngestionJobKey is extended with the ReferenceLabels
	unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey)
{
	string field;

	// initialize referencesRoot
    bool referencesSectionPresent = false;
    Json::Value referencesRoot(Json::arrayValue);
    if (parametersSectionPresent)
    {
        field = "References";
        if (JSONUtils::isMetadataPresent(parametersRoot, field))
        {
            referencesRoot = parametersRoot[field];

            referencesSectionPresent = true;
        }
    }

	// Generally if the References tag is present, these will be used as references for the Task
	// In case the References tag is NOT present, inherited references are used
	// Sometimes, we want to use both, the references coming from the tag and the inherid references.
	// For example a video is ingested and we want to overlay a logo that is already present into MMS.
	// In this case we add the Reference for the Image and we inherit the video from the Add-Content Task.
	// In these case we use the "DependenciesToBeAddedToReferencesAt" parameter.

	// 2021-04-25: "DependenciesToBeAddedToReferencesAt" could be:
	//	- AtTheBeginning
	//	- AtTheEnd
	//	- an integer specifying the position where to place the dependencies.
	//		0 means AtTheBeginning
	int dependenciesToBeAddedToReferencesAtIndex = -1;
	{
		string atTheBeginning = "Beginning";
		string atTheEnd = "End";

		string dependenciesToBeAddedToReferencesAt;
		field = "DependenciesToBeAddedToReferencesAt";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			dependenciesToBeAddedToReferencesAt = parametersRoot.get(field, "").asString();
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
						dependenciesToBeAddedToReferencesAtIndex =
							stoi(dependenciesToBeAddedToReferencesAt);
						if (dependenciesToBeAddedToReferencesAtIndex > referencesRoot.size())
							dependenciesToBeAddedToReferencesAtIndex = referencesRoot.size();
	
					}
					catch (exception e)
					{
						string errorMessage = string("DependenciesToBeAddedToReferencesAt is not well format")
							+ ", dependenciesToBeAddedToReferencesAt: " + dependenciesToBeAddedToReferencesAt
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}
	}

	// manage ReferenceLabel, inside the References Tag, If ReferenceLabel is present,
	// replace it with ReferenceIngestionJobKey
    if (referencesSectionPresent)
    {
        bool referencesChanged = false;

        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); ++referenceIndex)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];

            field = "ReferenceLabel";
            if (JSONUtils::isMetadataPresent(referenceRoot, field))
            {
                string referenceLabel = referenceRoot.get(field, "").asString();

                if (referenceLabel == "")
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be empty"
						+ ", processing label: " + taskOrGroupOfTasksLabel
						+ ", referenceLabel: " + referenceLabel;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];

                if (ingestionJobKeys.size() == 0)
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value is not found"
						+ ", processing label: " + taskOrGroupOfTasksLabel
						+ ", referenceLabel: " + referenceLabel;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                else if (ingestionJobKeys.size() > 1)
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be used in more than one Task"
                            + ", referenceLabel: " + referenceLabel
                            + ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size())
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }

                field = "ReferenceIngestionJobKey";
                referenceRoot[field] = ingestionJobKeys.back();
                
                referencesRoot[referenceIndex] = referenceRoot;
                
                field = "References";
                parametersRoot[field] = referencesRoot;
            
                referencesChanged = true;
                
                // The workflow specifies expliticily a reference (input for the task).
                // Probable this is because the Reference is not part of the
                // 'dependOnIngestionJobKeysOverallInput' parameter that it is generally
                // same of 'dependOnIngestionJobKeysForStarting'.
                // For this reason we have to make sure this Reference is inside
                // dependOnIngestionJobKeysForStarting in order to avoid the Task starts
                // when the input is not yet ready
                vector<int64_t>::iterator itrIngestionJobKey = find(
                        dependOnIngestionJobKeysForStarting.begin(), dependOnIngestionJobKeysForStarting.end(), 
                        ingestionJobKeys.back());
                if (itrIngestionJobKey == dependOnIngestionJobKeysForStarting.end())
                    dependOnIngestionJobKeysForStarting.push_back(ingestionJobKeys.back());
            }
        }

        /*
        if (referencesChanged)
        {
            {
                Json::StreamWriterBuilder wbuilder;

                taskMetadata = Json::writeString(wbuilder, parametersRoot);        
            }

            // commented because already logged in mmsEngineDBFacade
            // _logger->info(__FILEREF__ + "update IngestionJob"
            //     + ", localDependOnIngestionJobKey: " + to_string(localDependOnIngestionJobKey)
            //    + ", taskMetadata: " + taskMetadata
            // );

            _mmsEngineDBFacade->updateIngestionJobMetadataContent(conn, localDependOnIngestionJobKeyExecution, taskMetadata);
        }
        */
    }

    _logger->info(__FILEREF__ + "add to referencesRoot all the inherited references?"
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
        + ", taskOrGroupOfTasksLabel: " + taskOrGroupOfTasksLabel
        + ", IngestionType: " + ingestionType
        + ", parametersSectionPresent: " + to_string(parametersSectionPresent)
        + ", referencesSectionPresent: " + to_string(referencesSectionPresent)
        + ", dependenciesToBeAddedToReferencesAtIndex: "
			+ to_string(dependenciesToBeAddedToReferencesAtIndex)
        + ", dependOnIngestionJobKeysOverallInput.size(): " + to_string(dependOnIngestionJobKeysOverallInput.size())
    );

	// add to referencesRoot all the inherited references
    if ((!referencesSectionPresent || dependenciesToBeAddedToReferencesAtIndex != -1)
			&& dependOnIngestionJobKeysOverallInput.size() > 0)
    {
		// Enter here if No References tag is present (so we have to add the inherit input)
		// OR we want to add dependOnReferences to the Raferences tag

		if (dependenciesToBeAddedToReferencesAtIndex != -1)
		{
			{
				int previousReferencesRootSize = referencesRoot.size();
				int dependOnIngestionJobKeysSize = dependOnIngestionJobKeysOverallInput.size();

				_logger->info(__FILEREF__ + "add to referencesRoot all the inherited references"
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", taskOrGroupOfTasksLabel: " + taskOrGroupOfTasksLabel
					+ ", previousReferencesRootSize: " + to_string(previousReferencesRootSize)
					+ ", dependOnIngestionJobKeysSize: " + to_string(dependOnIngestionJobKeysSize)
					+ ", dependenciesToBeAddedToReferencesAtIndex: "
						+ to_string(dependenciesToBeAddedToReferencesAtIndex)
				);

				referencesRoot.resize(previousReferencesRootSize + dependOnIngestionJobKeysSize);
				for(int index = previousReferencesRootSize - 1;
					index >= dependenciesToBeAddedToReferencesAtIndex;
					index--)
				{
					_logger->info(__FILEREF__ + "making 'space' in referencesRoot"
						+ ", ingestionRootKey: " + to_string(ingestionRootKey)
						+ ", from " + to_string(index) + " to " + to_string(index + dependOnIngestionJobKeysSize)
					);

					referencesRoot[index + dependOnIngestionJobKeysSize] = referencesRoot[index];
				}

				for(int index = dependenciesToBeAddedToReferencesAtIndex;
					index < dependenciesToBeAddedToReferencesAtIndex + dependOnIngestionJobKeysSize;
					index++)
				{
					_logger->info(__FILEREF__ + "fill in dependOnIngestionJobKey"
						+ ", ingestionRootKey: " + to_string(ingestionRootKey)
						+ ", from " + to_string(index - dependenciesToBeAddedToReferencesAtIndex) + " to " + to_string(index)
					);

					Json::Value referenceRoot;
					string addedField = "ReferenceIngestionJobKey";
					referenceRoot[addedField] = dependOnIngestionJobKeysOverallInput.at(
						index - dependenciesToBeAddedToReferencesAtIndex);

					referencesRoot[index] = referenceRoot;
				}
			}

			/*
			for (int referenceIndex = dependOnIngestionJobKeysOverallInput.size();
				referenceIndex > 0; --referenceIndex)
			{
				Json::Value referenceRoot;
				string addedField = "ReferenceIngestionJobKey";
				referenceRoot[addedField] = dependOnIngestionJobKeysOverallInput.at(referenceIndex - 1);

				// add at the beginning in referencesRoot
				{
					int previousSize = referencesRoot.size();
					referencesRoot.resize(previousSize + 1);
					for(int index = previousSize; index > dependenciesToBeAddedToReferencesAtIndex;
						index--)
						referencesRoot[index] = referencesRoot[index - 1];
					referencesRoot[dependenciesToBeAddedToReferencesAtIndex] = referenceRoot;
				}
			}
			*/
		}
		else
		{
			for (int referenceIndex = 0;
				referenceIndex < dependOnIngestionJobKeysOverallInput.size();
				++referenceIndex)
			{
				Json::Value referenceRoot;
				string addedField = "ReferenceIngestionJobKey";
				referenceRoot[addedField] = dependOnIngestionJobKeysOverallInput.at(referenceIndex);

				referencesRoot.append(referenceRoot);
			}
		}

        field = "Parameters";
        string arrayField = "References";
        parametersRoot[arrayField] = referencesRoot;
        if (!parametersSectionPresent)
        {
            taskRoot[field] = parametersRoot;
        }

        //{
        //    Json::StreamWriterBuilder wbuilder;

        //    taskMetadata = Json::writeString(wbuilder, parametersRoot);        
        //}
        
        // commented because already logged in mmsEngineDBFacade
        // _logger->info(__FILEREF__ + "update IngestionJob"
        //     + ", localDependOnIngestionJobKey: " + to_string(localDependOnIngestionJobKey)
        //    + ", taskMetadata: " + taskMetadata
        // );

        //_mmsEngineDBFacade->updateIngestionJobMetadataContent(conn, localDependOnIngestionJobKeyExecution, taskMetadata);
    }
}

// return: ingestionJobKey associated to this task
vector<int64_t> API::ingestionSingleTask(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
		shared_ptr<Workspace> workspace, int64_t ingestionRootKey, Json::Value& taskRoot, 

		// dependOnSuccess == 0 -> OnError
		// dependOnSuccess == 1 -> OnSuccess
		// dependOnSuccess == -1 -> OnComplete
		// list of ingestion job keys to be executed before this task
        vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,

		// the media input are retrieved looking at the media generated by this list 
        vector<int64_t> dependOnIngestionJobKeysOverallInput,

        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
        string& responseBody)
{
    string field = "Type";
    string type = taskRoot.get(field, "").asString();

    string taskLabel;
    field = "Label";
    if (JSONUtils::isMetadataPresent(taskRoot, field))
    {
        taskLabel = taskRoot.get(field, "").asString();
    }

	_logger->info(__FILEREF__ + "Processing SingleTask..."
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
		+ ", type: " + type
		+ ", taskLabel: " + taskLabel
	);
    
    field = "Parameters";
    Json::Value parametersRoot;
    bool parametersSectionPresent = false;
    if (JSONUtils::isMetadataPresent(taskRoot, field))
    {
        parametersRoot = taskRoot[field];
        
        parametersSectionPresent = true;
    }
    
    if (type == "Encode")
    {
		// we will create a group of tasks and add there the Encode task in two scenarios:
		// case 1. in case of EncodingProfilesSet
		// case 2. in case we will have more than one References

		string encodingProfilesSetKeyField = "EncodingProfilesSetKey";
		string encodingProfilesSetLabelField = "EncodingProfilesSetLabel";
		string referencesField = "References";

		if (parametersSectionPresent && 
			(
			// case 1
            (JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
				|| JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)
            )
			||
			// case 2
			(JSONUtils::isMetadataPresent(parametersRoot, referencesField)
				&& parametersRoot[referencesField].size() > 1)
			)
		)
		{
			// we will replace the single Task with a GroupOfTasks where every task
			// is just for one profile/one reference

			// case 1
			bool profilesSetPresent = false;
			// case 2
			bool multiReferencesPresent = false;

			// we will use the vector for case 1
			vector<int64_t> encodingProfilesSetKeys;

            if (JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
				|| JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetLabelField))
			{
				// case 1

				profilesSetPresent = true;

				string encodingProfilesSetReference;

				if (JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetKeyField))
				{
					int64_t encodingProfilesSetKey = JSONUtils::asInt64(parametersRoot, encodingProfilesSetKeyField, 0);
        
					encodingProfilesSetReference = to_string(encodingProfilesSetKey);
            
					encodingProfilesSetKeys = 
						_mmsEngineDBFacade->getEncodingProfileKeysBySetKey(
						workspace->_workspaceKey, encodingProfilesSetKey);

					{
						Json::Value removed;
						parametersRoot.removeMember(encodingProfilesSetKeyField, &removed);
					}
				}
				else // if (JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetLabelField))
				{
					string encodingProfilesSetLabel = parametersRoot.get(encodingProfilesSetLabelField, "").asString();
        
					encodingProfilesSetReference = encodingProfilesSetLabel;
            
					encodingProfilesSetKeys = 
						_mmsEngineDBFacade->getEncodingProfileKeysBySetLabel(
							workspace->_workspaceKey, encodingProfilesSetLabel);

					{
						Json::Value removed;
						parametersRoot.removeMember(encodingProfilesSetLabelField, &removed);
					}
				}

				if (encodingProfilesSetKeys.size() == 0)
				{
					string errorMessage = __FILEREF__ + "No EncodingProfileKey into the EncodingProfilesSetKey"
						+ ", EncodingProfilesSetKey/EncodingProfilesSetLabel: " + encodingProfilesSetReference
						+ ", ingestionRootKey: " + to_string(ingestionRootKey)
						+ ", type: " + type
						+ ", taskLabel: " + taskLabel
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			// both, case 1 and case 2 could have multiple references
			Json::Value multiReferencesRoot;
			if (JSONUtils::isMetadataPresent(parametersRoot, referencesField)
				&& parametersRoot[referencesField].size() > 1)
			{
				multiReferencesPresent = true;

				multiReferencesRoot = parametersRoot[referencesField];

				{
					Json::Value removed;
					parametersRoot.removeMember(referencesField, &removed);
				}
			}

			if (!profilesSetPresent && !multiReferencesPresent)
			{
				string errorMessage = __FILEREF__ + "It's not possible to be here"
					+ ", type: " + type
					+ ", taskLabel: " + taskLabel
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			// based of the removeMember, parametersRoot will be:
			// in case of profiles set, without the profiles set parameter
			// in case of multiple references, without the References parameter

			Json::Value newTasksRoot(Json::arrayValue);

			if (profilesSetPresent && multiReferencesPresent)
			{
				for (int64_t encodingProfileKey: encodingProfilesSetKeys)
				{
					for(int referenceIndex = 0; referenceIndex < multiReferencesRoot.size(); referenceIndex++)
					{
						Json::Value newTaskRoot;
						string localLabel = taskLabel + " - EncodingProfileKey: " + to_string(encodingProfileKey)
							+ " - referenceIndex: " + to_string(referenceIndex)
						;
							
						field = "Label";
						newTaskRoot[field] = localLabel;

						field = "Type";
						newTaskRoot[field] = "Encode";

						Json::Value newParametersRoot = parametersRoot;

						field = "EncodingProfileKey";
						newParametersRoot[field] = encodingProfileKey;

						{
							Json::Value newReferencesRoot(Json::arrayValue);
							newReferencesRoot.append(multiReferencesRoot[referenceIndex]);

							field = "References";
							newParametersRoot[field] = newReferencesRoot;
						}

						field = "Parameters";
						newTaskRoot[field] = newParametersRoot;

						newTasksRoot.append(newTaskRoot);
					}
				}
			}
			else if (profilesSetPresent && !multiReferencesPresent)
			{
				for (int64_t encodingProfileKey: encodingProfilesSetKeys)
				{
					Json::Value newTaskRoot;
					string localLabel = taskLabel + " - EncodingProfileKey: " + to_string(encodingProfileKey)
					;
						
					field = "Label";
					newTaskRoot[field] = localLabel;

					field = "Type";
					newTaskRoot[field] = "Encode";

					Json::Value newParametersRoot = parametersRoot;

					field = "EncodingProfileKey";
					newParametersRoot[field] = encodingProfileKey;

					field = "Parameters";
					newTaskRoot[field] = newParametersRoot;

					newTasksRoot.append(newTaskRoot);
				}
			}
			else if (!profilesSetPresent && multiReferencesPresent)
			{
				for(int referenceIndex = 0; referenceIndex < multiReferencesRoot.size(); referenceIndex++)
				{
					Json::Value newTaskRoot;
					string localLabel = taskLabel + " - referenceIndex: " + to_string(referenceIndex)
					;
						
					field = "Label";
					newTaskRoot[field] = localLabel;

					field = "Type";
					newTaskRoot[field] = "Encode";

					Json::Value newParametersRoot = parametersRoot;

					{
						Json::Value newReferencesRoot(Json::arrayValue);
						newReferencesRoot.append(multiReferencesRoot[referenceIndex]);

						field = "References";
						newParametersRoot[field] = newReferencesRoot;
					}

					field = "Parameters";
					newTaskRoot[field] = newParametersRoot;

					newTasksRoot.append(newTaskRoot);
				}
			}
        
			Json::Value newParametersTasksGroupRoot;

			field = "ExecutionType";
			newParametersTasksGroupRoot[field] = "parallel";

			field = "Tasks";
			newParametersTasksGroupRoot[field] = newTasksRoot;

			Json::Value newTasksGroupRoot;

			field = "Type";
			newTasksGroupRoot[field] = "GroupOfTasks";

			field = "Parameters";
			newTasksGroupRoot[field] = newParametersTasksGroupRoot;
        
			field = "OnSuccess";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}

			field = "OnError";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}

			field = "OnComplete";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}
        
			return ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, 
                dependOnIngestionJobKeysForStarting, dependOnSuccess,
                dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                responseBody); 
		}
		else
		{
			_logger->info(__FILEREF__ + "No special management for Encode"
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", taskLabel: " + taskLabel
				+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
			);
		}
	}
    else if (type == "Live-Recorder")
    {
		// 1. Live-Recorder needs the UserKey/ApiKey for the ingestion of the chunks.
		// The same UserKey/ApiKey used for the ingestion of the Workflow are used to ingest the chunks

		// 2. Live-Recorder generates MediaItems as soon as the files/segments are generated by the Live-Recorder.
		// For this reason, the events (onSuccess, onError, onComplete) have to be attached
		// to the workflow built to add these contents
		// Here, we will remove the events (onSuccess, onError, onComplete) from LiveRecorder, if present,
		// and we will add temporary inside the Parameters section. These events will be managed later
		// in EncoderVideoAudioProxy.cpp when the workflow for the generated contents will be created

        Json::Value internalMMSRoot;
		{

			string field = "userKey";
			internalMMSRoot[field] = userKey;

			field = "apiKey";
			internalMMSRoot[field] = apiKey;

			/*
			// 2021-01-17: in case of MonitorHLS, MMS has to build a path to save the live segments.
			//	We will generated now a 'key' that will be used to build the path where the live segments
			//	are generated.
			//	That will guarantee that 2 recordings of the same channels will have two different paths
			field = "MonitorHLS";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				field = "deliveryKey";
				internalMMSRoot[field] = now.time_since_epoch().count();
			}
			*/
		}

		string onSuccessField = "OnSuccess";
		string onErrorField = "OnError";
		string onCompleteField = "OnComplete";
    	if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField)
			|| JSONUtils::isMetadataPresent(taskRoot, onErrorField)
			|| JSONUtils::isMetadataPresent(taskRoot, onCompleteField)
		)
    	{
    		if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField))
			{
        		Json::Value onSuccessRoot = taskRoot[onSuccessField];

				internalMMSRoot[onSuccessField] = onSuccessRoot;

				Json::Value removed;
				taskRoot.removeMember(onSuccessField, &removed);
			}
    		if (JSONUtils::isMetadataPresent(taskRoot, onErrorField))
			{
        		Json::Value onErrorRoot = taskRoot[onErrorField];

				internalMMSRoot[onErrorField] = onErrorRoot;

				Json::Value removed;
				taskRoot.removeMember(onErrorField, &removed);
			}
    		if (JSONUtils::isMetadataPresent(taskRoot, onCompleteField))
			{
        		Json::Value onCompleteRoot = taskRoot[onCompleteField];

				internalMMSRoot[onCompleteField] = onCompleteRoot;

				Json::Value removed;
				taskRoot.removeMember(onCompleteField, &removed);
			}
		}

		string internalMMSField = "InternalMMS";
		parametersRoot[internalMMSField] = internalMMSRoot;
	}
    else if (type == "Live-Cut")
    {
		// 1. Live-Cut needs the UserKey/ApiKey for the ingestion of the cut workflow.
		// The same UserKey/ApiKey used for the ingestion of the Workflow are used to ingest the cut
		//
		// 2. Live-Cut generates a workflow made of Concat plus Cut.
		// For this reason, the events (onSuccess, onError, onComplete) have to be attached
		// to the previous workflow 
		// Here, we will remove the events (onSuccess, onError, onComplete) from LiveCut, if present,
		// and we will add temporary inside the Parameters section. These events will be managed later
		// in MMSEngineProcessor.cpp when the new workflow will be created

        Json::Value internalMMSRoot;
		{
			string field = "userKey";
			internalMMSRoot[field] = userKey;

			field = "apiKey";
			internalMMSRoot[field] = apiKey;
		}

		string onSuccessField = "OnSuccess";
		string onErrorField = "OnError";
		string onCompleteField = "OnComplete";
    	if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField)
			|| JSONUtils::isMetadataPresent(taskRoot, onErrorField)
			|| JSONUtils::isMetadataPresent(taskRoot, onCompleteField)
		)
    	{
    		if (JSONUtils::isMetadataPresent(taskRoot, onSuccessField))
			{
        		Json::Value onSuccessRoot = taskRoot[onSuccessField];

				internalMMSRoot[onSuccessField] = onSuccessRoot;

				Json::Value removed;
				taskRoot.removeMember(onSuccessField, &removed);
			}
    		if (JSONUtils::isMetadataPresent(taskRoot, onErrorField))
			{
        		Json::Value onErrorRoot = taskRoot[onErrorField];

				internalMMSRoot[onErrorField] = onErrorRoot;

				Json::Value removed;
				taskRoot.removeMember(onErrorField, &removed);
			}
    		if (JSONUtils::isMetadataPresent(taskRoot, onCompleteField))
			{
        		Json::Value onCompleteRoot = taskRoot[onCompleteField];

				internalMMSRoot[onCompleteField] = onCompleteRoot;

				Json::Value removed;
				taskRoot.removeMember(onCompleteField, &removed);
			}
		}

		string internalMMSField = "InternalMMS";
		parametersRoot[internalMMSField] = internalMMSRoot;
	}
    else if (type == "Add-Content")
    {
		// The Add-Content Task can be used also to add just a variant/profile of a content
		// that it is already present into the MMS Repository.
		// This content that it is already present can be referenced using
		// the apposite parameter (VariantOfMediaItemKey) or using the VariantOfReferencedLabel
		// parameter.
		// In this last case, we have to add the VariantOfIngestionJobKey parameter using VariantOfReferencedLabel

		string field = "VariantOfReferencedLabel";
    	if (JSONUtils::isMetadataPresent(parametersRoot, field))
    	{
			string referenceLabel = parametersRoot.get(field, "").asString();

			if (referenceLabel == "")
			{
				string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be empty"
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", type: " + type
					+ ", taskLabel: " + taskLabel
					+ ", referenceLabel: " + referenceLabel;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];
                
			if (ingestionJobKeys.size() == 0)
			{
				string errorMessage = __FILEREF__ + "The 'referenceLabel' value is not found"
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", type: " + type
					+ ", taskLabel: " + taskLabel
					+ ", referenceLabel: " + referenceLabel;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			else if (ingestionJobKeys.size() > 1)
			{
				string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be used in more than one Task"
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", type: " + type
					+ ", taskLabel: " + taskLabel
					+ ", referenceLabel: " + referenceLabel
					+ ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size())
				;
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
			string workflowAsLibraryTypeField = "WorkflowAsLibraryType";
			string workflowAsLibraryLabelField = "WorkflowAsLibraryLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, workflowAsLibraryTypeField)
				|| !JSONUtils::isMetadataPresent(parametersRoot, workflowAsLibraryLabelField)
			)
			{
				string errorMessage = __FILEREF__ + "No WorkflowAsLibraryType/WorkflowAsLibraryLabel parameters into the Workflow-As-Library Task"
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", type: " + type
					+ ", taskLabel: " + taskLabel
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			string workflowAsLibraryType = parametersRoot.get(workflowAsLibraryTypeField, "").asString();
			string workflowAsLibraryLabel = parametersRoot.get(workflowAsLibraryLabelField, "").asString();

			int workspaceKey;
			if (workflowAsLibraryType == "MMS")
				workspaceKey = -1;
			else
				workspaceKey = workspace->_workspaceKey;

			workflowLibraryContent = _mmsEngineDBFacade->getWorkflowAsLibraryContent(
				workspaceKey, workflowAsLibraryLabel);
		}

		Json::Value workflowLibraryRoot = manageWorkflowVariables(workflowLibraryContent,
				parametersRoot);

		// create a GroupOfTasks and add the Root Task of the Library to the newGroupOfTasks

		Json::Value workflowLibraryTaskRoot;
		{
			string workflowRootTaskField = "Task";
			if (!JSONUtils::isMetadataPresent(workflowLibraryRoot, workflowRootTaskField))
			{
				string errorMessage = __FILEREF__ + "Wrong Workflow-As-Library format. Root Task was not found"
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", type: " + type
					+ ", taskLabel: " + taskLabel
					+ ", workflowLibraryContent: " + workflowLibraryContent
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			workflowLibraryTaskRoot = workflowLibraryRoot[workflowRootTaskField];
		}

		Json::Value newGroupOfTasksRoot;
		{
			Json::Value newGroupOfTasksParametersRoot;

			field = "ExecutionType";
			newGroupOfTasksParametersRoot[field] = "parallel";

			{
				Json::Value newTasksRoot(Json::arrayValue);
				newTasksRoot.append(workflowLibraryTaskRoot);

				field = "Tasks";
				newGroupOfTasksParametersRoot[field] = newTasksRoot;
			}
        
			field = "Type";
			newGroupOfTasksRoot[field] = "GroupOfTasks";

			field = "Parameters";
			newGroupOfTasksRoot[field] = newGroupOfTasksParametersRoot;
        
			field = "OnSuccess";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newGroupOfTasksRoot[field] = taskRoot[field];
			}

			field = "OnError";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newGroupOfTasksRoot[field] = taskRoot[field];
			}

			field = "OnComplete";
			if (JSONUtils::isMetadataPresent(taskRoot, field))
			{
				newGroupOfTasksRoot[field] = taskRoot[field];
			}
		}

		return ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey, newGroupOfTasksRoot, 
			dependOnIngestionJobKeysForStarting, dependOnSuccess,
			dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
			responseBody); 
    }

	manageReferencesInput(ingestionRootKey,
		taskLabel, type, taskRoot,
		parametersSectionPresent, parametersRoot,
		dependOnIngestionJobKeysForStarting,
		dependOnIngestionJobKeysOverallInput,
		mapLabelAndIngestionJobKey);

    string taskMetadata;

    if (parametersSectionPresent)
    {                
        Json::StreamWriterBuilder wbuilder;

        taskMetadata = Json::writeString(wbuilder, parametersRoot);        
    }
    
	vector<int64_t> waitForGlobalIngestionJobKeys;
	{
		field = "WaitFor";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			Json::Value waitForRoot = parametersRoot[field];

			for (int waitForIndex = 0; waitForIndex < waitForRoot.size(); ++waitForIndex)
			{
				Json::Value waitForLabelRoot = waitForRoot[waitForIndex];

				field = "GlobalIngestionLabel";
				if (JSONUtils::isMetadataPresent(waitForLabelRoot, field))
				{
					string waitForGlobalIngestionLabel = waitForLabelRoot.get(field, "").asString();

					_mmsEngineDBFacade->getIngestionJobsKeyByGlobalLabel (
							workspace->_workspaceKey, waitForGlobalIngestionLabel,
							waitForGlobalIngestionJobKeys);
					_logger->info(__FILEREF__ + "getIngestionJobsKeyByGlobalLabel"
						+ ", ingestionRootKey: " + to_string(ingestionRootKey)
						+ ", taskLabel: " + taskLabel
						+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
						+ ", waitForGlobalIngestionLabel: " + waitForGlobalIngestionLabel
						+ ", waitForGlobalIngestionJobKeys.size(): " + to_string(waitForGlobalIngestionJobKeys.size())
					);
				}
			}
		}
	}

	string processingStartingFrom;
	{
		field = "ProcessingStartingFrom";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
			processingStartingFrom = parametersRoot.get(field, "").asString();

		if (processingStartingFrom == "")
		{
			tm tmUTCDateTime;
			char sProcessingStartingFrom[64];

			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcNow  = chrono::system_clock::to_time_t(now);

			gmtime_r (&utcNow, &tmUTCDateTime);
			sprintf (sProcessingStartingFrom, "%04d-%02d-%02dT%02d:%02d:%02dZ",
				tmUTCDateTime. tm_year + 1900,
				tmUTCDateTime. tm_mon + 1,
				tmUTCDateTime. tm_mday,
				tmUTCDateTime. tm_hour,
				tmUTCDateTime. tm_min,
				tmUTCDateTime. tm_sec);

			processingStartingFrom = sProcessingStartingFrom;
		}
	}

    _logger->info(__FILEREF__ + "add IngestionJob"
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
        + ", taskLabel: " + taskLabel
        + ", taskMetadata: " + taskMetadata
        + ", IngestionType: " + type
        + ", processingStartingFrom: " + processingStartingFrom
        + ", dependOnIngestionJobKeysForStarting.size(): " + to_string(dependOnIngestionJobKeysForStarting.size())
        + ", dependOnSuccess: " + to_string(dependOnSuccess)
        + ", waitForGlobalIngestionJobKeys.size(): " + to_string(waitForGlobalIngestionJobKeys.size())
    );

	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(conn,
		workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata,
		MMSEngineDBFacade::toIngestionType(type), processingStartingFrom,
		dependOnIngestionJobKeysForStarting, dependOnSuccess, waitForGlobalIngestionJobKeys);
	field = "ingestionJobKey";
	taskRoot[field] = localDependOnIngestionJobKeyExecution;

	_logger->info(__FILEREF__ + "Save Label..."
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
		+ ", taskLabel: " + taskLabel
		+ ", localDependOnIngestionJobKeyExecution: " + to_string(localDependOnIngestionJobKeyExecution)
	);
    if (taskLabel != "")
        (mapLabelAndIngestionJobKey[taskLabel]).push_back(localDependOnIngestionJobKeyExecution);
    
    if (responseBody != "")
        responseBody += ", ";    
    responseBody +=
            (string("{ ")
            + "\"ingestionJobKey\": " + to_string(localDependOnIngestionJobKeyExecution) + ", "
            + "\"label\": \"" + taskLabel + "\" "
            + "}");

    vector<int64_t> localDependOnIngestionJobKeysForStarting;
    vector<int64_t> localDependOnIngestionJobKeysOverallInput;
    localDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKeyExecution);
    localDependOnIngestionJobKeysOverallInput.push_back(localDependOnIngestionJobKeyExecution);
    
	vector<int64_t> referencesOutputIngestionJobKeys;
    ingestionEvents(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
            localDependOnIngestionJobKeysForStarting, localDependOnIngestionJobKeysOverallInput,

			// in case of OnError, OverallInput has to be the same of the failed task
            dependOnIngestionJobKeysOverallInput,

			referencesOutputIngestionJobKeys,

			mapLabelAndIngestionJobKey, responseBody);


    return localDependOnIngestionJobKeysForStarting;
}

vector<int64_t> API::ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
	shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
	Json::Value& groupOfTasksRoot, 
	vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
	vector<int64_t> dependOnIngestionJobKeysOverallInput,
	unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
	string& responseBody)
{

	string type = "GroupOfTasks";

	string groupOfTaskLabel;
	string field = "Label";
	if (JSONUtils::isMetadataPresent(groupOfTasksRoot, field))
	{
		groupOfTaskLabel = groupOfTasksRoot.get(field, "").asString();
	}

	_logger->info(__FILEREF__ + "Processing GroupOfTasks..."
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
		+ ", groupOfTaskLabel: " + groupOfTaskLabel
	);

	// initialize parametersRoot
    field = "Parameters";
    if (!JSONUtils::isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    Json::Value& parametersRoot = groupOfTasksRoot[field];

    bool parallelTasks;
    
    field = "ExecutionType";
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string executionType = parametersRoot.get(field, "").asString();
    if (executionType == "parallel")
        parallelTasks = true;
    else if (executionType == "sequential")
        parallelTasks = false;
    else
    {
        string errorMessage = __FILEREF__ + "executionType field is wrong"
                + ", executionType: " + executionType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "Tasks";
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    Json::Value& tasksRoot = parametersRoot[field];

	/* 2021-02-20: A group that does not have any Task couls be a scenario,
	 * so we do not have to raise an error. Same check commented in Validation.cpp
    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
	*/

    // vector<int64_t> newDependOnIngestionJobKeysForStarting;
    vector<int64_t> newDependOnIngestionJobKeysOverallInputBecauseOfTasks;
    vector<int64_t> newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput;
    vector<int64_t> lastDependOnIngestionJobKeysForStarting;

	// dependOnSuccess for the Tasks 
	// case 1: parent (IngestionJob or Group of Tasks) On Success ---> GroupOfTasks
	//		in this case the Tasks will be executed depending the status of the parent,
	//		if success, the Tasks have to be executed.
	//		So dependOnSuccessForTasks = dependOnSuccess
	// case 2: parent Tasks of a Group of Tasks ---> GroupOfTasks (destination)
	//		In this case, if the parent Group of Tasks is executed, also the GroupOfTasks (destination)
	//		has to be executed
	//		So dependOnSuccessForTasks = -1 (OnComplete)
    for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
    {
        Json::Value& taskRoot = tasksRoot[taskIndex];

        string field = "Type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();
            
        vector<int64_t> localIngestionTaskDependOnIngestionJobKeyExecution;
        if (parallelTasks)
        {
            if (taskType == "GroupOfTasks")
            {
                int localDependOnSuccess = -1;

                localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                    conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);
            }
            else
            {
                localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
                    conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, dependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);
            }
        }
        else
        {
            if (taskIndex == 0)
            {
                if (taskType == "GroupOfTasks")
                {
					int localDependOnSuccess = -1;

                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                        conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                        dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                        responseBody);
                }
                else
                {
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
                        conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysForStarting, dependOnSuccess,
                        dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                        responseBody);
                }
            }
            else
            {
                int localDependOnSuccess = -1;
                
                if (taskType == "GroupOfTasks")
                {
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                        conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                        lastDependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                        dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                        responseBody);
                }
                else
                {
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
                        conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                        lastDependOnIngestionJobKeysForStarting, localDependOnSuccess,
                        dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                        responseBody);
                }
            }
            
            lastDependOnIngestionJobKeysForStarting = localIngestionTaskDependOnIngestionJobKeyExecution;
        }

        for (int64_t localDependOnIngestionJobKey: localIngestionTaskDependOnIngestionJobKeyExecution)
        {
            // newDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKey);
            newDependOnIngestionJobKeysOverallInputBecauseOfTasks.push_back(localDependOnIngestionJobKey);
        }
    }

    vector<int64_t> referencesOutputIngestionJobKeys;

	// The GroupOfTasks output (media) can be:
	// 1. the one generated by the first level of Tasks (newDependOnIngestionJobKeysOverallInputBecauseOfTasks)
	// 2. the one specified by the ReferencesOutput tag (newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput)
	//
	// In case of 1. it is needed to add the ReferencesOutput tag into the metadata json and
	// fill it with the newDependOnIngestionJobKeysOverallInputBecauseOfTasks data
	// In case of 2., ReferencesOutput is already into the metadata json. In case ReferenceLabel is used,
	// we have to change them with ReferenceIngestionJobKey
	bool referencesOutputPresent = false;
	{
		// initialize referencesRoot
		Json::Value referencesOutputRoot(Json::arrayValue);

		field = "ReferencesOutput";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			referencesOutputRoot = parametersRoot[field];

			referencesOutputPresent = referencesOutputRoot.size() > 0;
		}

		// manage ReferenceOutputLabel, inside the References Tag, If present ReferenceLabel,
		// replace it with ReferenceIngestionJobKey
		if (referencesOutputPresent)
		{
			// GroupOfTasks will wait only the specified ReferencesOutput. For this reason we replace
			// the ingestionJobKeys into newDependOnIngestionJobKeysOverallInput with the one of ReferencesOutput

			for (int referenceIndex = 0; referenceIndex < referencesOutputRoot.size(); ++referenceIndex)
			{
				Json::Value referenceOutputRoot = referencesOutputRoot[referenceIndex];

				field = "ReferenceLabel";
				if (JSONUtils::isMetadataPresent(referenceOutputRoot, field))
				{
					string referenceLabel = referenceOutputRoot.get(field, "XXX").asString();

					if (referenceLabel == "")
					{
						string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be empty"
                            + ", referenceLabel: " + referenceLabel;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
                
					vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];
                
					if (ingestionJobKeys.size() == 0)
					{
						string errorMessage = __FILEREF__ + "The 'referenceLabel' value is not found"
                            + ", referenceLabel: " + referenceLabel;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					else if (ingestionJobKeys.size() > 1)
					{
						string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be used in more than one Task"
                            + ", referenceLabel: " + referenceLabel
                            + ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size())
                            ;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					field = "ReferenceIngestionJobKey";
					referenceOutputRoot[field] = ingestionJobKeys.back();
                
					referencesOutputRoot[referenceIndex] = referenceOutputRoot;
                
					field = "ReferencesOutput";
					parametersRoot[field] = referencesOutputRoot;

					newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.push_back(
							ingestionJobKeys.back());

					referencesOutputIngestionJobKeys.push_back(ingestionJobKeys.back());
				}
			}
		}
		else if (newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size() > 0)
		{
			_logger->info(__FILEREF__ + "add to referencesOutputRoot all the inherited references?"
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", groupOfTaskLabel: " + groupOfTaskLabel
				+ ", referencesOutputPresent: " + to_string(referencesOutputPresent)
				+ ", newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(): "
					+ to_string(newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size())
			);

			// Enter here if No ReferencesOutput tag is present (so we have to add the inherit input)
			// OR we want to add dependOnReferences to the Raferences tag

			for (int referenceIndex = 0; referenceIndex < newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size();
					++referenceIndex)
			{
				Json::Value referenceOutputRoot;
				field = "ReferenceIngestionJobKey";
				referenceOutputRoot[field] = newDependOnIngestionJobKeysOverallInputBecauseOfTasks.at(referenceIndex);
            
				referencesOutputRoot.append(referenceOutputRoot);

				referencesOutputIngestionJobKeys.push_back(
						newDependOnIngestionJobKeysOverallInputBecauseOfTasks.at(referenceIndex));
			}

			_logger->info(__FILEREF__ + "Since ReferencesOutput is not present, set automatically the ReferencesOutput array tag using the ingestionJobKey of the Tasks"
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", groupOfTaskLabel: " + groupOfTaskLabel
				+ ", newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(): "
					+ to_string(newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size())
				+ ", referencesOutputRoot.size: " + to_string(referencesOutputRoot.size())
			);

			field = "ReferencesOutput";
			parametersRoot[field] = referencesOutputRoot;
			/*
			field = "Parameters";
			if (!parametersSectionPresent)
			{
				groupOfTaskRoot[field] = parametersRoot;
			}
			*/
		}
    }

	string processingStartingFrom;
	{
		field = "ProcessingStartingFrom";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
			processingStartingFrom = parametersRoot.get(field, "").asString();

		if (processingStartingFrom == "")
		{
			tm tmUTCDateTime;
			char sProcessingStartingFrom[64];

			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcNow  = chrono::system_clock::to_time_t(now);

			gmtime_r (&utcNow, &tmUTCDateTime);
			sprintf (sProcessingStartingFrom, "%04d-%02d-%02dT%02d:%02d:%02dZ",
				tmUTCDateTime. tm_year + 1900,
				tmUTCDateTime. tm_mon + 1,
				tmUTCDateTime. tm_mday,
				tmUTCDateTime. tm_hour,
				tmUTCDateTime. tm_min,
				tmUTCDateTime. tm_sec);

			processingStartingFrom = sProcessingStartingFrom;
		}
	}

	string taskMetadata;
	{
		Json::StreamWriterBuilder wbuilder;

		taskMetadata = Json::writeString(wbuilder, parametersRoot);
	}

	_logger->info(__FILEREF__ + "add IngestionJob (Group of Tasks)"
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
		+ ", groupOfTaskLabel: " + groupOfTaskLabel
		+ ", taskMetadata: " + taskMetadata
		+ ", IngestionType: " + type
		+ ", processingStartingFrom: " + processingStartingFrom
		+ ", newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size(): "
			+ to_string(newDependOnIngestionJobKeysOverallInputBecauseOfTasks.size())
		+ ", newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.size(): "
			+ to_string(newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput.size())
		+ ", dependOnSuccess: " + to_string(dependOnSuccess)
		+ ", referencesOutputPresent: " + to_string(referencesOutputPresent)
	);

	// - By default we fill newDependOnIngestionJobKeysOverallInput with the ingestionJobKeys
	//		of the first level of Tasks to be executed by the Group of Tasks
	// - dependOnSuccess: we have to set it to -1, otherwise,
	//		if the dependent job will fail and the dependency is OnSuccess or viceversa,
	//		the GroupOfTasks will not be executed
	vector<int64_t> waitForGlobalIngestionJobKeys;
	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(conn,
		workspace->_workspaceKey, ingestionRootKey, groupOfTaskLabel, taskMetadata,
		MMSEngineDBFacade::toIngestionType(type), processingStartingFrom,
		referencesOutputPresent ? newDependOnIngestionJobKeysOverallInputBecauseOfReferencesOutput
			: newDependOnIngestionJobKeysOverallInputBecauseOfTasks,
			dependOnSuccess,
			waitForGlobalIngestionJobKeys);
	field = "ingestionJobKey";
	groupOfTasksRoot[field] = localDependOnIngestionJobKeyExecution;

	// for each group of tasks child, the group of tasks (parent) IngestionJobKey is set
	{
		int64_t parentGroupOfTasksIngestionJobKey = localDependOnIngestionJobKeyExecution;
		for (int64_t childIngestionJobKey: newDependOnIngestionJobKeysOverallInputBecauseOfTasks)
		{
			_mmsEngineDBFacade->updateIngestionJobParentGroupOfTasks(
					conn, childIngestionJobKey,
					parentGroupOfTasksIngestionJobKey);
		}
	}

	_logger->info(__FILEREF__ + "Save Label..."
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
		+ ", groupOfTaskLabel: " + groupOfTaskLabel
		+ ", localDependOnIngestionJobKeyExecution: " + to_string(localDependOnIngestionJobKeyExecution)
	);
	if (groupOfTaskLabel != "")
		(mapLabelAndIngestionJobKey[groupOfTaskLabel]).push_back(localDependOnIngestionJobKeyExecution);

    if (responseBody != "")
        responseBody += ", ";    
    responseBody +=
            (string("{ ")
            + "\"ingestionJobKey\": " + to_string(localDependOnIngestionJobKeyExecution) + ", "
            + "\"label\": \"" + groupOfTaskLabel + "\" "
            + "}");

	/*
	 * 2019-10-01.
	 *		We have the following workflow:
	 *			GroupOfTasks to execute three Cuts (the three cuts have retention set to 0).
	 *			OnSuccess of the GroupOfTasks we have the Concat of the three Cuts
	 *
	 *			Here we are managing the GroupOfTasks and, in the below ingestionEvents, we are passing as dependencies,
	 *			just the ingestionJobKey of the GroupOfTasks.
	 *			In this case, we may have the following scenario:
	 *				1. MMSEngine first execute the three Cuts
	 *				2. MMSEngine execute the GroupOfTasks
	 *				3. MMSEngine starts the retention check and remove the three cuts. This is because the GroupOfTasks
	 *					is executed and the three cuts does not have any other dependencies
	 *				4. MMSEngine executes the Concat and fails because there are no cuts anymore
	 *
	 *		Actually the Tasks (1) specified by OnSuccess/OnError/OnComplete of the GroupOfTasks depend just
	 *		on the GroupOfTasks IngestionJobKey.
	 *		We need to add the ONCOMPLETE dependencies between the Tasks just mentioned above (1) and
	 *		the ReferencesOutput of the GroupOfTasks. This will solve the issue above. It is important that
	 *		the dependency is ONCOMPLETE. This because otherwise, if the dependency is OnSuccess and a ReferenceOutput fails,
	 *		the Tasks (1) will be marked as End_NotToBeExecuted and we do not want this because the execution or not
	 *		of the Task has to be decided ONLY by the logic inside the GroupOfTasks and not by the ReferenceOutput Task.
	 *
	 *		To implement that, we provide, as input parameter, the ReferencesOutput to the ingestionEvents method.
	 *		The ingestionEvents add the dependencies, OnComplete, between the Tasks (1) and the ReferencesOutput.
	 *
	 */
    vector<int64_t> localDependOnIngestionJobKeysForStarting;
    localDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKeyExecution);

    ingestionEvents(conn, userKey, apiKey, workspace, ingestionRootKey, groupOfTasksRoot, 
		localDependOnIngestionJobKeysForStarting, localDependOnIngestionJobKeysForStarting,
		// in case of OnError, OverallInput has to be the same of the failed task
        dependOnIngestionJobKeysOverallInput,

		referencesOutputIngestionJobKeys,
		mapLabelAndIngestionJobKey, responseBody);

    return localDependOnIngestionJobKeysForStarting;
}

void API::ingestionEvents(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        Json::Value& taskOrGroupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
        vector<int64_t> dependOnIngestionJobKeysOverallInputOnError,
        vector<int64_t>& referencesOutputIngestionJobKeys,
        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
        string& responseBody)
{

    string field = "OnSuccess";
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value& onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!JSONUtils::isMetadataPresent(onSuccessRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value& taskRoot = onSuccessRoot[field];                        

        string field = "Type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

		vector<int64_t> localIngestionJobKeys;
        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = 1;
            localIngestionJobKeys = ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = 1;
            localIngestionJobKeys = ingestionSingleTask(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
        }

		// to understand the reason I'm adding these dependencies, look at the comment marked as '2019-10-01'
		// inside the ingestionGroupOfTasks method
		{
			int dependOnSuccess = -1;	// OnComplete
			int orderNumber = -1;
			bool referenceOutputDependency = true;

			for (int64_t localIngestionJobKey: localIngestionJobKeys)
			{
				for (int64_t localReferenceOutputIngestionJobKey: referencesOutputIngestionJobKeys)
				{
					_mmsEngineDBFacade->addIngestionJobDependency (
						conn, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber,
						referenceOutputDependency);
				}
			}
		}
    }

    field = "OnError";
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value& onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!JSONUtils::isMetadataPresent(onErrorRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value& taskRoot = onErrorRoot[field];                        

        string field = "Type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "").asString();

		vector<int64_t> localIngestionJobKeys;
        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = 0;
            localIngestionJobKeys = ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = 0;
            localIngestionJobKeys = ingestionSingleTask(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
                    responseBody);            
        }

		// to understand the reason I'm adding these dependencies, look at the comment marked as '2019-10-01'
		// inside the ingestionGroupOfTasks method
		{
			int dependOnSuccess = -1;	// OnComplete
			int orderNumber = -1;
			bool referenceOutputDependency = true;

			for (int64_t localIngestionJobKey: localIngestionJobKeys)
			{
				for (int64_t localReferenceOutputIngestionJobKey: referencesOutputIngestionJobKeys)
				{
					_mmsEngineDBFacade->addIngestionJobDependency (
						conn, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey, orderNumber,
						referenceOutputDependency);
				}
			}
		}
    }    

    field = "OnComplete";
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value& onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!JSONUtils::isMetadataPresent(onCompleteRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value& taskRoot = onCompleteRoot[field];                        

        string field = "Type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "").asString();

		vector<int64_t> localIngestionJobKeys;
        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = -1;
            localIngestionJobKeys = ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = -1;
            localIngestionJobKeys = ingestionSingleTask(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
        }

		// to understand the reason I'm adding these dependencies, look at the comment marked as '2019-10-01'
		// inside the ingestionGroupOfTasks method
		{
			int dependOnSuccess = -1;	// OnComplete
			int orderNumber = -1;
			bool referenceOutputDependency = true;

			for (int64_t localIngestionJobKey: localIngestionJobKeys)
			{
				for (int64_t localReferenceOutputIngestionJobKey: referencesOutputIngestionJobKeys)
				{
					_mmsEngineDBFacade->addIngestionJobDependency (
						conn, localIngestionJobKey, dependOnSuccess, localReferenceOutputIngestionJobKey,
						orderNumber, referenceOutputDependency);
				}
			}
		}
    }    
}

void API::uploadedBinary(
        FCGX_Request& request,
        string requestMethod,
        unordered_map<string, string> queryParameters,
		shared_ptr<Workspace> workspace,
        // unsigned long contentLength,
        unordered_map<string, string>& requestDetails
)
{
    string api = "uploadedBinary";

    // char* buffer = nullptr;

    try
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("'ingestionJobKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

        auto binaryPathFileIt = requestDetails.find("HTTP_X_FILE");
        if (binaryPathFileIt == requestDetails.end())
        {
            string errorMessage = string("'HTTP_X_FILE' item is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
		// sourceBinaryPathFile will be something like: /var/catramms/storage/nginxWorkingAreaRepository/0000001023
        string sourceBinaryPathFile = binaryPathFileIt->second;

        // Content-Range: bytes 0-99999/100000
        bool contentRangePresent = false;
        long long contentRangeStart  = -1;
        long long contentRangeEnd  = -1;
        long long contentRangeSize  = -1;
        auto contentRangeIt = requestDetails.find("HTTP_CONTENT_RANGE");
        if (contentRangeIt != requestDetails.end())
        {
            string contentRange = contentRangeIt->second;
            try
            {
                parseContentRange(contentRange,
                    contentRangeStart,
                    contentRangeEnd,
                    contentRangeSize);

                contentRangePresent = true;                
            }
            catch(exception e)
            {
                string errorMessage = string("Content-Range is not well done. Expected format: 'Content-Range: bytes <start>-<end>/<size>'")
                    + ", contentRange: " + contentRange
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }
        }

        _logger->info(__FILEREF__ + "Content-Range details"
            + ", contentRangePresent: " + to_string(contentRangePresent)
            + ", contentRangeStart: " + to_string(contentRangeStart)
            + ", contentRangeEnd: " + to_string(contentRangeEnd)
            + ", contentRangeSize: " + to_string(contentRangeSize)
        );

        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
        string destBinaryPathName =
			workspaceIngestionRepository
			+ "/"
			+ to_string(ingestionJobKey)
			+ "_source";
		bool segmentedContent = false;
		try
		{
			tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus,
				string, string> ingestionJobDetails = _mmsEngineDBFacade->getIngestionJobDetails(
						workspace->_workspaceKey, ingestionJobKey);

			string parameters;
			tie(ignore, ignore, ignore, parameters, ignore) = ingestionJobDetails;

			Json::Value parametersRoot;
			{
				Json::CharReaderBuilder builder;
				Json::CharReader* reader = builder.newCharReader();
				string errors;

				bool parsingSuccessful = reader->parse(parameters.c_str(),
						parameters.c_str() + parameters.size(),
						&parametersRoot, &errors);
				delete reader;

				if (!parsingSuccessful)
				{
					string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", errors: " + errors
						+ ", parameters: " + parameters
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			string field = "FileFormat";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string fileFormat = parametersRoot.get(field, "").asString();
				if (fileFormat == "m3u8")
					segmentedContent = true;
			}
		}
		catch(runtime_error e)
		{
			string errorMessage = string("mmsEngineDBFacade->getIngestionJobDetails failed")
				+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceBinaryPathFile: " + sourceBinaryPathFile
				+ ", destBinaryPathName: " + destBinaryPathName
				+ ", e.what: " + e.what()
			;
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);            
		}
		catch(exception e)
		{
			string errorMessage = string("mmsEngineDBFacade->getIngestionJobDetails failed")
				+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceBinaryPathFile: " + sourceBinaryPathFile
				+ ", destBinaryPathName: " + destBinaryPathName
			;
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);            
		}
		if (segmentedContent)                                                                                 
			destBinaryPathName = destBinaryPathName + ".tar.gz";

        if (!contentRangePresent)
        {
            try
            {
                _logger->info(__FILEREF__ + "Moving file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourceBinaryPathFile: " + sourceBinaryPathFile
                    + ", destBinaryPathName: " + destBinaryPathName
                );

                FileIO::moveFile(sourceBinaryPathFile, destBinaryPathName);
            }
            catch(runtime_error e)
            {
                string errorMessage = string("Error to move file")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourceBinaryPathFile: " + sourceBinaryPathFile
                    + ", destBinaryPathName: " + destBinaryPathName
					+ ", e.what: " + e.what()
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }
            catch(exception e)
            {
                string errorMessage = string("Error to move file")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourceBinaryPathFile: " + sourceBinaryPathFile
                    + ", destBinaryPathName: " + destBinaryPathName
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }

			if (segmentedContent)
			{
				try
				{
					// by a convention, the directory inside the tar file has to be named as 'content'
                    string localSourceBinaryPathFile = "/content.tar.gz";

					_logger->info(__FILEREF__ + "Calling manageTarFileInCaseOfIngestionOfSegments "
						+ ", destBinaryPathName: " + destBinaryPathName
						+ ", workspaceIngestionRepository: " + workspaceIngestionRepository
						+ ", sourceBinaryPathFile: " + sourceBinaryPathFile
						+ ", localSourceBinaryPathFile: " + localSourceBinaryPathFile
					);

					manageTarFileInCaseOfIngestionOfSegments(ingestionJobKey,
							destBinaryPathName, workspaceIngestionRepository,
							localSourceBinaryPathFile);
				}
				catch(runtime_error e)
				{
					string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}

            bool sourceBinaryTransferred = true;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
            );                            
            _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
                ingestionJobKey, sourceBinaryTransferred);
        }
        else
        {
            //  Content-Range is present
            
            if (FileIO::fileExisting (destBinaryPathName))
            {
                if (contentRangeStart == 0)
                {
                    // content is reset
                    ofstream osDestStream(destBinaryPathName.c_str(), 
                            ofstream::binary | ofstream::trunc);

                    osDestStream.close();
                }
                
                bool inCaseOfLinkHasItToBeRead  = false;
                unsigned long workspaceIngestionBinarySizeInBytes = FileIO::getFileSizeInBytes (
                    destBinaryPathName, inCaseOfLinkHasItToBeRead);
                unsigned long binarySizeInBytes = FileIO::getFileSizeInBytes (
                    sourceBinaryPathFile, inCaseOfLinkHasItToBeRead);
                
                if (contentRangeStart != workspaceIngestionBinarySizeInBytes)
                {
                    string errorMessage = string("This is NOT the next expected chunk because Content-Range start is different from fileSizeInBytes")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", contentRangeStart: " + to_string(contentRangeStart)
                        + ", workspaceIngestionBinarySizeInBytes: " + to_string(workspaceIngestionBinarySizeInBytes)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
                
                /*
                if (contentRangeEnd - contentRangeStart + 1 != binarySizeInBytes)
                {
                    string errorMessage = string("The size specified by Content-Range start and end is not consistent with the size of the binary ingested")
                        + ", contentRangeStart: " + to_string(contentRangeStart)
                        + ", contentRangeEnd: " + to_string(contentRangeEnd)
                        + ", binarySizeInBytes: " + to_string(binarySizeInBytes)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
                 */
                
                try
                {
                    bool removeSrcFileAfterConcat = true;
                    
                    _logger->info(__FILEREF__ + "Concat file"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", destBinaryPathName: " + destBinaryPathName
                        + ", sourceBinaryPathFile: " + sourceBinaryPathFile
                        + ", removeSrcFileAfterConcat: " + to_string(removeSrcFileAfterConcat)
                    );

                    FileIO::concatFile(destBinaryPathName, sourceBinaryPathFile, removeSrcFileAfterConcat);
                }
                catch(exception e)
                {
                    string errorMessage = string("Error to concat file")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", destBinaryPathName: " + destBinaryPathName
                        + ", sourceBinaryPathFile: " + sourceBinaryPathFile
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
            }
            else
            {
                // binary file does not exist, so this is the first chunk
                
                if (contentRangeStart != 0)
                {
                    string errorMessage = string("This is the first chunk of the file and Content-Range start has to be 0")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", contentRangeStart: " + to_string(contentRangeStart)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
                
                try
                {
                    _logger->info(__FILEREF__ + "Moving file"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", sourceBinaryPathFile: " + sourceBinaryPathFile
                        + ", destBinaryPathName: " + destBinaryPathName
                    );

                    FileIO::moveFile(sourceBinaryPathFile, destBinaryPathName);
                }
                catch(exception e)
                {
                    string errorMessage = string("Error to move file")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", sourceBinaryPathFile: " + sourceBinaryPathFile
                        + ", destBinaryPathName: " + destBinaryPathName
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
            }
            
            if (contentRangeEnd + 1 == contentRangeSize)
            {
				if (segmentedContent)
				{
					try
					{
						// by a convention, the directory inside the tar file has to be named as 'content'
						string localSourceBinaryPathFile = "/content.tar.gz";

						_logger->info(__FILEREF__ + "Calling manageTarFileInCaseOfIngestionOfSegments "
							+ ", destBinaryPathName: " + destBinaryPathName
							+ ", workspaceIngestionRepository: " + workspaceIngestionRepository
							+ ", sourceBinaryPathFile: " + sourceBinaryPathFile
							+ ", localSourceBinaryPathFile: " + localSourceBinaryPathFile
						);

						manageTarFileInCaseOfIngestionOfSegments(ingestionJobKey,
							destBinaryPathName, workspaceIngestionRepository,
							localSourceBinaryPathFile);
					}
					catch(runtime_error e)
					{
						string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

                bool sourceBinaryTransferred = true;
                _logger->info(__FILEREF__ + "Update IngestionJob"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
                );                            
                _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
					ingestionJobKey, sourceBinaryTransferred);
            }
        }
        
        string responseBody;
        sendSuccess(request, 201, responseBody);

        /*
        if (requestMethod == "HEAD")
        {
            unsigned long fileSize = 0;
            try
            {
                if (FileIO::fileExisting(workspaceIngestionBinaryPathName))
                {
                    bool inCaseOfLinkHasItToBeRead = false;
                    fileSize = FileIO::getFileSizeInBytes (
                        workspaceIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                }
            }
            catch(exception e)
            {
                string errorMessage = string("Error to retrieve the file size")
                    + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }

            sendHeadSuccess(request, 200, fileSize);
        }
        else
        {
            chrono::system_clock::time_point uploadStartTime = chrono::system_clock::now();

            bool resume = false;
            {
                if (xCatraMMSResumeHeader != "")
                {
                    unsigned long fileSize = 0;
                    try
                    {
                        if (FileIO::fileExisting(workspaceIngestionBinaryPathName))
                        {
                            bool inCaseOfLinkHasItToBeRead = false;
                            fileSize = FileIO::getFileSizeInBytes (
                                workspaceIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                        }
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Error to retrieve the file size")
                            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                        ;
                        _logger->error(__FILEREF__ + errorMessage);
    //
    //                    sendError(500, errorMessage);
    //
    //                    throw runtime_error(errorMessage);            
                    }

                    if (stol(xCatraMMSResumeHeader) == fileSize)
                    {
                        _logger->info(__FILEREF__ + "Resume is enabled"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                        resume = true;
                    }
                    else
                    {
                        _logger->info(__FILEREF__ + "Resume is NOT enabled (X-CatraMMS-Resume header found but different length)"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                    }
                }
                else
                {
                    _logger->info(__FILEREF__ + "Resume flag is NOT present (No X-CatraMMS-Resume header found)"
                    );
                }
            }
            
            ofstream binaryFileStream(workspaceIngestionBinaryPathName, 
                    resume ? (ofstream::binary | ofstream::app) : (ofstream::binary | ofstream::trunc));
            buffer = new char [_binaryBufferLength];

            unsigned long currentRead;
            unsigned long totalRead = 0;
            {
                // we have the content-length and we will use it to read the binary

                chrono::system_clock::time_point lastTimeProgressUpdate = chrono::system_clock::now();
                double lastPercentageUpdated = -1;
                
                unsigned long bytesToBeRead;
                while (totalRead < contentLength)
                {
                    if (contentLength - totalRead >= _binaryBufferLength)
                        bytesToBeRead = _binaryBufferLength;
                    else
                        bytesToBeRead = contentLength - totalRead;

                    currentRead = FCGX_GetStr(buffer, bytesToBeRead, request.in);
                    // cin.read(buffer, bytesToBeRead);
                    // currentRead = cin.gcount();
                    if (currentRead != bytesToBeRead)
                    {
                        // this should never happen because it will be against the content-length
                        string errorMessage = string("Error reading the binary")
                            + ", contentLength: " + to_string(contentLength)
                            + ", totalRead: " + to_string(totalRead)
                            + ", bytesToBeRead: " + to_string(bytesToBeRead)
                            + ", currentRead: " + to_string(currentRead)
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        sendError(request, 400, errorMessage);

                        throw runtime_error(errorMessage);            
                    }

                    totalRead   += currentRead;

                    binaryFileStream.write(buffer, currentRead); 
                    
                    {
                        chrono::system_clock::time_point now = chrono::system_clock::now();

                        if (now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds))
                        {
                            double progress = ((double) totalRead / (double) contentLength) * 100;
                            // int uploadingPercentage = floorf(progress * 100) / 100;
                            // this is to have one decimal in the percentage
                            double uploadingPercentage = ((double) ((int) (progress * 10))) / 10;

                            _logger->info(__FILEREF__ + "Upload still running"
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", progress: " + to_string(progress)
                                + ", uploadingPercentage: " + to_string(uploadingPercentage)
                                + ", totalRead: " + to_string(totalRead)
                                + ", contentLength: " + to_string(contentLength)
                            );

                            lastTimeProgressUpdate = now;

                            if (lastPercentageUpdated != uploadingPercentage)
                            {
                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", uploadingPercentage: " + to_string(uploadingPercentage)
                                );                            
                                _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                    ingestionJobKey, uploadingPercentage);

                                lastPercentageUpdated = uploadingPercentage;
                            }
                        }
                    }
                }
            }

            binaryFileStream.close();

            delete buffer;

            unsigned long elapsedUploadInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - uploadStartTime).count();
            _logger->info(__FILEREF__ + "Binary read"
                + ", contentLength: " + to_string(contentLength)
                + ", totalRead: " + to_string(totalRead)
                + ", elapsedUploadInSeconds: " + to_string(elapsedUploadInSeconds)
            );

//            {
//                // Chew up any remaining stdin - this shouldn't be necessary
//                // but is because mod_fastcgi doesn't handle it correctly.
//
//                // ignore() doesn't set the eof bit in some versions of glibc++
//                // so use gcount() instead of eof()...
//                do 
//                    cin.ignore(bufferLength); 
//                while (cin.gcount() == bufferLength);
//            }    

            bool sourceBinaryTransferred = true;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
            );                            
            _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
                ingestionJobKey, sourceBinaryTransferred);

            string responseBody = string("{ ")
                + "\"contentLength\": " + to_string(contentLength) + ", "
                + "\"writtenBytes\": " + to_string(totalRead) + ", "
                + "\"elapsedUploadInSeconds\": " + to_string(elapsedUploadInSeconds) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
        }
        */
    }
    catch (runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }    
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }    
}

void API::manageTarFileInCaseOfIngestionOfSegments(
		int64_t ingestionJobKey,
		string tarBinaryPathName, string workspaceIngestionRepository,
		string sourcePathName)
{
	string executeCommand;
	try
	{
		// tar into workspaceIngestion directory
		//	source will be something like <ingestion key>_source
		//	destination will be the original directory (that has to be the same name of the tar file name)
		executeCommand =
			"tar xfz " + tarBinaryPathName
			+ " --directory " + workspaceIngestionRepository;
		_logger->info(__FILEREF__ + "Start tar command "
			+ ", executeCommand: " + executeCommand
		);
		chrono::system_clock::time_point startTar = chrono::system_clock::now();
		int executeCommandStatus = ProcessUtility::execute(executeCommand);
		chrono::system_clock::time_point endTar = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "End tar command "
			+ ", executeCommand: " + executeCommand
			+ ", @MMS statistics@ - tarDuration (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count()) + "@"
		);
		if (executeCommandStatus != 0)
		{
			string errorMessage = string("ProcessUtility::execute failed")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", executeCommandStatus: " + to_string(executeCommandStatus) 
				+ ", executeCommand: " + executeCommand 
			;

			_logger->error(__FILEREF__ + errorMessage);
          
			throw runtime_error(errorMessage);
		}

		// sourceFileName is the name of the tar file name that is the same
		//	of the name of the directory inside the tar file
		string sourceFileName;
		{
			string suffix(".tar.gz");
			if (!(sourcePathName.size() >= suffix.size()
				&& 0 == sourcePathName.compare(sourcePathName.size()-suffix.size(), suffix.size(), suffix)))
			{
				string errorMessage = __FILEREF__ + "sourcePathName does not end with " + suffix
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourcePathName: " + sourcePathName
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			size_t startFileNameIndex = sourcePathName.find_last_of("/");
			if (startFileNameIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "sourcePathName bad format"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourcePathName: " + sourcePathName
					+ ", startFileNameIndex: " + to_string(startFileNameIndex)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceFileName = sourcePathName.substr(startFileNameIndex + 1);
			sourceFileName = sourceFileName.substr(0, sourceFileName.size() - suffix.size());
		}

		// remove tar file
		{
			string sourceTarFile = workspaceIngestionRepository + "/"
				+ to_string(ingestionJobKey)
				+ "_source"
				+ ".tar.gz";

			_logger->info(__FILEREF__ + "Remove file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceTarFile: " + sourceTarFile
			);

			FileIO::remove(sourceTarFile);
		}

		// rename directory generated from tar: from user_tar_filename to 1247848_source
		{
			string sourceDirectory = workspaceIngestionRepository + "/" + sourceFileName;
			string destDirectory = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
			_logger->info(__FILEREF__ + "Start moveDirectory..."
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceDirectory: " + sourceDirectory
				+ ", destDirectory: " + destDirectory
			);
			// 2020-05-01: since the remove of the director could fails because of nfs issue,
			//  better do a copy and then a remove.
			//  In this way, in case the remove fails, we can ignore the error.
			//  The directory will be removed later by cron job
			{
				chrono::system_clock::time_point startPoint = chrono::system_clock::now();
				FileIO::copyDirectory(sourceDirectory, destDirectory,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP |
					S_IROTH | S_IXOTH);
				chrono::system_clock::time_point endPoint = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "End copyDirectory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceDirectory: " + sourceDirectory
					+ ", destDirectory: " + destDirectory
					+ ", @MMS COPY statistics@ - copyDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
				);
			}

			try
			{
				chrono::system_clock::time_point startPoint = chrono::system_clock::now();
				bool removeRecursively = true;
				FileIO::removeDirectory(sourceDirectory, removeRecursively);
				chrono::system_clock::time_point endPoint = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "End removeDirectory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceDirectory: " + sourceDirectory
					+ ", @MMS REMOVE statistics@ - removeDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
				);
			}
			catch(runtime_error e)
			{
				string errorMessage = string("removeDirectory failed")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
         
				// throw runtime_error(errorMessage);
			}
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);
         
		throw runtime_error(errorMessage);
	}
}

void API::stopUploadFileProgressThread()
{
    _fileUploadProgressThreadShutdown       = true;
    
    this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
}

void API::fileUploadProgressCheck()
{

    while (!_fileUploadProgressThreadShutdown)
    {
        this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
        
        lock_guard<mutex> locker(_fileUploadProgressData->_mutex);

        for (auto itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.begin(); 
                itr != _fileUploadProgressData->_filesUploadProgressToBeMonitored.end(); )
        {            
            bool iteratorAlreadyUpdated = false;
                        
            if (itr->_callFailures >= _maxProgressCallFailures)
            {
                _logger->error(__FILEREF__ + "fileUploadProgressCheck: remove entry because of too many call failures"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
                    + ", binaryListenHost: " + itr->_binaryListenHost
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
                );
                itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element
                
                continue;
            }
            
            try 
            {
                string progressURL = string("http://:")
					+ itr->_binaryListenHost
					+ ":"
					+ to_string(_webServerPort)
					+ _progressURI;
                string progressIdHeader = string("X-Progress-ID: ") + itr->_progressId;
                string hostHeader = string("Host: ") + itr->_binaryVirtualHostName;

                _logger->info(__FILEREF__ + "Call for upload progress"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
                    + ", binaryListenHost: " + itr->_binaryListenHost
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", progressURL: " + progressURL
                    + ", progressIdHeader: " + progressIdHeader
                    + ", hostHeader: " + hostHeader
                );

                curlpp::Cleanup cleaner;
                curlpp::Easy request;
                ostringstream response;

                list<string> header;
                header.push_back(progressIdHeader);
                header.push_back(hostHeader);	// important for the nginx virtual host

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(progressURL));

				int curlTimeoutInSeconds = 120;
				request.setOpt(new curlpp::options::Timeout(curlTimeoutInSeconds));

                request.setOpt(new curlpp::options::HttpHeader(header));
                request.setOpt(new curlpp::options::WriteStream(&response));
                request.perform();

                string sResponse = response.str();

                // LF and CR create problems to the json parser...
                while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
                    sResponse.pop_back();
                
                _logger->info(__FILEREF__ + "Call for upload progress response"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
                    + ", binaryListenHost: " + itr->_binaryListenHost
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", sResponse: " + sResponse
                );

                try
                {
                    Json::Value uploadProgressResponse;

                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;
                    
                    bool parsingSuccessful = reader->parse(sResponse.c_str(),
                            sResponse.c_str() + sResponse.size(), 
                            &uploadProgressResponse, &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse the response body"
                                + ", errors: " + errors
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    // { "state" : "uploading", "received" : 731195032, "size" : 745871360 }
                    // At the end: { "state" : "done" }
                    // In case of error: { "state" : "error", "status" : 500 }
                    string state = uploadProgressResponse.get("state", "XXX").asString();
                    if (state == "done")
                    {
                        double relativeProgress = 100.0;
                        double relativeUploadingPercentage = 100.0;
                        
                        int64_t absoluteReceived = -1;
                        if (itr->_contentRangePresent)
                            absoluteReceived    = itr->_contentRangeEnd;
                        int64_t absoluteSize = -1;
                        if (itr->_contentRangePresent)
                            absoluteSize    = itr->_contentRangeSize;

                        double absoluteProgress;
                        if (itr->_contentRangePresent)
                            absoluteProgress = ((double) absoluteReceived / (double) absoluteSize) * 100;
                            
                        // this is to have one decimal in the percentage
                        double absoluteUploadingPercentage;
                        if (itr->_contentRangePresent)
                            absoluteUploadingPercentage = ((double) ((int) (absoluteProgress * 10))) / 10;

                        if (itr->_contentRangePresent)
                        {
                            _logger->info(__FILEREF__ + "Upload just finished"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
								+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
								+ ", binaryListenHost: " + itr->_binaryListenHost
                                + ", relativeProgress: " + to_string(relativeProgress)
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                                + ", absoluteProgress: " + to_string(absoluteProgress)
                                + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                            );
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Upload just finished"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
								+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
								+ ", binaryListenHost: " + itr->_binaryListenHost
                                + ", relativeProgress: " + to_string(relativeProgress)
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                            );
                        }

                        if (itr->_contentRangePresent)
                        {
                            _logger->info(__FILEREF__ + "Update IngestionJob"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
								+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
								+ ", binaryListenHost: " + itr->_binaryListenHost
                                + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                            );                            
                            _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                itr->_ingestionJobKey, absoluteUploadingPercentage);
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Update IngestionJob"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
								+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
								+ ", binaryListenHost: " + itr->_binaryListenHost
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                            );                            
                            _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                itr->_ingestionJobKey, relativeUploadingPercentage);
                        }

                        itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element
                        
                        iteratorAlreadyUpdated = true;
                    }
                    else if (state == "error")
                    {
                        _logger->error(__FILEREF__ + "fileUploadProgressCheck: remove entry because state is 'error'"
                            + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                            + ", progressId: " + itr->_progressId
							+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
							+ ", binaryListenHost: " + itr->_binaryListenHost
                            + ", callFailures: " + to_string(itr->_callFailures)
                            + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
                        );
                        itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element

                        iteratorAlreadyUpdated = true;
                    }
                    else if (state == "uploading")
                    {
                        int64_t relativeReceived = JSONUtils::asInt64(uploadProgressResponse, "received", 0);
                        int64_t absoluteReceived = -1;
                        if (itr->_contentRangePresent)
                            absoluteReceived    = relativeReceived + itr->_contentRangeStart;
                        int64_t relativeSize = JSONUtils::asInt64(uploadProgressResponse, "size", 0);
                        int64_t absoluteSize = -1;
                        if (itr->_contentRangePresent)
                            absoluteSize    = itr->_contentRangeSize;

                        double relativeProgress = ((double) relativeReceived / (double) relativeSize) * 100;
                        double absoluteProgress;
                        if (itr->_contentRangePresent)
                            absoluteProgress = ((double) absoluteReceived / (double) absoluteSize) * 100;
                            
                        // this is to have one decimal in the percentage
                        double relativeUploadingPercentage = ((double) ((int) (relativeProgress * 10))) / 10;
                        double absoluteUploadingPercentage;
                        if (itr->_contentRangePresent)
                            absoluteUploadingPercentage = ((double) ((int) (absoluteProgress * 10))) / 10;

                        if (itr->_contentRangePresent)
                        {
                            _logger->info(__FILEREF__ + "Upload still running"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
								+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
								+ ", binaryListenHost: " + itr->_binaryListenHost
                                + ", relativeProgress: " + to_string(relativeProgress)
                                + ", absoluteProgress: " + to_string(absoluteProgress)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                                + ", relativeReceived: " + to_string(relativeReceived)
                                + ", absoluteReceived: " + to_string(absoluteReceived)
                                + ", relativeSize: " + to_string(relativeSize)
                                + ", absoluteSize: " + to_string(absoluteSize)
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                                + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                            );
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Upload still running"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
								+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
								+ ", binaryListenHost: " + itr->_binaryListenHost
                                + ", progress: " + to_string(relativeProgress)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                                + ", received: " + to_string(relativeReceived)
                                + ", size: " + to_string(relativeSize)
                                + ", uploadingPercentage: " + to_string(relativeUploadingPercentage)
                            );
                        }

                        if (itr->_contentRangePresent)
                        {
                            if (itr->_lastPercentageUpdated != absoluteUploadingPercentage)
                            {
                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                    + ", progressId: " + itr->_progressId
									+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
									+ ", binaryListenHost: " + itr->_binaryListenHost
                                    + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                                );                            
                                _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                    itr->_ingestionJobKey, absoluteUploadingPercentage);

                                itr->_lastPercentageUpdated = absoluteUploadingPercentage;
                            }
                        }
                        else
                        {
                            if (itr->_lastPercentageUpdated != relativeUploadingPercentage)
                            {
                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                    + ", progressId: " + itr->_progressId
									+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
									+ ", binaryListenHost: " + itr->_binaryListenHost
                                    + ", uploadingPercentage: " + to_string(relativeUploadingPercentage)
                                );                            
                                _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                    itr->_ingestionJobKey, relativeUploadingPercentage);

                                itr->_lastPercentageUpdated = relativeUploadingPercentage;
                            }
                        }
                    }
                    else
                    {
                        string errorMessage = string("file upload progress. State is wrong")
                            + ", state: " + state
                            + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                            + ", progressId: " + itr->_progressId
							+ ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
							+ ", binaryListenHost: " + itr->_binaryListenHost
                            + ", callFailures: " + to_string(itr->_callFailures)
                            + ", progressURL: " + progressURL
                            + ", progressIdHeader: " + progressIdHeader
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(...)
                {
                    string errorMessage = string("response Body json is not well format")
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch (curlpp::LogicError & e) 
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (LogicError)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
					+ ", binaryListenHost: " + itr->_binaryListenHost
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }
            catch (curlpp::RuntimeError & e) 
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (RuntimeError)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
					+ ", binaryListenHost: " + itr->_binaryListenHost
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }
            catch (runtime_error e)
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (runtime_error)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
					+ ", binaryListenHost: " + itr->_binaryListenHost
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }
            catch (exception e)
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (exception)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", binaryVirtualHostName: " + itr->_binaryVirtualHostName
					+ ", binaryListenHost: " + itr->_binaryListenHost
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }

            if (!iteratorAlreadyUpdated)
                itr++;
        }
    }
}

void API::ingestionRootsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestionRootsStatus";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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
				rows = _maxPageSize;
        }
        
        bool startAndEndIngestionDatePresent = false;
        string startIngestionDate;
        string endIngestionDate;
        auto startIngestionDateIt = queryParameters.find("startIngestionDate");
        auto endIngestionDateIt = queryParameters.find("endIngestionDate");
        if (startIngestionDateIt != queryParameters.end() && endIngestionDateIt != queryParameters.end())
        {
            startIngestionDate = startIngestionDateIt->second;
            endIngestionDate = endIngestionDateIt->second;
            
            startAndEndIngestionDatePresent = true;
        }

        string label;
        auto labelIt = queryParameters.find("label");
        if (labelIt != queryParameters.end() && labelIt->second != "")
        {
            label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
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

        {
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getIngestionRootsStatus(
                    workspace, ingestionRootKey, mediaItemKey,
                    start, rows,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    label, status, asc, ingestionJobOutputs
                    );

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, ingestionStatusRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::ingestionRootMetaDataContent(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestionRootMetaDataContent";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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
            string ingestionRootMetaDataContent =
				_mmsEngineDBFacade->getIngestionRootMetaDataContent(
				workspace, ingestionRootKey, processedMetadata);

            sendSuccess(request, 200, ingestionRootMetaDataContent);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::ingestionJobsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestionJobsStatus";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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
				rows = _maxPageSize;
        }
        
        string label;
        auto labelIt = queryParameters.find("label");
        if (labelIt != queryParameters.end() && labelIt->second != "")
        {
            label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = curlpp::unescape(firstDecoding);
        }

        bool startAndEndIngestionDatePresent = false;
        string startIngestionDate;
        string endIngestionDate;
        auto startIngestionDateIt = queryParameters.find("startIngestionDate");
        auto endIngestionDateIt = queryParameters.find("endIngestionDate");
        if (startIngestionDateIt != queryParameters.end() && endIngestionDateIt != queryParameters.end())
        {
            startIngestionDate = startIngestionDateIt->second;
            endIngestionDate = endIngestionDateIt->second;
            
            startAndEndIngestionDatePresent = true;
        }

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

        string jsonParametersCondition;
        auto jsonParametersConditionIt = queryParameters.find("jsonParametersCondition");
        if (jsonParametersConditionIt != queryParameters.end() && jsonParametersConditionIt->second != "")
        {
            jsonParametersCondition = jsonParametersConditionIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
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

        {
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getIngestionJobsStatus(
                    workspace, ingestionJobKey,
                    start, rows, label,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    ingestionType, jsonParametersCondition, asc, status, ingestionJobOutputs
                    );

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, ingestionStatusRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}


void API::cancelIngestionJob(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "API::cancelIngestionJob";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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
		 *	2. the ffmpeg command will never start, may be because of a wrong url
		 *	In this scenario there is no way to cancel the job because:
		 *		1. The EncoderVideoAudioProxy thread will never exit from his loop
		 *			because it is a Live Proxy. The only way to exit is through the kill of the encoding job
		 *			(kill of the ffmpeg command)
		 *		2. The encoding job cannot be killed because the ffmpeg process will never start
		 *
		 *  Really I discovered later that the above scenario was already managed
		 *  by the kill encoding job method.
		 *  In fact, this method set the encodingStatusFailures into DB to -100.
		 *  The EncoderVideoAudioProxy thread checks this number and, if it is negative, exit for his
		 *  internal look
		 *
		 *  For this reason, it would be better to avoid to use the forceCancel parameter because
		 *  it is set the ingestionJob status to End_CanceledByUser but it could leave
		 *  the EncoderVideoAudioProxy thread allocated and/or the ffmpeg process running.
		 *
		 * This forceCancel parameter is useful in scenarios where we have to force the status
		 * of the IngestionJob to End_CanceledByUser status.
		 * In this case it is important to check if there are active associated EncodingJob
		 * (i.e. ToBeProcessed or Processing) and set them to End_CanceledByUser.
		 *
		 * Otherwise the EncodingJob, orphan of the IngestionJob, will remain definitevely
		 * in this 'active' state creating problems to the Engine.
		 * Also, these EncodingJobs may have also the processor field set to NULL
		 * (specially in case of ToBeProcessed) and therefore they will not managed 
		 * by the reset procedure called when the Engine start.
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

		tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string>
			ingestionJobDetails = _mmsEngineDBFacade->getIngestionJobDetails(
					workspace->_workspaceKey, ingestionJobKey);
		tie(ignore, ignore, ingestionStatus, ignore, ignore) = ingestionJobDetails;

		if (!forceCancel && ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
		{
			string errorMessage = string("The IngestionJob cannot be removed because of his Status")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
			;
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		_logger->info(__FILEREF__ + "Update IngestionJob"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", IngestionStatus: " + "End_CanceledByUser"
			+ ", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
			MMSEngineDBFacade::IngestionStatus::End_CanceledByUser, 
			"");

		if (forceCancel)
			_mmsEngineDBFacade->forceCancelEncodingJob (ingestionJobKey);

        string responseBody;
        sendSuccess(request, 200, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::updateIngestionJob(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
		unordered_map<string, string> queryParameters,
        string requestBody,
		bool admin)
{
    string api = "updateIngestionJob";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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
            _logger->info(__FILEREF__ + "getIngestionJobDetails"
				+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

			tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus,
				string, string> ingestionJobDetails = _mmsEngineDBFacade->getIngestionJobDetails (
					workspace->_workspaceKey, ingestionJobKey);

			string			label;
			MMSEngineDBFacade::IngestionType	ingestionType;
			MMSEngineDBFacade::IngestionStatus ingestionStatus;
			string			metaDataContent;

			tie(label, ingestionType, ingestionStatus, metaDataContent, ignore) = ingestionJobDetails;

			if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
			{
				string errorMessage = string("It is not possible to update an IngestionJob that it is not in Start_TaskQueued status")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			Json::Value metadataRoot;
			try
			{
				Json::CharReaderBuilder builder;
				Json::CharReader* reader = builder.newCharReader();
				string errors;

				bool parsingSuccessful = reader->parse(requestBody.c_str(),
					requestBody.c_str() + requestBody.size(), 
					&metadataRoot, &errors);
				delete reader;

				if (!parsingSuccessful)
				{
					string errorMessage = string("Json metadata failed during the parsing")
						+ ", errors: " + errors
						+ ", json data: " + requestBody
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(exception e)
			{
				string errorMessage = string("Json metadata failed during the parsing"
					", json data: " + requestBody
				);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			string field = "IngestionType";
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = string("IngestionType field is missing")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string sIngestionType = metadataRoot.get("IngestionType", "").asString();

			if (sIngestionType == MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionType::LiveRecorder))
			{
				if (ingestionType != MMSEngineDBFacade::IngestionType::LiveRecorder)
				{
					string errorMessage = string("It was requested an Update of Live-Recorder but IngestionType is not a LiveRecorder")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType)
					;
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
							newIngestionJobLabel = metadataRoot.get("IngestionJobLabel", "").asString();
						}

						field = "ChannelLabel";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							channelLabelModified = true;
							newChannelLabel = metadataRoot.get("ChannelLabel", "").asString();
						}

						field = "RecordingPeriodStart";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							recordingPeriodStartModified = true;
							newRecordingPeriodStart = metadataRoot.get("RecordingPeriodStart", "").asString();
						}

						field = "RecordingPeriodEnd";
						if (JSONUtils::isMetadataPresent(metadataRoot, field))
						{
							recordingPeriodEndModified = true;
							newRecordingPeriodEnd = metadataRoot.get("RecordingPeriodEnd", "").asString();
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
						// Validator validator(_logger, _mmsEngineDBFacade, _configuration);
						DateTime::sDateSecondsToUtc(newRecordingPeriodStart);
					}

					if (recordingPeriodEndModified)
					{
						// Validator validator(_logger, _mmsEngineDBFacade, _configuration);
						DateTime::sDateSecondsToUtc(newRecordingPeriodEnd);
					}

					_logger->info(__FILEREF__ + "Update IngestionJob"
						+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					);

					_mmsEngineDBFacade->updateIngestionJob_LiveRecorder (
						workspace->_workspaceKey,
						ingestionJobKey,
						ingestionJobLabelModified, newIngestionJobLabel,
						channelLabelModified, newChannelLabel,
						recordingPeriodStartModified, newRecordingPeriodStart,
						recordingPeriodEndModified, newRecordingPeriodEnd,
						recordingVirtualVODModified, newRecordingVirtualVOD,
						admin
					);

					_logger->info(__FILEREF__ + "IngestionJob updated"
						+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					);
				}
			}

			Json::Value responseRoot;
			responseRoot["status"] = string("success");

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, responseRoot);
            
            sendSuccess(request, 200, responseBody);            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

