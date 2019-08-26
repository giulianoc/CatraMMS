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
#include <fstream>
#include <sstream>
#include <regex>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
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

        Json::Value requestBodyRoot;
        try
        {
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

            string field = "Variables";
            if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                Json::Value variablesRoot = requestBodyRoot[field];
                if (variablesRoot.begin() != variablesRoot.end())
                {
                    string localRequestBody = requestBody;
                    
                    _logger->info(__FILEREF__ + "variables processing...");
                    
                    for(Json::Value::iterator it = variablesRoot.begin(); it != variablesRoot.end(); ++it)
                    {
                        Json::Value key = it.key();
                        Json::Value value = (*it);
                        
                        Json::StreamWriterBuilder wbuilder;
                        string sKey = Json::writeString(wbuilder, key);
                        if (sKey.length() > 2)  // to remove the first and last "
                            sKey = sKey.substr(1, sKey.length() - 2);
                        string sValue = Json::writeString(wbuilder, value);        
                        if (sValue.length() > 2)    // to remove the first and last "
                            sValue = sValue.substr(1, sValue.length() - 2);
                        
                        // string variableToBeReplaced = string("\\$\\{") + sKey + "\\}";
                        // localRequestBody = regex_replace(localRequestBody, regex(variableToBeReplaced), sValue);
                        string variableToBeReplaced = string("${") + sKey + "}";
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
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            string rootType = requestBodyRoot.get(field, "XXX").asString();

            string rootLabel;
            field = "Label";
            if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                rootLabel = requestBodyRoot.get(field, "XXX").asString();
            }    
            
            int64_t ingestionRootKey = _mmsEngineDBFacade->addIngestionRoot(conn,
                workspace->_workspaceKey, rootType, rootLabel, requestBody.c_str());
    
            field = "Task";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            Json::Value taskRoot = requestBodyRoot[field];                        
            
            field = "Type";
            if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            string taskType = taskRoot.get(field, "XXX").asString();
            
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

            bool commit = true;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);
            
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
				_mmsEngineDBFacade->endIngestionJobs(conn, commit);
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
				_mmsEngineDBFacade->endIngestionJobs(conn, commit);
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
				_mmsEngineDBFacade->endIngestionJobs(conn, commit);
			}


            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
        _logger->info(__FILEREF__ + "Ingestion statistics"
            + ", elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count())
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

// return: ingestionJobKey associated to this task
vector<int64_t> API::ingestionSingleTask(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey, Json::Value taskRoot, 

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
    string type = taskRoot.get(field, "XXX").asString();

    string taskLabel;
    field = "Label";
    if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
    {
        taskLabel = taskRoot.get(field, "XXX").asString();
    }

	_logger->info(__FILEREF__ + "Processing SingleTask..."
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
		+ ", type: " + type
		+ ", taskLabel: " + taskLabel
	);
    
    field = "Parameters";
    Json::Value parametersRoot;
    bool parametersSectionPresent = false;
    if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
    {
        parametersRoot = taskRoot[field];
        
        parametersSectionPresent = true;
    }
    
    if (type == "Encode")
    {
		string encodingProfilesSetKeyField = "EncodingProfilesSetKey";
		string encodingProfilesSetLabelField = "EncodingProfilesSetLabel";

		if (parametersSectionPresent && 
            (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
				|| _mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)
            )
		)
		{
			// to manage the encode of 'profiles set' we will replace the single Task with
			// a GroupOfTasks where every task is just for one profile
        
			string encodingProfilesSetReference;
        
			vector<int64_t> encodingProfilesSetKeys;
			if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetKeyField))
			{
				int64_t encodingProfilesSetKey = parametersRoot.get(encodingProfilesSetKeyField, "XXX").asInt64();
        
				encodingProfilesSetReference = to_string(encodingProfilesSetKey);
            
				encodingProfilesSetKeys = 
					_mmsEngineDBFacade->getEncodingProfileKeysBySetKey(
					workspace->_workspaceKey, encodingProfilesSetKey);
			}
			else // if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetLabelField))
			{
				string encodingProfilesSetLabel = parametersRoot.get(encodingProfilesSetLabelField, "XXX").asString();
        
				encodingProfilesSetReference = encodingProfilesSetLabel;
            
				encodingProfilesSetKeys = 
					_mmsEngineDBFacade->getEncodingProfileKeysBySetLabel(
						workspace->_workspaceKey, encodingProfilesSetLabel);
			}
        
			if (encodingProfilesSetKeys.size() == 0)
			{
				string errorMessage = __FILEREF__ + "No EncodingProfileKey into the EncodingProfilesSetKey"
                    + ", EncodingProfilesSetKey/EncodingProfilesSetLabel: " + encodingProfilesSetReference;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
        
			string encodingPriority;
			field = "EncodingPriority";
			if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
			{
				encodingPriority = parametersRoot.get(field, "XXX").asString();
				/*
				string sRequestedEncodingPriority = parametersRoot.get(field, "XXX").asString();
				MMSEngineDBFacade::EncodingPriority requestedEncodingPriority = 
                    MMSEngineDBFacade::toEncodingPriority(sRequestedEncodingPriority);
            
				encodingPriority = MMSEngineDBFacade::toString(requestedEncodingPriority);
				*/
			}
			/*
			else
			{
				encodingPriority = MMSEngineDBFacade::toString(
                    static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority));
			}
			*/
        
            
			Json::Value newTasksRoot(Json::arrayValue);
        
			for (int64_t encodingProfileKey: encodingProfilesSetKeys)
			{
				Json::Value newTaskRoot;
				string localLabel = taskLabel + " - EncodingProfileKey " + to_string(encodingProfileKey);

				field = "Label";
				newTaskRoot[field] = localLabel;
            
				field = "Type";
				newTaskRoot[field] = "Encode";
            
				Json::Value newParametersRoot;
            
				field = "References";
				if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
				{
					newParametersRoot[field] = parametersRoot[field];
				}
            
				field = "EncodingProfileKey";
				newParametersRoot[field] = encodingProfileKey;
            
				if (encodingPriority != "")
				{
					field = "EncodingPriority";
					newParametersRoot[field] = encodingPriority;
				}
            
				field = "Parameters";
				newTaskRoot[field] = newParametersRoot;
            
				newTasksRoot.append(newTaskRoot);
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
			if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}

			field = "OnError";
			if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}

			field = "OnComplete";
			if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
			{
				newTasksGroupRoot[field] = taskRoot[field];
			}
        
			return ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey, newTasksGroupRoot, 
                dependOnIngestionJobKeysForStarting, dependOnSuccess,
                dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                responseBody); 
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
		}

		string onSuccessField = "OnSuccess";
		string onErrorField = "OnError";
		string onCompleteField = "OnComplete";
    	if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onSuccessField)
			|| _mmsEngineDBFacade->isMetadataPresent(taskRoot, onErrorField)
			|| _mmsEngineDBFacade->isMetadataPresent(taskRoot, onCompleteField)
		)
    	{
    		if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onSuccessField))
			{
        		Json::Value onSuccessRoot = taskRoot[onSuccessField];

				internalMMSRoot[onSuccessField] = onSuccessRoot;

				Json::Value removed;
				taskRoot.removeMember(onSuccessField, &removed);
			}
    		if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onErrorField))
			{
        		Json::Value onErrorRoot = taskRoot[onErrorField];

				internalMMSRoot[onErrorField] = onErrorRoot;

				Json::Value removed;
				taskRoot.removeMember(onErrorField, &removed);
			}
    		if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onCompleteField))
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
    	if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    	{
			string referenceLabel = parametersRoot.get(field, "").asString();

			if (referenceLabel == "")
			{
				string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be empty"
					+ ", processing label: " + taskLabel
					+ ", referenceLabel: " + referenceLabel;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];
                
			if (ingestionJobKeys.size() == 0)
			{
				string errorMessage = __FILEREF__ + "The 'referenceLabel' value is not found"
					+ ", processing label: " + taskLabel
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

			field = "VariantOfIngestionJobKey";
			parametersRoot[field] = ingestionJobKeys.back();
		}
	}

	// Generally if the References tag is present, these will be used as references for the Task
	// In case the References tag is NOT present, inherited references are used
	// In same cases, we want to use both, the references coming from the tag and the inherid references.
	// For example a video is ingested and we want to overlay a logo that is already present into MMS.
	// In this case we add the Reference for the Image and we inherit the video from the Add-Content Task.
	// In these case we use the "DependenciesToBeAddedToReferences" parameter.
    bool dependenciesToBeAddedToReferences = false;
    field = "DependenciesToBeAddedToReferences";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
		dependenciesToBeAddedToReferences = parametersRoot.get(field, false).asBool();
	}

	// initialize referencesRoot
    bool referencesSectionPresent = false;
    Json::Value referencesRoot(Json::arrayValue);
    if (parametersSectionPresent)
    {
        field = "References";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            referencesRoot = parametersRoot[field];

            referencesSectionPresent = true;
        }
    }

	// manage ReferenceLabel, inside the References Tag, If present ReferenceLabel,
	// replace it with ReferenceIngestionJobKey
    if (referencesSectionPresent)
    {
        bool referencesChanged = false;

        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); ++referenceIndex)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];

            field = "ReferenceLabel";
            if (_mmsEngineDBFacade->isMetadataPresent(referenceRoot, field))
            {
                string referenceLabel = referenceRoot.get(field, "XXX").asString();

                if (referenceLabel == "")
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be empty"
						+ ", processing label: " + taskLabel
						+ ", referenceLabel: " + referenceLabel;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];
                
                if (ingestionJobKeys.size() == 0)
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value is not found"
						+ ", processing label: " + taskLabel
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
        + ", taskLabel: " + taskLabel
        + ", IngestionType: " + type
        + ", parametersSectionPresent: " + to_string(parametersSectionPresent)
        + ", referencesSectionPresent: " + to_string(referencesSectionPresent)
        + ", dependenciesToBeAddedToReferences: " + to_string(dependenciesToBeAddedToReferences)
        + ", dependOnIngestionJobKeysOverallInput.size(): " + to_string(dependOnIngestionJobKeysOverallInput.size())
    );

	// add to referencesRoot all the inherited references
    if ((!referencesSectionPresent || dependenciesToBeAddedToReferences)
			&& dependOnIngestionJobKeysOverallInput.size() > 0)
    {
		// Enter here if No References tag is present (so we have to add the inherit input)
		// OR we want to add dependOnReferences to the Raferences tag

        for (int referenceIndex = 0; referenceIndex < dependOnIngestionJobKeysOverallInput.size(); ++referenceIndex)
        {
            Json::Value referenceRoot;
            string addedField = "ReferenceIngestionJobKey";
            referenceRoot[addedField] = dependOnIngestionJobKeysOverallInput.at(referenceIndex);
            
            referencesRoot.append(referenceRoot);
        }

        field = "Parameters";
        string arrayField = "References";
        parametersRoot[arrayField] = referencesRoot;
        if (!parametersSectionPresent)
        {
            taskRoot[field] = parametersRoot;
        }

        /*        
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
        */
    }

    string taskMetadata;

    if (parametersSectionPresent)
    {                
        Json::StreamWriterBuilder wbuilder;

        taskMetadata = Json::writeString(wbuilder, parametersRoot);        
    }
    
    _logger->info(__FILEREF__ + "add IngestionJob"
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
        + ", taskLabel: " + taskLabel
        + ", taskMetadata: " + taskMetadata
        + ", IngestionType: " + type
        + ", dependOnIngestionJobKeysForStarting.size(): " + to_string(dependOnIngestionJobKeysForStarting.size())
        + ", dependOnSuccess: " + to_string(dependOnSuccess)
    );

    int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(conn,
            workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type), 
            dependOnIngestionJobKeysForStarting, dependOnSuccess);
    
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
    
    ingestionEvents(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
            localDependOnIngestionJobKeysForStarting, localDependOnIngestionJobKeysOverallInput,

			// in case of OnError, OverallInput has to be the same of the failed task
            dependOnIngestionJobKeysOverallInput,

			mapLabelAndIngestionJobKey, responseBody);


    return localDependOnIngestionJobKeysForStarting;
}

