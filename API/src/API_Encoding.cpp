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

#include <fstream>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "API.h"


void API::encodingJobsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "encodingJobsStatus";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t encodingJobKey = -1;
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt != queryParameters.end() && encodingJobKeyIt->second != "")
        {
            encodingJobKey = stoll(encodingJobKeyIt->second);
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

        bool startAndEndEncodingDatePresent = false;
        string startEncodingDate;
        string endEncodingDate;
        auto startEncodingDateIt = queryParameters.find("startEncodingDate");
        auto endEncodingDateIt = queryParameters.find("endEncodingDate");
        if (startEncodingDateIt != queryParameters.end() && endEncodingDateIt != queryParameters.end())
        {
            startEncodingDate = startEncodingDateIt->second;
            endEncodingDate = endEncodingDateIt->second;
            
            startAndEndEncodingDatePresent = true;
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

        string type = "";
        auto typeIt = queryParameters.find("type");
        if (typeIt != queryParameters.end() && typeIt->second != "")
        {
            type = typeIt->second;
        }

        {
            Json::Value encodingStatusRoot = _mmsEngineDBFacade->getEncodingJobsStatus(
                    workspace, encodingJobKey,
                    start, rows,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    startAndEndEncodingDatePresent, startEncodingDate, endEncodingDate,
                    asc, status, type
                    );

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encodingStatusRoot);
            
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

void API::encodingJobPriority(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "encodingJobPriority";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t encodingJobKey = -1;
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt != queryParameters.end() && encodingJobKeyIt->second != "")
        {
            encodingJobKey = stoll(encodingJobKeyIt->second);
        }

        MMSEngineDBFacade::EncodingPriority newEncodingJobPriority;
        bool newEncodingJobPriorityPresent = false;
        auto newEncodingJobPriorityCodeIt = queryParameters.find("newEncodingJobPriorityCode");
        if (newEncodingJobPriorityCodeIt != queryParameters.end() && newEncodingJobPriorityCodeIt->second != "")
        {
            newEncodingJobPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(stoll(newEncodingJobPriorityCodeIt->second));
            newEncodingJobPriorityPresent = true;
        }

        bool tryEncodingAgain = false;
        auto tryEncodingAgainIt = queryParameters.find("tryEncodingAgain");
        if (tryEncodingAgainIt != queryParameters.end())
        {
            if (tryEncodingAgainIt->second == "false")
                tryEncodingAgain = false;
            else
                tryEncodingAgain = true;
        }

        {
            if (newEncodingJobPriorityPresent)
            {
                _mmsEngineDBFacade->updateEncodingJobPriority(
                    workspace, encodingJobKey, newEncodingJobPriority);
            }
            
            if (tryEncodingAgain)
            {
                _mmsEngineDBFacade->updateEncodingJobTryAgain(
                    workspace, encodingJobKey);
            }
            
            if (!newEncodingJobPriorityPresent && !tryEncodingAgain)
            {
                _logger->warn(__FILEREF__ + "Useless API call, no encoding update was done"
                    + ", newEncodingJobPriorityPresent: " + to_string(newEncodingJobPriorityPresent)
                    + ", tryEncodingAgain: " + to_string(tryEncodingAgain)
                );
            }

            string responseBody;
            
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

void API::killOrCancelEncodingJob(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "killOrCancelEncodingJob";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t encodingJobKey = -1;
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt != queryParameters.end() && encodingJobKeyIt->second != "")
        {
            encodingJobKey = stoll(encodingJobKeyIt->second);
        }

        {
			tuple<int64_t, string, string, MMSEngineDBFacade::EncodingStatus, bool, bool,
				string, MMSEngineDBFacade::EncodingStatus, int64_t> encodingJobDetails =
				_mmsEngineDBFacade->getEncodingJobDetails(encodingJobKey);

			int64_t ingestionJobKey;
			string type;
			string transcoder;
			MMSEngineDBFacade::EncodingStatus status;
			bool highAvailability;
			bool main;
			int64_t theOtherEncodingJobKey;
			string theOtherTranscoder;
			MMSEngineDBFacade::EncodingStatus theOtherStatus;

			tie(ingestionJobKey, type, transcoder, status, highAvailability, main, theOtherTranscoder,
					theOtherStatus, theOtherEncodingJobKey) = encodingJobDetails;

			_logger->info(__FILEREF__ + "getEncodingJobDetails"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", type: " + type
				+ ", transcoder: " + transcoder
				+ ", status: " + MMSEngineDBFacade::toString(status)
				+ ", highAvailability: " + to_string(highAvailability)
				+ ", main: " + to_string(main)
				+ ", theOtherTranscoder: " + theOtherTranscoder
				+ ", theOtherStatus: " + MMSEngineDBFacade::toString(theOtherStatus)
				+ ", theOtherEncodingJobKey: " + to_string(theOtherEncodingJobKey)
			);

			if (type == "LiveRecorder")
			{
				if (highAvailability)
				{
					// first has to be killed the main encodingJob, it updates the encoder status
					// later the other encodingJob
					if (main)
					{
						if (status == MMSEngineDBFacade::EncodingStatus::Processing)
						{
							_logger->info(__FILEREF__ + "killEncodingJob"
								+ ", transcoder: " + transcoder
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							);

							try
							{
								killEncodingJob(transcoder, encodingJobKey);
							}
							catch (...)
							{
								_logger->info(__FILEREF__ + "killEncodingJob failed, force update of the status"
									+ ", transcoder: " + transcoder
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
								);

								{
									_logger->info(__FILEREF__ + "updateEncodingJob KilledByUser"                                
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)                         
										+ ", encodingJobKey: " + to_string(encodingJobKey)                           
										+ ", main: " + to_string(main)                                                                
									);
									// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
									// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
									// 'no update is done'
									int64_t mediaItemKey = -1;
									int64_t encodedPhysicalPathKey = -1;
									_mmsEngineDBFacade->updateEncodingJob (encodingJobKey,
										MMSEngineDBFacade::EncodingError::KilledByUser,
										mediaItemKey, encodedPhysicalPathKey,
										main ? ingestionJobKey : -1,
										"killEncodingJob failed");
								}
							}
						}

						if (theOtherStatus == MMSEngineDBFacade::EncodingStatus::Processing)
						{
							_logger->info(__FILEREF__ + "killEncodingJob"
								+ ", transcoder: " + theOtherTranscoder
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(theOtherEncodingJobKey)
							);

							try
							{
								killEncodingJob(theOtherTranscoder, theOtherEncodingJobKey);
							}
							catch (...)
							{
								_logger->info(__FILEREF__ + "killEncodingJob failed, force update of the status"
									+ ", transcoder: " + transcoder
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(theOtherEncodingJobKey)
								);

								{
									_logger->info(__FILEREF__ + "updateEncodingJob KilledByUser"                                
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)                         
										+ ", encodingJobKey: " + to_string(theOtherEncodingJobKey)                           
										+ ", main: " + to_string(main)
									);
									// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
									// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
									// 'no update is done'
									int64_t mediaItemKey = -1;
									int64_t encodedPhysicalPathKey = -1;
									_mmsEngineDBFacade->updateEncodingJob (theOtherEncodingJobKey,
										MMSEngineDBFacade::EncodingError::KilledByUser,
										mediaItemKey, encodedPhysicalPathKey,
										main ? ingestionJobKey : -1,
										"killEncodingJob failed");
								}
							}
						}
					}
					else
					{
						if (theOtherStatus == MMSEngineDBFacade::EncodingStatus::Processing)
						{
							_logger->info(__FILEREF__ + "killEncodingJob"
								+ ", transcoder: " + theOtherTranscoder
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(theOtherEncodingJobKey)
							);

							try
							{
								killEncodingJob(theOtherTranscoder, theOtherEncodingJobKey);
							}
							catch (...)
							{
								_logger->info(__FILEREF__ + "killEncodingJob failed, force update of the status"
									+ ", transcoder: " + theOtherTranscoder
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(theOtherEncodingJobKey)
								);

								{
									_logger->info(__FILEREF__ + "updateEncodingJob KilledByUser"                                
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)                         
										+ ", encodingJobKey: " + to_string(theOtherEncodingJobKey)                           
										+ ", main: " + to_string(main)
									);
									// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
									// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
									// 'no update is done'
									int64_t mediaItemKey = -1;
									int64_t encodedPhysicalPathKey = -1;
									_mmsEngineDBFacade->updateEncodingJob (theOtherEncodingJobKey,
										MMSEngineDBFacade::EncodingError::KilledByUser,
										mediaItemKey, encodedPhysicalPathKey,
										main ? ingestionJobKey : -1,
										"killEncodingJob failed");
								}
							}
						}

						if (status == MMSEngineDBFacade::EncodingStatus::Processing)
						{
							_logger->info(__FILEREF__ + "killEncodingJob"
								+ ", transcoder: " + transcoder
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							);

							try
							{
								killEncodingJob(transcoder, encodingJobKey);
							}
							catch (...)
							{
								_logger->info(__FILEREF__ + "killEncodingJob failed, force update of the status"
									+ ", transcoder: " + transcoder
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", encodingJobKey: " + to_string(encodingJobKey)
								);

								{
									_logger->info(__FILEREF__ + "updateEncodingJob KilledByUser"                                
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)                         
										+ ", encodingJobKey: " + to_string(encodingJobKey)                           
										+ ", main: " + to_string(main)
									);
									// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
									// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
									// 'no update is done'
									int64_t mediaItemKey = -1;
									int64_t encodedPhysicalPathKey = -1;
									_mmsEngineDBFacade->updateEncodingJob (encodingJobKey,
										MMSEngineDBFacade::EncodingError::KilledByUser,
										mediaItemKey, encodedPhysicalPathKey,
										main ? ingestionJobKey : -1,
										"killEncodingJob failed");
								}
							}
						}
					}
				}
				else
				{
					if (status == MMSEngineDBFacade::EncodingStatus::Processing)
					{
						_logger->info(__FILEREF__ + "killEncodingJob"
							+ ", transcoder: " + transcoder
							+ ", encodingJobKey: " + to_string(encodingJobKey)
						);

						try
						{
							killEncodingJob(transcoder, encodingJobKey);
						}
						catch (...)
						{
							_logger->info(__FILEREF__ + "killEncodingJob failed, force update of the status"
								+ ", transcoder: " + transcoder
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
							);

							{
								_logger->info(__FILEREF__ + "updateEncodingJob KilledByUser"                                
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)                         
									+ ", encodingJobKey: " + to_string(encodingJobKey)                           
								);

								int64_t mediaItemKey = -1;
								int64_t encodedPhysicalPathKey = -1;
								_mmsEngineDBFacade->updateEncodingJob (encodingJobKey,
									MMSEngineDBFacade::EncodingError::KilledByUser,
									mediaItemKey, encodedPhysicalPathKey,
									ingestionJobKey, "killEncodingJob failed");
							}
						}
					}
				}
			}
			else if (type == "LiveProxy")
			{
				if (status == MMSEngineDBFacade::EncodingStatus::Processing)
				{
					// In this case we may have 2 scenarios:
					// 1. process (ffmpeg) is running
					// 2. process (ffmpeg) fails to run and we have the Task in the loop
					//		within EncoderVideoAudioProxy trying to make ffmpeg starting calling the Transcoder.
					//
					// In case 1, the below killEncodingJob works fine and this is the solution
					// In case 2, killEncodingJob will fail because there is no ffmpeg process running.
					//		For this reason we call updateEncodingJobFailuresNumber and set failuresNumber
					//		to a negative value. This is a 'aggreement' with EncoderVideoAudioProxy making
					//		the EncoderVideoAudioProxy thread to terminate his loop 

					try
					{
						_logger->info(__FILEREF__ + "killEncodingJob"
							+ ", transcoder: " + transcoder
							+ ", encodingJobKey: " + to_string(encodingJobKey)
						);
						killEncodingJob(transcoder, encodingJobKey);
					}
					catch(runtime_error e)
					{
						// this is the case 2
						long newFailuresNumber = -100;

						_logger->info(__FILEREF__ + "Making FailuresNumber negative"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", newFailuresNumber: " + to_string(newFailuresNumber)
						);
						_mmsEngineDBFacade->updateEncodingJobFailuresNumber(
							encodingJobKey, newFailuresNumber);
					}
				}
				else if (status == MMSEngineDBFacade::EncodingStatus::ToBeProcessed)
				{
					MMSEngineDBFacade::EncodingError encodingError
						= MMSEngineDBFacade::EncodingError::CanceledByUser;
					int64_t mediaItemKey = 0;
					int64_t encodedPhysicalPathKey = 0;
					_mmsEngineDBFacade->updateEncodingJob(
							encodingJobKey, encodingError, mediaItemKey, encodedPhysicalPathKey,
							ingestionJobKey, "Canceled By User");
				}
			}
			else
			{
				if (status == MMSEngineDBFacade::EncodingStatus::Processing)
				{
					_logger->info(__FILEREF__ + "killEncodingJob"
						+ ", transcoder: " + transcoder
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					);
					killEncodingJob(transcoder, encodingJobKey);
				}
				else if (status == MMSEngineDBFacade::EncodingStatus::ToBeProcessed)
				{
					MMSEngineDBFacade::EncodingError encodingError
						= MMSEngineDBFacade::EncodingError::CanceledByUser;
					int64_t mediaItemKey = 0;
					int64_t encodedPhysicalPathKey = 0;
					_mmsEngineDBFacade->updateEncodingJob(
							encodingJobKey, encodingError, mediaItemKey, encodedPhysicalPathKey,
							ingestionJobKey, "Canceled By User");
				}
			}

            string responseBody;
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

