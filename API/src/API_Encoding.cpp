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
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "PostgresConnection.h"
#include "Validator.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <regex>
#include <sstream>

void API::encodingJobsStatus(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "encodingJobsStatus";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t encodingJobKey = requestData.getQueryParameter("encodingJobKey", static_cast<int64_t>(-1));

		int start = requestData.getQueryParameter("start", static_cast<int32_t>(0));

		int rows = requestData.getQueryParameter("rows", static_cast<int32_t>(10));
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

		string startIngestionDate = requestData.getQueryParameter("startIngestionDate", "");
		string endIngestionDate = requestData.getQueryParameter("endIngestionDate", "");

		string startEncodingDate = requestData.getQueryParameter("startEncodingDate", "");
		string endEncodingDate = requestData.getQueryParameter("endEncodingDate", "");

		int64_t encoderKey = requestData.getQueryParameter("encoderKey", static_cast<int64_t>(-1));

		bool alsoEncodingJobsFromOtherWorkspaces = requestData.getQueryParameter("alsoEncodingJobsFromOtherWorkspaces", false);

		bool asc = requestData.getQueryParameter("asc", true);

		string status = requestData.getQueryParameter("status", "all");

		string types = requestData.getQueryParameter("types", "");

		bool fromMaster = requestData.getQueryParameter("fromMaster", false);

		{
			json encodingStatusRoot = _mmsEngineDBFacade->getEncodingJobsStatus(
				apiAuthorizationDetails->workspace, encodingJobKey, start, rows,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate,
				// startAndEndEncodingDatePresent,
				startEncodingDate, endEncodingDate, encoderKey, alsoEncodingJobsFromOtherWorkspaces, asc, status, types, fromMaster
			);

			string responseBody = JSONUtils::toString(encodingStatusRoot);

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

void API::encodingJobPriority(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "encodingJobPriority";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t encodingJobKey = requestData.getQueryParameter("encodingJobKey", static_cast<int64_t>(-1));

		bool newEncodingJobPriorityPresent = false;
		int32_t iNewEncodingJobPriority = requestData.getQueryParameter("newEncodingJobPriorityCode", static_cast<int32_t>(-1),
			false, &newEncodingJobPriorityPresent);
		MMSEngineDBFacade::EncodingPriority newEncodingJobPriority;
		if (newEncodingJobPriorityPresent && iNewEncodingJobPriority >= 0)
			newEncodingJobPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(iNewEncodingJobPriority);

		bool tryEncodingAgain = requestData.getQueryParameter("tryEncodingAgain", false);

		{
			if (newEncodingJobPriorityPresent)
			{
				_mmsEngineDBFacade->updateEncodingJobPriority(apiAuthorizationDetails->workspace, encodingJobKey, newEncodingJobPriority);
			}

			if (tryEncodingAgain)
			{
				_mmsEngineDBFacade->updateEncodingJobTryAgain(apiAuthorizationDetails->workspace, encodingJobKey);
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

void API::killOrCancelEncodingJob(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "killOrCancelEncodingJob";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canKillEncoding)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canKillEncoding: {}",
			apiAuthorizationDetails->canKillEncoding
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		int64_t encodingJobKey = requestData.getQueryParameter("encodingJobKey", static_cast<int64_t>(-1), false);

		// killType: "kill", "restartWithinEncoder", "killToRestartByEngine"
		string killType = requestData.getQueryParameter("killType", string("kill"), false);
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

void API::encodingProfilesSetsList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "encodingProfilesSetsList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		const int64_t encodingProfilesSetKey = requestData.getQueryParameter("encodingProfilesSetKey", static_cast<int64_t>(-1));

		optional<MMSEngineDBFacade::ContentType> contentType;
		optional<string> sContentType = requestData.getOptQueryParameter<string>("contentType");
		if (sContentType)
			contentType = MMSEngineDBFacade::toContentType(*sContentType);

		{

			json encodingProfilesSetListRoot =
				_mmsEngineDBFacade->getEncodingProfilesSetList(apiAuthorizationDetails->workspace->_workspaceKey, encodingProfilesSetKey, contentType);

			string responseBody = JSONUtils::toString(encodingProfilesSetListRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
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

void API::encodingProfilesList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "encodingProfilesList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t encodingProfileKey = requestData.getQueryParameter("encodingProfileKey", static_cast<int64_t>(-1));

		optional<MMSEngineDBFacade::ContentType> contentType;
		optional<string> sContentType = requestData.getOptQueryParameter<string>("contentType");
		if (sContentType)
			contentType = MMSEngineDBFacade::toContentType(*sContentType);

		string label = requestData.getQueryParameter("label", "");

		{
			json encodingProfileListRoot =
				_mmsEngineDBFacade->getEncodingProfileList(apiAuthorizationDetails->workspace->_workspaceKey, encodingProfileKey, contentType, label);

			string responseBody = JSONUtils::toString(encodingProfileListRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
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

void API::addUpdateEncodingProfilesSet(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addUpdateEncodingProfilesSet";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCreateProfiles)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canCreateProfiles: {}",
			apiAuthorizationDetails->canCreateProfiles
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(
			requestData.getQueryParameter("contentType", "", true));

		json encodingProfilesSetRoot = JSONUtils::toJson(requestData.requestBody);

		string responseBody;

		try
		{
			Validator validator(_mmsEngineDBFacade, _configurationRoot);
			validator.validateEncodingProfilesSetRootMetadata(contentType, encodingProfilesSetRoot);

			string field = "label";
			if (!JSONUtils::isPresent(encodingProfilesSetRoot, field))
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
				apiAuthorizationDetails->workspace->_workspaceKey, contentType, label, removeEncodingProfilesIfPresent
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
					apiAuthorizationDetails->workspace->_workspaceKey, profileLabel, contentType, encodingProfilesSetKey
				);
#else
				int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfileIntoSetIfNotAlreadyPresent(
					conn, workspace->_workspaceKey, profileLabel, contentType, encodingProfilesSetKey
				);
#endif

				if (!responseBody.empty())
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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);
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

void API::addEncodingProfile(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addEncodingProfile";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCreateProfiles)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canCreateProfiles: {}",
			apiAuthorizationDetails->canCreateProfiles
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(
			requestData.getQueryParameter("contentType", "", true));

		json encodingProfileRoot = JSONUtils::toJson(requestData.requestBody);

		string responseBody;

		try
		{
			Validator validator(_mmsEngineDBFacade, _configurationRoot);
			validator.validateEncodingProfileRootMetadata(contentType, encodingProfileRoot);

			string field = "label";
			if (!JSONUtils::isPresent(encodingProfileRoot, field))
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
				_mmsEngineDBFacade->addEncodingProfile(apiAuthorizationDetails->workspace->_workspaceKey, profileLabel, contentType, deliveryTechnology, jsonEncodingProfile);

			responseBody =
				(string("{ ") + "\"encodingProfileKey\": " + to_string(encodingProfileKey) + ", \"label\": \"" + profileLabel + "\" " + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addEncodingProfile failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);
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

void API::removeEncodingProfile(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeEncodingProfile";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCreateProfiles)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canCreateProfiles: {}",
			apiAuthorizationDetails->canCreateProfiles
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		const int64_t encodingProfileKey = requestData.getQueryParameter("encodingProfileKey", static_cast<int64_t>(-1), true);

		try
		{
			_mmsEngineDBFacade->removeEncodingProfile(apiAuthorizationDetails->workspace->_workspaceKey, encodingProfileKey);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncodingProfile failed"
				", e.what(): {}",
				e.what()
			);

			throw;
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

void API::removeEncodingProfilesSet(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeEncodingProfilesSet";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCreateProfiles)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canCreateProfiles: {}",
			apiAuthorizationDetails->canCreateProfiles
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		const int64_t encodingProfilesSetKey = requestData.getQueryParameter("encodingProfilesSetKey", static_cast<int64_t>(-1), true);

		try
		{
			_mmsEngineDBFacade->removeEncodingProfilesSet(apiAuthorizationDetails->workspace->_workspaceKey, encodingProfilesSetKey);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncodingProfilesSet failed"
				", e.what(): {}",
				e.what()
			);

			throw;
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

		throw;
	}
}