#ifdef DB_FOR_GROUP_OF_TASKS
vector<int64_t> API::ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
	shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
	Json::Value groupOfTasksRoot, 
	vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
	vector<int64_t> dependOnIngestionJobKeysOverallInput,
	unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
	string& responseBody)
{

	string type = "GroupOfTasks";

	string groupOfTaskLabel;
	string field = "Label";
	if (_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
	{
		groupOfTaskLabel = groupOfTasksRoot.get(field, "XXX").asString();
	}

	_logger->info(__FILEREF__ + "Processing GroupOfTasks..."
		+ ", ingestionRootKey: " + to_string(ingestionRootKey)
		+ ", groupOfTaskLabel: " + groupOfTaskLabel
	);

	// initialize parametersRoot
    field = "Parameters";
    Json::Value parametersRoot;
    if (!_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    parametersRoot = groupOfTasksRoot[field];

    bool parallelTasks;
    
    field = "ExecutionType";
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string executionType = parametersRoot.get(field, "XXX").asString();
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
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    Json::Value tasksRoot = parametersRoot[field];

    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    // vector<int64_t> newDependOnIngestionJobKeysForStarting;
    vector<int64_t> newDependOnIngestionJobKeysOverallInput;
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
        Json::Value taskRoot = tasksRoot[taskIndex];

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
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
            newDependOnIngestionJobKeysOverallInput.push_back(localDependOnIngestionJobKey);
        }
    }

	// The GroupOfTasks output (media) can be:
	// 1. the one generated by the first level of Tasks (newDependOnIngestionJobKeysOverallInput)
	// 2. the one specified by the ReferencesOutput tag
	//
	// In case of 1. it is needed to add the ReferencesOutput tag into the metadata json and
	// fill it with the newDependOnIngestionJobKeysOverallInput data
	// In case of 2., ReferencesOutput is already into the metadata json. In case ReferenceLabel is used,
	// we have to change them with ReferenceIngestionJobKey
	{
		// initialize referencesRoot
		bool referencesOutputSectionPresent = false;
		Json::Value referencesOutputRoot(Json::arrayValue);

		field = "ReferencesOutput";
		if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
		{
			referencesOutputRoot = parametersRoot[field];

			referencesOutputSectionPresent = true;
		}

		// manage ReferenceOutputLabel, inside the References Tag, If present ReferenceLabel,
		// replace it with ReferenceIngestionJobKey
		if (referencesOutputSectionPresent)
		{
			for (int referenceIndex = 0; referenceIndex < referencesOutputRoot.size(); ++referenceIndex)
			{
				Json::Value referenceOutputRoot = referencesOutputRoot[referenceIndex];

				field = "ReferenceLabel";
				if (_mmsEngineDBFacade->isMetadataPresent(referenceOutputRoot, field))
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
				}
			}
		}

		_logger->info(__FILEREF__ + "add to referencesOutputRoot all the inherited references?"
			+ ", ingestionRootKey: " + to_string(ingestionRootKey)
			+ ", groupOfTaskLabel: " + groupOfTaskLabel
			+ ", referencesOutputSectionPresent: " + to_string(referencesOutputSectionPresent)
			+ ", newDependOnIngestionJobKeysOverallInput.size(): " + to_string(newDependOnIngestionJobKeysOverallInput.size())
		);

		// add to referencesRoot all the inherited references
		if (!referencesOutputSectionPresent && newDependOnIngestionJobKeysOverallInput.size() > 0)
		{
			// Enter here if No ReferencesOutput tag is present (so we have to add the inherit input)
			// OR we want to add dependOnReferences to the Raferences tag

			for (int referenceIndex = 0; referenceIndex < newDependOnIngestionJobKeysOverallInput.size(); ++referenceIndex)
			{
				Json::Value referenceOutputRoot;
				field = "ReferenceIngestionJobKey";
				referenceOutputRoot[field] = newDependOnIngestionJobKeysOverallInput.at(referenceIndex);
            
				referencesOutputRoot.append(referenceOutputRoot);
			}

			_logger->info(__FILEREF__ + "Since ReferencesOutput is not present, set automatically the ReferencesOutput array tag using the ingestionJobKey of the Tasks"
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", groupOfTaskLabel: " + groupOfTaskLabel
				+ ", newDependOnIngestionJobKeysOverallInput.size(): " + to_string(newDependOnIngestionJobKeysOverallInput.size())
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
		+ ", newDependOnIngestionJobKeysOverallInput.size(): " + to_string(newDependOnIngestionJobKeysOverallInput.size())
		+ ", dependOnSuccess: " + to_string(dependOnSuccess)
	);

	int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(conn,
		workspace->_workspaceKey, ingestionRootKey, groupOfTaskLabel, taskMetadata,
		MMSEngineDBFacade::toIngestionType(type),
		newDependOnIngestionJobKeysOverallInput, dependOnSuccess);

	// for each group of tasks child, the group of tasks (parent) IngestionJobKey is set
	{
		int64_t parentGroupOfTasksIngestionJobKey = localDependOnIngestionJobKeyExecution;
		for (int64_t childIngestionJobKey: newDependOnIngestionJobKeysOverallInput)
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

    vector<int64_t> localDependOnIngestionJobKeysForStarting;
    localDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKeyExecution);

    ingestionEvents(conn, userKey, apiKey, workspace, ingestionRootKey, groupOfTasksRoot, 
		localDependOnIngestionJobKeysForStarting, localDependOnIngestionJobKeysForStarting,
		// in case of OnError, OverallInput has to be the same of the failed task
        dependOnIngestionJobKeysOverallInput,

		mapLabelAndIngestionJobKey, responseBody);

    return localDependOnIngestionJobKeysForStarting;
}
#endif

#ifdef NO_DB_FOR_GROUP_OF_TASKS
vector<int64_t> API::ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        Json::Value groupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
        vector<int64_t> dependOnIngestionJobKeysOverallInput,
        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey, string& responseBody)
{
    
	// initialize parametersRoot
    string field = "Parameters";
    Json::Value parametersRoot;
    if (!_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    parametersRoot = groupOfTasksRoot[field];

    bool parallelTasks;
    
    field = "ExecutionType";
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string executionType = parametersRoot.get(field, "XXX").asString();
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
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    Json::Value tasksRoot = parametersRoot[field];

    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    vector<int64_t> newDependOnIngestionJobKeysForStarting;
    vector<int64_t> newDependOnIngestionJobKeysOverallInput;
    vector<int64_t> lastDependOnIngestionJobKeysForStarting;
    for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
    {
        Json::Value taskRoot = tasksRoot[taskIndex];

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
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
                localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                    conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, dependOnSuccess, 
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
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                        conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysForStarting, dependOnSuccess, 
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
            newDependOnIngestionJobKeysForStarting.push_back(localDependOnIngestionJobKey);
            newDependOnIngestionJobKeysOverallInput.push_back(localDependOnIngestionJobKey);
        }
    }

    ingestionEvents(conn, userKey, apiKey, workspace, ingestionRootKey, groupOfTasksRoot, 
		newDependOnIngestionJobKeysForStarting, newDependOnIngestionJobKeysOverallInput,
		// in case of OnError, OverallInput has to be the same of the failed task
        dependOnIngestionJobKeysOverallInput,

		mapLabelAndIngestionJobKey, responseBody);
    
    return newDependOnIngestionJobKeysForStarting;
}
#endif