void API::encodingProfilesSetsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "encodingProfilesSetsList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        int64_t encodingProfilesSetKey = -1;
        auto encodingProfilesSetKeyIt = queryParameters.find("encodingProfilesSetKey");
        if (encodingProfilesSetKeyIt != queryParameters.end() && encodingProfilesSetKeyIt->second != "")
        {
            encodingProfilesSetKey = stoll(encodingProfilesSetKeyIt->second);
        }

        bool contentTypePresent = false;
        MMSEngineDBFacade::ContentType contentType;
        auto contentTypeIt = queryParameters.find("contentType");
        if (contentTypeIt != queryParameters.end() && contentTypeIt->second != "")
        {
            contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);
            
            contentTypePresent = true;
        }
        
        {
            
            Json::Value encodingProfilesSetListRoot = _mmsEngineDBFacade->getEncodingProfilesSetList(
                    workspace->_workspaceKey, encodingProfilesSetKey,
                    contentTypePresent, contentType);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encodingProfilesSetListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::encodingProfilesList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "encodingProfilesList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        int64_t encodingProfileKey = -1;
        auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
        if (encodingProfileKeyIt != queryParameters.end() && encodingProfileKeyIt->second != "")
        {
            encodingProfileKey = stoll(encodingProfileKeyIt->second);
        }

        bool contentTypePresent = false;
        MMSEngineDBFacade::ContentType contentType;
        auto contentTypeIt = queryParameters.find("contentType");
        if (contentTypeIt != queryParameters.end() && contentTypeIt->second != "")
        {
            contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);
            
            contentTypePresent = true;
        }
        
        {
            Json::Value encodingProfileListRoot = _mmsEngineDBFacade->getEncodingProfileList(
                    workspace->_workspaceKey, encodingProfileKey,
                    contentTypePresent, contentType);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encodingProfileListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::addEncodingProfilesSet(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addEncodingProfilesSet";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        auto sContentTypeIt = queryParameters.find("contentType");
        if (sContentTypeIt == queryParameters.end())
        {
            string errorMessage = string("'contentType' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        MMSEngineDBFacade::ContentType contentType = 
                MMSEngineDBFacade::toContentType(sContentTypeIt->second);
        
        Json::Value encodingProfilesSetRoot;
        try
        {
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &encodingProfilesSetRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                            + ", errors: " + errors
                            + ", requestBody: " + requestBody
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
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

        try
        {            
            conn = _mmsEngineDBFacade->beginIngestionJobs();

            Validator validator(_logger, _mmsEngineDBFacade, _configuration);
            validator.validateEncodingProfilesSetRootMetadata(contentType, encodingProfilesSetRoot);
        
            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfilesSetRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string label = encodingProfilesSetRoot.get(field, "XXX").asString();
                        
            int64_t encodingProfilesSetKey = _mmsEngineDBFacade->addEncodingProfilesSet(conn,
                    workspace->_workspaceKey, contentType, label);
            
            field = "Profiles";
            Json::Value profilesRoot = encodingProfilesSetRoot[field];

            for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
            {
                string profileLabel = profilesRoot[profileIndex].asString();
                
                int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfileIntoSet(
                        conn, workspace->_workspaceKey, profileLabel,
                        contentType, encodingProfilesSetKey);

                if (responseBody != "")
                    responseBody += string(", ");
                responseBody += (
                        string("{ ") 
                        + "\"encodingProfileKey\": " + to_string(encodingProfileKey)
                        + ", \"label\": \"" + profileLabel + "\" "
                        + "}"
                        );
            }
            
            /*            
            if (_mmsEngineDBFacade->isMetadataPresent(encodingProfilesSetRoot, field))
            {
                Json::Value profilesRoot = encodingProfilesSetRoot[field];

                for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
                {
                    Json::Value profileRoot = profilesRoot[profileIndex];

                    string field = "Label";
                    if (!_mmsEngineDBFacade->isMetadataPresent(profileRoot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    string profileLabel = profileRoot.get(field, "XXX").asString();
            
                    MMSEngineDBFacade::EncodingTechnology encodingTechnology;
                    
                    if (contentType == MMSEngineDBFacade::ContentType::Image)
                        encodingTechnology = MMSEngineDBFacade::EncodingTechnology::Image;
                    else
                        encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;
                       
                    string jsonProfile;
                    {
                        Json::StreamWriterBuilder wbuilder;

                        jsonProfile = Json::writeString(wbuilder, profileRoot);
                    }
                       
                    int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfile(
                        conn, workspace->_workspaceKey, profileLabel,
                        contentType, encodingTechnology, jsonProfile,
                        encodingProfilesSetKey);
                    
                    if (responseBody != "")
                        responseBody += string(", ");
                    responseBody += (
                            string("{ ") 
                            + "\"encodingProfileKey\": " + to_string(encodingProfileKey)
                            + ", \"label\": \"" + profileLabel + "\" "
                            + "}"
                            );
                }
            }
            */
            
            bool commit = true;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);
            
            string beginOfResponseBody = string("{ ")
                + "\"encodingProfilesSet\": { "
                    + "\"encodingProfilesSetKey\": " + to_string(encodingProfilesSetKey)
                    + ", \"label\": \"" + label + "\" "
                    + "}, "
                    + "\"profiles\": [ ";
            responseBody.insert(0, beginOfResponseBody);
            responseBody += " ] }";
        }
        catch(runtime_error e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);
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

void API::addEncodingProfile(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addEncodingProfile";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        auto sContentTypeIt = queryParameters.find("contentType");
        if (sContentTypeIt == queryParameters.end())
        {
            string errorMessage = string("'contentType' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        MMSEngineDBFacade::ContentType contentType = 
                MMSEngineDBFacade::toContentType(sContentTypeIt->second);
        
        Json::Value encodingProfileRoot;
        try
        {
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &encodingProfileRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                            + ", errors: " + errors
                            + ", requestBody: " + requestBody
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
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

        try
        {
            Validator validator(_logger, _mmsEngineDBFacade, _configuration);
            validator.validateEncodingProfileRootMetadata(contentType, encodingProfileRoot);

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string profileLabel = encodingProfileRoot.get(field, "XXX").asString();

            MMSEngineDBFacade::EncodingTechnology encodingTechnology;

            if (contentType == MMSEngineDBFacade::ContentType::Image)
                encodingTechnology = MMSEngineDBFacade::EncodingTechnology::Image;
            else
                encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;

            string jsonEncodingProfile;
            {
                Json::StreamWriterBuilder wbuilder;

                jsonEncodingProfile = Json::writeString(wbuilder, encodingProfileRoot);
            }
            
            int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfile(
                workspace->_workspaceKey, profileLabel,
                contentType, encodingTechnology, jsonEncodingProfile);

            responseBody = (
                    string("{ ") 
                    + "\"encodingProfileKey\": " + to_string(encodingProfileKey)
                    + ", \"label\": \"" + profileLabel + "\" "
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);
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

void API::removeEncodingProfile(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEncodingProfile";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
        if (encodingProfileKeyIt == queryParameters.end())
        {
            string errorMessage = string("'encodingProfileKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t encodingProfileKey = stoll(encodingProfileKeyIt->second);
        
        try
        {
            _mmsEngineDBFacade->removeEncodingProfile(
                workspace->_workspaceKey, encodingProfileKey);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        string responseBody;
        
        sendSuccess(request, 200, responseBody);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::removeEncodingProfilesSet(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEncodingProfilesSet";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto encodingProfilesSetKeyIt = queryParameters.find("encodingProfilesSetKey");
        if (encodingProfilesSetKeyIt == queryParameters.end())
        {
            string errorMessage = string("'encodingProfilesSetKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t encodingProfilesSetKey = stoll(encodingProfilesSetKeyIt->second);
        
        try
        {
            _mmsEngineDBFacade->removeEncodingProfilesSet(
                workspace->_workspaceKey, encodingProfilesSetKey);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfilesSet failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfilesSet failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        string responseBody;
        
        sendSuccess(request, 200, responseBody);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::killEncodingJob(string transcoderHost, int64_t encodingJobKey)
{
	string ffmpegEncoderURL;
	ostringstream response;
	try
	{
		ffmpegEncoderURL = _ffmpegEncoderProtocol
			+ "://"
			+ transcoderHost + ":"
			+ to_string(_ffmpegEncoderPort)
			+ _ffmpegEncoderKillEncodingURI
			+ "/" + to_string(encodingJobKey)
		;
            
		list<string> header;

		{
			string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
			string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

			header.push_back(basicAuthorization);
		}
            
		curlpp::Cleanup cleaner;
		curlpp::Easy request;

		// Setting the URL to retrive.
		request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));
		request.setOpt(new curlpp::options::CustomRequest("DELETE"));

		if (_ffmpegEncoderProtocol == "https")
		{
			/*
                  typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                  typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                  typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                  typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                  typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                  typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                  typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                  typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                  typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                  typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                  typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                  typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                  typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
			*/
                                                                                                
              
			/*
			// cert is stored PEM coded in file... 
			// since PEM is default, we needn't set it for PEM 
			// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
			equest.setOpt(sslCertType);

			// set the cert for client authentication
			// "testcert.pem"
			// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
			curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
			request.setOpt(sslCert);
			*/

			/*
			// sorry, for engine we must set the passphrase
			//   (if the key has one...)
			// const char *pPassphrase = NULL;
			if(pPassphrase)
			curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

			// if we use a key stored in a crypto engine,
			//   we must set the key type to "ENG"
			// pKeyType  = "PEM";
			curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

			// set the private key (file or ID in engine)
			// pKeyName  = "testkey.pem";
			curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

			// set the file with the certs vaildating the server
			// *pCACertFile = "cacert.pem";
			curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
			*/
              
			// disconnect if we can't validate server's cert
			bool bSslVerifyPeer = false;
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
			request.setOpt(sslVerifyPeer);
              
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
			request.setOpt(sslVerifyHost);
              
			// request.setOpt(new curlpp::options::SslEngineDefault());                                              

		}

		request.setOpt(new curlpp::options::HttpHeader(header));

		request.setOpt(new curlpp::options::WriteStream(&response));

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "killEncodingJob"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
		);
		request.perform();
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "killEncodingJob"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", response.str: " + response.str()
		);

		string sResponse = response.str();

		// LF and CR create problems to the json parser...                                                    
		while (sResponse.back() == 10 || sResponse.back() == 13)                                              
			sResponse.pop_back();                                                                             

		{
			string message = __FILEREF__ + "Kill encoding response"                                       
				+ ", encodingJobKey: " + to_string(encodingJobKey)          
				+ ", sResponse: " + sResponse                                                                 
			;                                                                                             
			_logger->info(message);
		}

		long responseCode = curlpp::infos::ResponseCode::get(request);                                        
		if (responseCode != 200)
		{
			string errorMessage = __FILEREF__ + "Kill encoding URL failed"                                       
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", sResponse: " + sResponse                                                                 
				+ ", responseCode: " + to_string(responseCode)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (LogicError)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{ 
		string errorMessage = string("killEncoding URL failed (RuntimeError)")
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		;
          
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
}

