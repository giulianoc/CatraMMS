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
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "catralibraries/PostgresConnection.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <regex>
#include <sstream>

void API::encodingJobsStatus(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "encodingJobsStatus";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
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
			{
				// 2022-02-13: changed to return an error otherwise the user
				//	think to ask for a huge number of items while the return is much less

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
		}

		string startIngestionDate;
		auto startIngestionDateIt = queryParameters.find("startIngestionDate");
		if (startIngestionDateIt != queryParameters.end())
			startIngestionDate = startIngestionDateIt->second;

		string endIngestionDate;
		auto endIngestionDateIt = queryParameters.find("endIngestionDate");
		if (endIngestionDateIt != queryParameters.end())
			endIngestionDate = endIngestionDateIt->second;

		string startEncodingDate;
		auto startEncodingDateIt = queryParameters.find("startEncodingDate");
		if (startEncodingDateIt != queryParameters.end())
			startEncodingDate = startEncodingDateIt->second;

		string endEncodingDate;
		auto endEncodingDateIt = queryParameters.find("endEncodingDate");
		if (endEncodingDateIt != queryParameters.end())
			endEncodingDate = endEncodingDateIt->second;

		int64_t encoderKey = -1;
		auto encoderKeyIt = queryParameters.find("encoderKey");
		if (encoderKeyIt != queryParameters.end() && encoderKeyIt->second != "")
		{
			encoderKey = stoll(encoderKeyIt->second);
		}

		bool alsoEncodingJobsFromOtherWorkspaces = false;
		auto alsoEncodingJobsFromOtherWorkspacesIt = queryParameters.find("alsoEncodingJobsFromOtherWorkspaces");
		if (alsoEncodingJobsFromOtherWorkspacesIt != queryParameters.end() && alsoEncodingJobsFromOtherWorkspacesIt->second != "")
		{
			if (alsoEncodingJobsFromOtherWorkspacesIt->second == "true")
				alsoEncodingJobsFromOtherWorkspaces = true;
			else
				alsoEncodingJobsFromOtherWorkspaces = false;
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

		string types = "";
		auto typesIt = queryParameters.find("types");
		if (typesIt != queryParameters.end() && typesIt->second != "")
		{
			types = typesIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(types, regex(plus), plusDecoded);

			types = CurlWrapper::unescape(firstDecoding);
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
			json encodingStatusRoot = _mmsEngineDBFacade->getEncodingJobsStatus(
				workspace, encodingJobKey, start, rows,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate,
				// startAndEndEncodingDatePresent,
				startEncodingDate, endEncodingDate, encoderKey, alsoEncodingJobsFromOtherWorkspaces, asc, status, types, fromMaster
			);

			string responseBody = JSONUtils::toString(encodingStatusRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::encodingJobPriority(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "encodingJobPriority";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
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
				_mmsEngineDBFacade->updateEncodingJobPriority(workspace, encodingJobKey, newEncodingJobPriority);
			}

			if (tryEncodingAgain)
			{
				_mmsEngineDBFacade->updateEncodingJobTryAgain(workspace, encodingJobKey);
			}

			if (!newEncodingJobPriorityPresent && !tryEncodingAgain)
			{
				SPDLOG_WARN(
					"Useless API call, no encoding update was done"
					", newEncodingJobPriorityPresent: {}"
					", tryEncodingAgain: {}",
					newEncodingJobPriorityPresent, tryEncodingAgain
				);
			}

			string responseBody;

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::killOrCancelEncodingJob(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "killOrCancelEncodingJob";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
	);

	try
	{
		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), false);
		/*
		int64_t encodingJobKey = -1;
		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt != queryParameters.end() && encodingJobKeyIt->second != "")
			encodingJobKey = stoll(encodingJobKeyIt->second);
		*/

		// killType: "kill", "restartWithinEncoder", "killToRestartByEngine"
		string killType = getQueryParameter(queryParameters, "killType", string("kill"), false);
		/*
		bool lightKill = false;
		auto lightKillIt = queryParameters.find("lightKill");
		if (lightKillIt != queryParameters.end() && lightKillIt->second != "")
			lightKill = lightKillIt->second == "true" ? true : false;
		*/

		{
			// 2022-12-18: fromMaster true perchè serve una info sicura
			auto [ingestionJobKey, type, encoderKey, status] =
				_mmsEngineDBFacade->encodingJob_IngestionJobKeyTypeEncoderKeyStatus(encodingJobKey, true);

			SPDLOG_INFO(
				"encodingJob_IngestionJobKeyTypeEncoderKeyStatus"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", killType: {}"
				", type: {}"
				", encoderKey: {}"
				", status: {}",
				ingestionJobKey, encodingJobKey, killType, type, encoderKey, MMSEngineDBFacade::toString(status)
			);

			if (type == "LiveRecorder")
			{
				{
					if (status == MMSEngineDBFacade::EncodingStatus::Processing)
					{
						// In this case we may have 3 scenarios:
						// 1. process (ffmpeg) is running
						// 2. process (ffmpeg) fails to run and we have the Task in the loop
						//		within EncoderVideoAudioProxy trying to make ffmpeg starting calling the Transcoder.
						// 3. process (ffmpeg) is not running, EncoderVideoAudioProxy is not managing the EncodingJob
						//		and the status is Processing
						//
						// In case 1, the below killEncodingJob works fine and this is the solution
						// In case 2, killEncodingJob will fail because there is no ffmpeg process running.
						//		For this reason we call updateEncodingJobIsKilled and set isKilled
						//		to true. This is an 'aggreement' with EncoderVideoAudioProxy making
						//		the EncoderVideoAudioProxy thread to terminate his loop
						// In case 3, the EncodingJob is updated to KilledByUser
						try
						{
							SPDLOG_INFO(
								"killEncodingJob"
								", encoderKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", killType: {}",
								encoderKey, ingestionJobKey, encodingJobKey, killType
							);
							killEncodingJob(encoderKey, ingestionJobKey, encodingJobKey, killType);

							// to make sure EncoderVideoProxyThread resources are released,
							// the isKilled flag is also set
							if (killType == "kill")
							{
								// this is the case 2
								bool isKilled = true;

								SPDLOG_INFO(
									"Setting isKilled flag"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", isKilled: {}",
									ingestionJobKey, encodingJobKey, isKilled
								);
								_mmsEngineDBFacade->updateEncodingJobIsKilled(encodingJobKey, isKilled);

								SPDLOG_INFO(
									"sleeping"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", _intervalInSecondsToCheckEncodingFinished: {}",
									ingestionJobKey, encodingJobKey, _intervalInSecondsToCheckEncodingFinished
								);
								// case 3: in case updateEncodingJobIsKilled did not work, we are in scenario 3
								//		To check that updateEncodingJobIsKilled did not work, we will sleep and check again the status
								this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
								;

								// 2022-12-18: fromMaster true perchè serve una info sicura
								tie(ingestionJobKey, type, encoderKey, status) =
									_mmsEngineDBFacade->encodingJob_IngestionJobKeyTypeEncoderKeyStatus(encodingJobKey, true);

								SPDLOG_INFO(
									"encodingJob_IngestionJobKeyTypeEncoderKeyStatus"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", type: {}"
									", encoderKey: {}"
									", status: {}",
									ingestionJobKey, encodingJobKey, type, encoderKey, MMSEngineDBFacade::toString(status)
								);

								if (status == MMSEngineDBFacade::EncodingStatus::Processing)
								{
									SPDLOG_INFO(
										"killEncodingJob and updateEncodingJobIsKilled failed, force update of the status"
										", encoderKey: {}"
										", ingestionJobKey: {}"
										", encodingJobKey: {}",
										encoderKey, ingestionJobKey, encodingJobKey
									);

									{
										SPDLOG_INFO(
											"updateEncodingJob KilledByUser"
											", ingestionJobKey: {}"
											", encodingJobKey: {}",
											ingestionJobKey, encodingJobKey
										);

										_mmsEngineDBFacade->updateEncodingJob(
											encodingJobKey, MMSEngineDBFacade::EncodingError::KilledByUser,
											false, // isIngestionJobFinished: this field is not used by updateEncodingJob
											ingestionJobKey, "killEncodingJob failed"
										);
									}
								}
							}
						}
						catch (...)
						{
							// this is the case 2
							if (killType == "kill")
							{
								bool isKilled = true;

								SPDLOG_INFO(
									"Setting isKilled flag"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", isKilled: {}",
									ingestionJobKey, encodingJobKey, isKilled
								);
								_mmsEngineDBFacade->updateEncodingJobIsKilled(encodingJobKey, isKilled);

								SPDLOG_INFO(
									"sleeping"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", _intervalInSecondsToCheckEncodingFinished: {}",
									ingestionJobKey, encodingJobKey, _intervalInSecondsToCheckEncodingFinished
								);
								// case 3: in case updateEncodingJobIsKilled did not work, we are in scenario 3
								//		To check that updateEncodingJobIsKilled did not work, we will sleep and check again the status
								this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
								;

								// 2022-12-18: fromMaster true perchè serve una info sicura
								tie(ingestionJobKey, type, encoderKey, status) =
									_mmsEngineDBFacade->encodingJob_IngestionJobKeyTypeEncoderKeyStatus(encodingJobKey, true);

								SPDLOG_INFO(
									"encodingJob_IngestionJobKeyTypeEncoderKeyStatus"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", type: {}"
									", encoderKey: {}"
									", status: {}",
									ingestionJobKey, encodingJobKey, type, encoderKey, MMSEngineDBFacade::toString(status)
								);

								if (status == MMSEngineDBFacade::EncodingStatus::Processing)
								{
									SPDLOG_INFO(
										"killEncodingJob and updateEncodingJobIsKilled failed, force update of the status"
										", encoderKey: {}"
										", ingestionJobKey: {}"
										", encodingJobKey: {}",
										encoderKey, ingestionJobKey, encodingJobKey
									);

									{
										SPDLOG_INFO(
											"updateEncodingJob KilledByUser"
											", ingestionJobKey: {}"
											", encodingJobKey: {}",
											ingestionJobKey, encodingJobKey
										);

										_mmsEngineDBFacade->updateEncodingJob(
											encodingJobKey, MMSEngineDBFacade::EncodingError::KilledByUser,
											false, // isIngestionJobFinished: this field is not used by updateEncodingJob
											ingestionJobKey, "killEncodingJob failed"
										);
									}
								}
							}
						}
					}
				}
			}
			else if (type == "LiveProxy" || type == "VODProxy" || type == "Countdown")
			{
				if (status == MMSEngineDBFacade::EncodingStatus::Processing)
				{
					// In this case we may have 3 scenarios:
					// 1. process (ffmpeg) is running
					// 2. process (ffmpeg) fails to run and we have the Task in the loop
					//		within EncoderVideoAudioProxy trying to make ffmpeg starting calling the Transcoder.
					// 3. process (ffmpeg) is not running, EncoderVideoAudioProxy is not managing the EncodingJob
					//		and the status is Processing
					//
					// In case 1, the below killEncodingJob works fine and this is the solution
					// In case 2, killEncodingJob will fail because there is no ffmpeg process running.
					//		For this reason we call updateEncodingJobIsKilled and set isKilled
					//		to true. This is an 'aggreement' with EncoderVideoAudioProxy making
					//		the EncoderVideoAudioProxy thread to terminate his loop
					// In case 3, the EncodingJob is updated to KilledByUser

					try
					{
						SPDLOG_INFO(
							"killEncodingJob"
							", encoderKey: {}"
							", ingestionJobKey: {}"
							", encodingJobKey: {}",
							encoderKey, ingestionJobKey, encodingJobKey
						);
						killEncodingJob(encoderKey, ingestionJobKey, encodingJobKey, killType);

						// to make sure EncoderVideoProxyThread resources are released,
						// the isKilled flag is also set
						if (killType == "kill")
						{
							// this is the case 2
							bool isKilled = true;

							SPDLOG_INFO(
								"Setting isKilled flag"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", isKilled: {}",
								ingestionJobKey, encodingJobKey, isKilled
							);
							_mmsEngineDBFacade->updateEncodingJobIsKilled(encodingJobKey, isKilled);

							SPDLOG_INFO(
								"sleeping"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", _intervalInSecondsToCheckEncodingFinished: {}",
								ingestionJobKey, encodingJobKey, _intervalInSecondsToCheckEncodingFinished
							);
							// case 3: in case updateEncodingJobIsKilled did not work, we are in scenario 3
							//		To check that updateEncodingJobIsKilled did not work, we will sleep and check again the status
							this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
							;

							// 2022-12-18: fromMaster true perchè serve una info sicura
							tie(ingestionJobKey, type, encoderKey, status) =
								_mmsEngineDBFacade->encodingJob_IngestionJobKeyTypeEncoderKeyStatus(encodingJobKey, true);

							SPDLOG_INFO(
								"encodingJob_IngestionJobKeyTypeEncoderKeyStatus"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", type: {}"
								", encoderKey: {}"
								", status: {}",
								ingestionJobKey, encodingJobKey, type, encoderKey, MMSEngineDBFacade::toString(status)
							);

							if (status == MMSEngineDBFacade::EncodingStatus::Processing)
							{
								SPDLOG_INFO(
									"killEncodingJob and updateEncodingJobIsKilled failed, force update of the status"
									", encoderKey: {}"
									", ingestionJobKey: {}"
									", encodingJobKey: {}",
									encoderKey, ingestionJobKey, encodingJobKey
								);

								{
									SPDLOG_INFO(
										"updateEncodingJob KilledByUser"
										", ingestionJobKey: {}"
										", encodingJobKey: {}",
										ingestionJobKey, encodingJobKey
									);

									_mmsEngineDBFacade->updateEncodingJob(
										encodingJobKey, MMSEngineDBFacade::EncodingError::KilledByUser,
										false, // isIngestionJobFinished: this field is not used by updateEncodingJob
										ingestionJobKey, "killEncodingJob failed"
									);
								}
							}
						}
					}
					catch (runtime_error &e)
					{
						// this is the case 2
						if (killType == "kill")
						{
							bool isKilled = true;

							SPDLOG_INFO(
								"Setting isKilled flag"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", isKilled: {}",
								ingestionJobKey, encodingJobKey, isKilled
							);
							_mmsEngineDBFacade->updateEncodingJobIsKilled(encodingJobKey, isKilled);

							SPDLOG_INFO(
								"sleeping"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", _intervalInSecondsToCheckEncodingFinished: {}",
								ingestionJobKey, encodingJobKey, _intervalInSecondsToCheckEncodingFinished
							);
							// case 3: in case updateEncodingJobIsKilled did not work, we are in scenario 3
							//		To check that updateEncodingJobIsKilled did not work, we will sleep and check again the status
							this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
							;

							// 2022-12-18: fromMaster true perchè serve una info sicura
							tie(ingestionJobKey, type, encoderKey, status) =
								_mmsEngineDBFacade->encodingJob_IngestionJobKeyTypeEncoderKeyStatus(encodingJobKey, true);

							SPDLOG_INFO(
								"encodingJob_IngestionJobKeyTypeEncoderKeyStatus"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", type: {}"
								", encoderKey: {}"
								", status: {}",
								ingestionJobKey, encodingJobKey, type, encoderKey, MMSEngineDBFacade::toString(status)
							);

							if (status == MMSEngineDBFacade::EncodingStatus::Processing)
							{
								SPDLOG_INFO(
									"killEncodingJob and updateEncodingJobIsKilled failed, force update of the status"
									", encoderKey: {}"
									", ingestionJobKey: {}"
									", encodingJobKey: {}",
									encoderKey, ingestionJobKey, encodingJobKey
								);

								{
									SPDLOG_INFO(
										"updateEncodingJob KilledByUser"
										", ingestionJobKey: {}"
										", encodingJobKey: {}",
										ingestionJobKey, encodingJobKey
									);

									_mmsEngineDBFacade->updateEncodingJob(
										encodingJobKey, MMSEngineDBFacade::EncodingError::KilledByUser,
										false, // isIngestionJobFinished: this field is not used by updateEncodingJob
										ingestionJobKey, "killEncodingJob failed"
									);
								}
							}
						}
					}
				}
				else if (status == MMSEngineDBFacade::EncodingStatus::ToBeProcessed)
				{
					if (killType == "kill")
					{
						MMSEngineDBFacade::EncodingError encodingError = MMSEngineDBFacade::EncodingError::CanceledByUser;
						_mmsEngineDBFacade->updateEncodingJob(
							encodingJobKey, encodingError,
							false, // isIngestionJobFinished: this field is not used by updateEncodingJob
							ingestionJobKey, "Canceled By User"
						);
					}
				}
			}
			else
			{
				if (status == MMSEngineDBFacade::EncodingStatus::Processing)
				{
					SPDLOG_INFO(
						"killEncodingJob"
						", encoderKey: {}"
						", ingestionJobKey: {}"
						", encodingJobKey: {}",
						encoderKey, ingestionJobKey, encodingJobKey
					);
					killEncodingJob(encoderKey, ingestionJobKey, encodingJobKey, killType);
				}
				else if (status == MMSEngineDBFacade::EncodingStatus::ToBeProcessed)
				{
					if (killType == "kill")
					{
						MMSEngineDBFacade::EncodingError encodingError = MMSEngineDBFacade::EncodingError::CanceledByUser;
						_mmsEngineDBFacade->updateEncodingJob(
							encodingJobKey, encodingError,
							false, // isIngestionJobFinished: this field is not used by updateEncodingJob
							ingestionJobKey, "Canceled By User"
						);
					}
				}
			}

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::encodingProfilesSetsList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "encodingProfilesSetsList";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, workspace->_workspaceKey
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

			json encodingProfilesSetListRoot =
				_mmsEngineDBFacade->getEncodingProfilesSetList(workspace->_workspaceKey, encodingProfilesSetKey, contentTypePresent, contentType);

			string responseBody = JSONUtils::toString(encodingProfilesSetListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::encodingProfilesList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "encodingProfilesList";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, workspace->_workspaceKey
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

		string label;
		auto labelIt = queryParameters.find("label");
		if (labelIt != queryParameters.end() && labelIt->second != "")
		{
			label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = CurlWrapper::unescape(firstDecoding);
		}

		{
			json encodingProfileListRoot =
				_mmsEngineDBFacade->getEncodingProfileList(workspace->_workspaceKey, encodingProfileKey, contentTypePresent, contentType, label);

			string responseBody = JSONUtils::toString(encodingProfileListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addUpdateEncodingProfilesSet(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addUpdateEncodingProfilesSet";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
	);

	try
	{
		auto sContentTypeIt = queryParameters.find("contentType");
		if (sContentTypeIt == queryParameters.end())
		{
			string errorMessage = "'contentType' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(sContentTypeIt->second);

		json encodingProfilesSetRoot = JSONUtils::toJson(requestBody);

		string responseBody;

		try
		{
			Validator validator(_mmsEngineDBFacade, _configurationRoot);
			validator.validateEncodingProfilesSetRootMetadata(contentType, encodingProfilesSetRoot);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(encodingProfilesSetRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string label = JSONUtils::asString(encodingProfilesSetRoot, field, "");

			bool removeEncodingProfilesIfPresent = true;
#ifdef __POSTGRES__
			int64_t encodingProfilesSetKey = _mmsEngineDBFacade->addEncodingProfilesSetIfNotAlreadyPresent(
				workspace->_workspaceKey, contentType, label, removeEncodingProfilesIfPresent
			);
#else
			int64_t encodingProfilesSetKey = _mmsEngineDBFacade->addEncodingProfilesSetIfNotAlreadyPresent(
				conn, workspace->_workspaceKey, contentType, label, removeEncodingProfilesIfPresent
			);
#endif

			field = "Profiles";
			json profilesRoot = encodingProfilesSetRoot[field];

			for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
			{
				string profileLabel = JSONUtils::asString(profilesRoot[profileIndex]);

#ifdef __POSTGRES__
				int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfileIntoSetIfNotAlreadyPresent(
					workspace->_workspaceKey, profileLabel, contentType, encodingProfilesSetKey
				);
#else
				int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfileIntoSetIfNotAlreadyPresent(
					conn, workspace->_workspaceKey, profileLabel, contentType, encodingProfilesSetKey
				);
#endif

				if (responseBody != "")
					responseBody += string(", ");
				responseBody +=
					(string("{ ") + "\"encodingProfileKey\": " + to_string(encodingProfileKey) + ", \"label\": \"" + profileLabel + "\" " + "}");
			}

			string beginOfResponseBody = string("{ ") + "\"encodingProfilesSet\": { " +
										 "\"encodingProfilesSetKey\": " + to_string(encodingProfilesSetKey) + ", \"label\": \"" + label + "\" " +
										 "}, " + "\"profiles\": [ ";
			responseBody.insert(0, beginOfResponseBody);
			responseBody += " ] }";
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"request body parsing failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addEncodingProfile(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addEncodingProfile";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
	);

	try
	{
		auto sContentTypeIt = queryParameters.find("contentType");
		if (sContentTypeIt == queryParameters.end())
		{
			string errorMessage = "'contentType' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(sContentTypeIt->second);

		json encodingProfileRoot = JSONUtils::toJson(requestBody);

		string responseBody;

		try
		{
			Validator validator(_mmsEngineDBFacade, _configurationRoot);
			validator.validateEncodingProfileRootMetadata(contentType, encodingProfileRoot);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(encodingProfileRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string profileLabel = JSONUtils::asString(encodingProfileRoot, field, "");

			MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
			{
				field = "fileFormat";
				string fileFormat = JSONUtils::asString(encodingProfileRoot, field, "");

				deliveryTechnology = MMSEngineDBFacade::fileFormatToDeliveryTechnology(fileFormat);
				/*
				string fileFormatLowerCase;
				fileFormatLowerCase.resize(fileFormat.size());
				transform(fileFormat.begin(), fileFormat.end(),
					fileFormatLowerCase.begin(), [](unsigned char c){return tolower(c); } );

				if (fileFormatLowerCase == "hls"
					|| fileFormatLowerCase == "dash"
				)
					deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming;
				else if (fileFormatLowerCase == "mp4"
					|| fileFormatLowerCase == "mkv"
					|| fileFormatLowerCase == "mov"
				)
					deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::DownloadAndStreaming;
				else
					deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::Download;
				*/

				SPDLOG_INFO(
					"deliveryTechnology"
					", fileFormat: {}"
					// + ", fileFormatLowerCase: " + fileFormatLowerCase
					", deliveryTechnology: {}",
					fileFormat, MMSEngineDBFacade::toString(deliveryTechnology)
				);
			}

			string jsonEncodingProfile;
			{
				jsonEncodingProfile = JSONUtils::toString(encodingProfileRoot);
			}

			int64_t encodingProfileKey =
				_mmsEngineDBFacade->addEncodingProfile(workspace->_workspaceKey, profileLabel, contentType, deliveryTechnology, jsonEncodingProfile);

			responseBody =
				(string("{ ") + "\"encodingProfileKey\": " + to_string(encodingProfileKey) + ", \"label\": \"" + profileLabel + "\" " + "}");
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addEncodingProfile failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addEncodingProfile failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeEncodingProfile(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeEncodingProfile";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, workspace->_workspaceKey
	);

	try
	{
		auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
		if (encodingProfileKeyIt == queryParameters.end())
		{
			string errorMessage = "'encodingProfileKey' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingProfileKey = stoll(encodingProfileKeyIt->second);

		try
		{
			_mmsEngineDBFacade->removeEncodingProfile(workspace->_workspaceKey, encodingProfileKey);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncodingProfile failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncodingProfile failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}

		string responseBody;

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeEncodingProfilesSet(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeEncodingProfilesSet";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, workspace->_workspaceKey
	);

	try
	{
		auto encodingProfilesSetKeyIt = queryParameters.find("encodingProfilesSetKey");
		if (encodingProfilesSetKeyIt == queryParameters.end())
		{
			string errorMessage = "'encodingProfilesSetKey' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingProfilesSetKey = stoll(encodingProfilesSetKeyIt->second);

		try
		{
			_mmsEngineDBFacade->removeEncodingProfilesSet(workspace->_workspaceKey, encodingProfilesSetKey);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncodingProfilesSet failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncodingProfilesSet failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}

		string responseBody;

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::killEncodingJob(
	int64_t encoderKey, int64_t ingestionJobKey, int64_t encodingJobKey,
	string killType // "kill", "restartWithinEncoder", "killToRestartByEngine"
)
{
	string ffmpegEncoderURL;
	ostringstream response;
	try
	{
		pair<string, bool> encoderDetails = _mmsEngineDBFacade->getEncoderURL(encoderKey);
		string transcoderHost;
		tie(transcoderHost, ignore) = encoderDetails;

		// ffmpegEncoderURL = _ffmpegEncoderProtocol
		// 	+ "://"
		// 	+ transcoderHost + ":"
		// 	+ to_string(_ffmpegEncoderPort)
		// ffmpegEncoderURL = transcoderHost + _ffmpegEncoderKillEncodingURI + "/" + to_string(ingestionJobKey) + "/" + to_string(encodingJobKey) +
		// 			   "?lightKill=" + (lightKill ? "true" : "false");
		ffmpegEncoderURL =
			std::format("{}{}/{}/{}?killType={}", transcoderHost, _ffmpegEncoderKillEncodingURI, ingestionJobKey, encodingJobKey, killType);

		vector<string> otherHeaders;
		CurlWrapper::httpDelete(
			ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword),
			otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey)
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"killEncoding URL failed"
			", encodingJobKey: {}"
			", ffmpegEncoderURL: {}"
			", exception: {}"
			", response.str(): {}",
			encodingJobKey, ffmpegEncoderURL, e.what(), response.str()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"killEncoding URL failed"
			", encodingJobKey: {}"
			", ffmpegEncoderURL: {}"
			", exception: {}"
			", response.str(): {}",
			encodingJobKey, ffmpegEncoderURL, e.what(), response.str()
		);

		throw e;
	}
}