void API::ingestionEvents(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        Json::Value taskOrGroupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
        vector<int64_t> dependOnIngestionJobKeysOverallInputOnError,
        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
        string& responseBody)
{

    string field = "OnSuccess";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onSuccessRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onSuccessRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = 1;
            ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = 1;
            ingestionSingleTask(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
    }

    field = "OnError";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onErrorRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onErrorRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = 0;
            ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = 0;
            ingestionSingleTask(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInputOnError, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
    }    

    field = "OnComplete";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onCompleteRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onCompleteRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = -1;
            ingestionGroupOfTasks(conn, userKey, apiKey, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = -1;
            ingestionSingleTask(conn, userKey, apiKey, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysForStarting, localDependOnSuccess, 
                    dependOnIngestionJobKeysOverallInput, mapLabelAndIngestionJobKey,
                    responseBody);            
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
        string binaryPathFile = binaryPathFileIt->second;

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

        string workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace);
        workspaceIngestionBinaryPathName
                .append("/")
                .append(to_string(ingestionJobKey))
                .append("_source")
                ;
             
        if (!contentRangePresent)
        {
            try
            {
                _logger->info(__FILEREF__ + "Moving file"
                    + ", binaryPathFile: " + binaryPathFile
                    + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                );

                FileIO::moveFile(binaryPathFile, workspaceIngestionBinaryPathName);
            }
            catch(exception e)
            {
                string errorMessage = string("Error to move file")
                    + ", binaryPathFile: " + binaryPathFile
                    + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
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
            
            if (FileIO::fileExisting (workspaceIngestionBinaryPathName))
            {
                if (contentRangeStart == 0)
                {
                    // content is reset
                    ofstream osDestStream(workspaceIngestionBinaryPathName.c_str(), 
                            ofstream::binary | ofstream::trunc);

                    osDestStream.close();
                }
                
                bool inCaseOfLinkHasItToBeRead  = false;
                unsigned long workspaceIngestionBinarySizeInBytes = FileIO::getFileSizeInBytes (
                    workspaceIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                unsigned long binarySizeInBytes = FileIO::getFileSizeInBytes (
                    binaryPathFile, inCaseOfLinkHasItToBeRead);
                
                if (contentRangeStart != workspaceIngestionBinarySizeInBytes)
                {
                    string errorMessage = string("This is NOT the next expected chunk because Content-Range start is different from fileSizeInBytes")
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
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                        + ", binaryPathFile: " + binaryPathFile
                        + ", removeSrcFileAfterConcat: " + to_string(removeSrcFileAfterConcat)
                    );

                    FileIO::concatFile(workspaceIngestionBinaryPathName, binaryPathFile, removeSrcFileAfterConcat);
                }
                catch(exception e)
                {
                    string errorMessage = string("Error to concat file")
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                        + ", binaryPathFile: " + binaryPathFile
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
                        + ", contentRangeStart: " + to_string(contentRangeStart)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
                
                try
                {
                    _logger->info(__FILEREF__ + "Moving file"
                        + ", binaryPathFile: " + binaryPathFile
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                    );

                    FileIO::moveFile(binaryPathFile, workspaceIngestionBinaryPathName);
                }
                catch(exception e)
                {
                    string errorMessage = string("Error to move file")
                        + ", binaryPathFile: " + binaryPathFile
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
            }
            
            if (contentRangeEnd + 1 == contentRangeSize)
            {
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
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
                );
                itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element
                
                continue;
            }
            
            try 
            {
                string progressURL = string("http://localhost:") + to_string(_webServerPort) + _progressURI;
                string progressIdHeader = string("X-Progress-ID: ") + itr->_progressId;

                _logger->info(__FILEREF__ + "Call for upload progress"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", progressURL: " + progressURL
                    + ", progressIdHeader: " + progressIdHeader
                );

                curlpp::Cleanup cleaner;
                curlpp::Easy request;
                ostringstream response;

                list<string> header;
                header.push_back(progressIdHeader);

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(progressURL));
                request.setOpt(new curlpp::options::HttpHeader(header));
                request.setOpt(new curlpp::options::WriteStream(&response));
                request.perform();

                string sResponse = response.str();

                // LF and CR create problems to the json parser...
                while (sResponse.back() == 10 || sResponse.back() == 13)
                    sResponse.pop_back();
                
                _logger->info(__FILEREF__ + "Call for upload progress response"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
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
                            + ", callFailures: " + to_string(itr->_callFailures)
                            + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
                        );
                        itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element

                        iteratorAlreadyUpdated = true;
                    }
                    else if (state == "uploading")
                    {
                        int64_t relativeReceived = uploadProgressResponse.get("received", "XXX").asInt64();
                        int64_t absoluteReceived = -1;
                        if (itr->_contentRangePresent)
                            absoluteReceived    = relativeReceived + itr->_contentRangeStart;
                        int64_t relativeSize = uploadProgressResponse.get("size", "XXX").asInt64();
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

			string labelDecoded = curlpp::unescape(label);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			label = regex_replace(labelDecoded, regex(plus), plusDecoded);

			/*
			CURL *curl = curl_easy_init();
			if(curl)
			{
				int outLength;
				char *decoded = curl_easy_unescape(curl,
						label.c_str(), label.length(), &outLength);
				if(decoded)
				{
					string sDecoded = decoded;
					curl_free(decoded);

					// still there is the '+' char
					string plus = "\\+";
					string plusDecoded = " ";
					label = regex_replace(sDecoded, regex(plus), plusDecoded);
				}
			}
			*/
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

        {
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getIngestionRootsStatus(
                    workspace, ingestionRootKey, mediaItemKey,
                    start, rows,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    label, status, asc
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

        {
            string ingestionRootMetaDataContent = _mmsEngineDBFacade->getIngestionRootMetaDataContent(
                    workspace, ingestionRootKey);

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

			string labelDecoded = curlpp::unescape(label);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			label = regex_replace(labelDecoded, regex(plus), plusDecoded);
			/*
			CURL *curl = curl_easy_init();
			if(curl)
			{
				int outLength;
				char *decoded = curl_easy_unescape(curl,
						label.c_str(), label.length(), &outLength);
				if(decoded)
				{
					string sDecoded = decoded;
					curl_free(decoded);

					// still there is the '+' char
					string plus = "\\+";
					string plusDecoded = " ";
					label = regex_replace(sDecoded, regex(plus), plusDecoded);
				}
			}
			*/
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
                    ingestionType, asc, status
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

