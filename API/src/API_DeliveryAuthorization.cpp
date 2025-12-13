
#include "API.h"
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include <format>
#include <regex>

void API::createDeliveryAuthorization(
const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "createDeliveryAuthorization";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO("Received {}", api);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canDeliveryAuthorization)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", deliveryAuthorization: {}",
			apiAuthorizationDetails->canDeliveryAuthorization
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	string clientIPAddress = getClientIPAddress(requestDetails);

	try
	{
		// 2024-12-23: nel caso in cui:
		// - ci sia un sistema intermedio tra i veri client e MMS e
		// - sia questo sistema intermedio che crea le richieste di delivery
		// Questo sistema forward l'IP del client come parametro
		string playerIP = getQueryParameter(queryParameters, "remoteIPAddress", clientIPAddress, false);
		if (playerIP == clientIPAddress)
			playerIP = getQueryParameter(queryParameters, "playerIP", clientIPAddress, false);
		bool playerIPToBeAuthorized = getQueryParameter(queryParameters, "playerIPToBeAuthorized", false, false);

		string playerCountry = getQueryParameter(queryParameters, "playerCountry", string(), false);
		string playerRegion = getQueryParameter(queryParameters, "playerRegion", string(), false);

		int64_t physicalPathKey = getMapParameter(queryParameters, "physicalPathKey", static_cast<int64_t>(-1), false);
		int64_t mediaItemKey = getMapParameter(queryParameters, "mediaItemKey", static_cast<int64_t>(-1), false);
		if (mediaItemKey == 0)
			mediaItemKey = -1;

		string uniqueName = getQueryParameter(queryParameters, "uniqueName", string(), false);

		int64_t encodingProfileKey = getMapParameter(queryParameters, "encodingProfileKey", static_cast<int64_t>(-1), false);
		if (encodingProfileKey == 0)
			encodingProfileKey = -1;

		string encodingProfileLabel = getQueryParameter(queryParameters, "encodingProfileLabel", string(), false);

		// this is for live authorization
		int64_t ingestionJobKey = getMapParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), false);

		// this is for live authorization
		int64_t deliveryCode = getMapParameter(queryParameters, "deliveryCode", static_cast<int64_t>(-1), false);

		if (physicalPathKey == -1 &&
			((mediaItemKey == -1 && uniqueName.empty())
			) // || (encodingProfileKey == -1 && encodingProfileLabel == "")) commentato perchè profile -1 indica 'source profile'
			&& ingestionJobKey == -1)
		{
			string errorMessage = "The 'physicalPathKey' or the (mediaItemKey-uniqueName)/(encodingProfileKey-encodingProfileLabel) or "
								  "ingestionJobKey parameters have to be present";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int32_t ttlInSeconds = getMapParameter(queryParameters, "ttlInSeconds", _defaultTTLInSeconds, false);
		int32_t maxRetries = getMapParameter(queryParameters, "maxRetries", _defaultMaxRetries, false);

		bool reuseAuthIfPresent = getMapParameter(queryParameters, "reuseAuthIfPresent", true, false);
		bool redirect = getMapParameter(queryParameters, "redirect", _defaultRedirect, false);
		bool save = getMapParameter(queryParameters, "save", false, false);

		string deliveryType = getQueryParameter(queryParameters, "deliveryType", string(), false);

		bool filteredByStatistic = getMapParameter(queryParameters, "filteredByStatistic", false, false);

		string userId = getQueryParameter(queryParameters, "userId", string(), false);

		try
		{
			bool warningIfMissingMediaItemKey = false;
			pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace, playerIP,

				mediaItemKey, uniqueName, encodingProfileKey, encodingProfileLabel,

				physicalPathKey,

				ingestionJobKey, deliveryCode,

				ttlInSeconds, maxRetries, reuseAuthIfPresent, playerIPToBeAuthorized, playerCountry, playerRegion, save, deliveryType,

				warningIfMissingMediaItemKey, filteredByStatistic, userId
			);

			string deliveryURL;
			string deliveryFileName;

			tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

			if (redirect)
			{
				sendRedirect(request, deliveryURL, false);
			}
			else
			{
				json responseRoot;

				string field = "deliveryURL";
				responseRoot[field] = deliveryURL;

				field = "deliveryFileName";
				responseRoot[field] = deliveryFileName;

				field = "ttlInSeconds";
				responseRoot[field] = ttlInSeconds;

				field = "maxRetries";
				responseRoot[field] = maxRetries;

				string responseBody = JSONUtils::toString(responseRoot);

				/*
				string responseBody = string("{ ")
					+ "\"deliveryURL\": \"" + deliveryURL + "\""
					+ ", \"deliveryFileName\": \"" + deliveryFileName + "\""
					+ ", \"ttlInSeconds\": " + to_string(ttlInSeconds)
					+ ", \"maxRetries\": " + to_string(maxRetries)
					+ " }";
				*/
				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
			}
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
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::createBulkOfDeliveryAuthorization(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "createBulkOfDeliveryAuthorization";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO("Received {}", api);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canDeliveryAuthorization)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", deliveryAuthorization: {}",
			apiAuthorizationDetails->canDeliveryAuthorization
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	string clientIPAddress = getClientIPAddress(requestDetails);

	try
	{
		/*
		 * input:
			{
				"uniqueNameList" : [
					{
						"uniqueName": "...",
						"encodingProfileKey": 123
					},
					...
				],
				"liveIngestionJobKeyList" : [
					{
						"ingestionJobKey": 1234
					},
					...
				]
			 }
			output is like the input with the addition of the deliveryURL field
		*/
		json deliveryAutorizationDetailsRoot;
		try
		{
			deliveryAutorizationDetailsRoot = JSONUtils::toJson(requestBody);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(e.what());

			throw runtime_error(e.what());
		}

		try
		{
			// 2024-12-23: nel caso in cui:
			// - ci sia un sistema intermedio tra i veri client e MMS e
			// - sia questo sistema intermedio che crea le richieste di delivery
			// Questo sistema forward l'IP del client come parametro
			string playerIP = getQueryParameter(queryParameters, "remoteIPAddress", clientIPAddress, false);
			if (playerIP == clientIPAddress)
				playerIP = getQueryParameter(queryParameters, "playerIP", clientIPAddress, false);

			int32_t ttlInSeconds = getMapParameter(queryParameters, "ttlInSeconds", _defaultTTLInSeconds, false);

			int32_t maxRetries = getMapParameter(queryParameters, "maxRetries", _defaultMaxRetries, false);

			bool reuseAuthIfPresent = getMapParameter(queryParameters, "reuseAuthIfPresent", true, false);

			bool save = false;

			string field = "mediaItemKeyList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				json mediaItemKeyListRoot = deliveryAutorizationDetailsRoot[field];

				// spesso molte entry contengono lo stesso input. Ad esempio, la pagina mediaItems della GUI,
				// spesso richiede la stessa immagina (nel caso dei contenuti di una serie, le immagini
				// sono tutte le stesse). Per questo motivo usiamo una mappa che conserva le deliveryURL
				// ed evita di farle ricalcolare se il lavoro è stato già fatto
				map<string, string> deliveryURLAlreadyCreated;
				for (auto &[keyRoot, valRoot] : mediaItemKeyListRoot.items())
				{
					field = "mediaItemKey";
					int64_t mediaItemKey = JSONUtils::asInt64(valRoot, field, -1);
					field = "encodingProfileKey";
					int64_t encodingProfileKey = JSONUtils::asInt64(valRoot, field, -1);
					field = "encodingProfileLabel";
					string encodingProfileLabel = JSONUtils::asString(valRoot, field, "");

					field = "deliveryType";
					string deliveryType = JSONUtils::asString(valRoot, field, "");

					field = "filteredByStatistic";
					bool filteredByStatistic = JSONUtils::asBool(valRoot, field, false);

					field = "userId";
					string userId = JSONUtils::asString(valRoot, field, "");

					string requestKey = std::format(
						"{}_{}_{}_{}_{}_{}", mediaItemKey, encodingProfileKey, encodingProfileLabel, deliveryType, filteredByStatistic, userId
					);
					map<string, string>::const_iterator searchIt = deliveryURLAlreadyCreated.find(requestKey);
					if (searchIt == deliveryURLAlreadyCreated.end())
					{
						pair<string, string> deliveryAuthorizationDetails;
						try
						{
							bool warningIfMissingMediaItemKey = true;
							deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
								apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace, playerIP,

								mediaItemKey,
								"", // uniqueName,
								encodingProfileKey, encodingProfileLabel,

								-1, // physicalPathKey,

								-1, // ingestionJobKey,
								-1, // deliveryCode,

								ttlInSeconds, maxRetries, reuseAuthIfPresent,
								false, // playerIPToBeAuthorized
								"",	   // playerCountry
								"",	   // playerRegion
								save, deliveryType, warningIfMissingMediaItemKey, filteredByStatistic, userId
							);
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"createDeliveryAuthorization failed"
								", mediaItemKey: {}"
								", encodingProfileKey: {}"
								", e.what(): {}",
								mediaItemKey, encodingProfileKey, e.what()
							);

							continue;
						}

						string deliveryURL;

						tie(deliveryURL, ignore) = deliveryAuthorizationDetails;

						field = "deliveryURL";
						valRoot[field] = deliveryURL;

						deliveryURLAlreadyCreated.insert(make_pair(requestKey, deliveryURL));
					}
					else
					{
						field = "deliveryURL";
						valRoot[field] = searchIt->second;
					}
				}

				field = "mediaItemKeyList";
				deliveryAutorizationDetailsRoot[field] = mediaItemKeyListRoot;
			}

			field = "uniqueNameList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				json uniqueNameListRoot = deliveryAutorizationDetailsRoot[field];

				// spesso molte entry contengono lo stesso input. Ad esempio, la pagina mediaItems della GUI,
				// spesso richiede la stessa immagina (nel caso dei contenuti di una serie, le immagini
				// sono tutte le stesse). Per questo motivo usiamo una mappa che conserva le deliveryURL
				// ed evita di farle ricalcolare se il lavoro è stato già fatto
				map<string, string> deliveryURLAlreadyCreated;
				for (int uniqueNameIndex = 0; uniqueNameIndex < uniqueNameListRoot.size(); uniqueNameIndex++)
				{
					json uniqueNameRoot = uniqueNameListRoot[uniqueNameIndex];

					field = "uniqueName";
					string uniqueName = JSONUtils::asString(uniqueNameRoot, field, "");
					field = "encodingProfileKey";
					int64_t encodingProfileKey = JSONUtils::asInt64(uniqueNameRoot, field, -1);
					field = "encodingProfileLabel";
					string encodingProfileLabel = JSONUtils::asString(uniqueNameRoot, field, "");

					field = "deliveryType";
					string deliveryType = JSONUtils::asString(uniqueNameRoot, field, "");

					field = "filteredByStatistic";
					bool filteredByStatistic = JSONUtils::asBool(uniqueNameRoot, field, false);

					field = "userId";
					string userId = JSONUtils::asString(uniqueNameRoot, field, "");

					string requestKey = std::format(
						"{}_{}_{}_{}_{}_{}", uniqueName, encodingProfileKey, encodingProfileLabel, deliveryType, filteredByStatistic, userId
					);
					map<string, string>::const_iterator searchIt = deliveryURLAlreadyCreated.find(requestKey);
					if (searchIt == deliveryURLAlreadyCreated.end())
					{
						pair<string, string> deliveryAuthorizationDetails;
						try
						{
							bool warningIfMissingMediaItemKey = true;
							deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
								apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace, playerIP,

								-1, // mediaItemKey,
								uniqueName, encodingProfileKey, encodingProfileLabel,

								-1, // physicalPathKey,

								-1, // ingestionJobKey,
								-1, // deliveryCode,

								ttlInSeconds, maxRetries, reuseAuthIfPresent,
								false, // playerIPToBeAuthorized
								"",	   // playerCountry
								"",	   // playerRegion
								save, deliveryType, warningIfMissingMediaItemKey, filteredByStatistic, userId
							);
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"createDeliveryAuthorization failed"
								", uniqueName: {}"
								", encodingProfileKey: {}"
								", e.what(): {}",
								uniqueName, encodingProfileKey, e.what()
							);

							continue;
						}

						string deliveryURL;
						string deliveryFileName;

						tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

						field = "deliveryURL";
						uniqueNameRoot[field] = deliveryURL;

						deliveryURLAlreadyCreated.insert(make_pair(requestKey, deliveryURL));
					}
					else
					{
						field = "deliveryURL";
						uniqueNameRoot[field] = searchIt->second;
					}

					uniqueNameListRoot[uniqueNameIndex] = uniqueNameRoot;
				}

				field = "uniqueNameList";
				deliveryAutorizationDetailsRoot[field] = uniqueNameListRoot;
			}

			field = "liveIngestionJobKeyList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				json liveIngestionJobKeyListRoot = deliveryAutorizationDetailsRoot[field];
				for (int liveIngestionJobKeyIndex = 0; liveIngestionJobKeyIndex < liveIngestionJobKeyListRoot.size(); liveIngestionJobKeyIndex++)
				{
					json liveIngestionJobKeyRoot = liveIngestionJobKeyListRoot[liveIngestionJobKeyIndex];

					field = "ingestionJobKey";
					int64_t ingestionJobKey = JSONUtils::asInt64(liveIngestionJobKeyRoot, field, -1);
					field = "deliveryCode";
					int64_t deliveryCode = JSONUtils::asInt64(liveIngestionJobKeyRoot, field, -1);

					field = "deliveryType";
					string deliveryType = JSONUtils::asString(liveIngestionJobKeyRoot, field, "");

					field = "filteredByStatistic";
					bool filteredByStatistic = JSONUtils::asBool(liveIngestionJobKeyRoot, field, false);

					field = "userId";
					string userId = JSONUtils::asString(liveIngestionJobKeyRoot, field, "");

					pair<string, string> deliveryAuthorizationDetails;
					try
					{
						bool warningIfMissingMediaItemKey = false;
						deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
							apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace, playerIP,

							-1, // mediaItemKey,
							"", // uniqueName,
							-1, // encodingProfileKey,
							"", // encodingProfileLabel,

							-1, // physicalPathKey,

							ingestionJobKey, deliveryCode,

							ttlInSeconds, maxRetries, reuseAuthIfPresent,
							false, // playerIPToBeAuthorized
							"",	   // playerCountry
							"",	   // playerRegion
							save, deliveryType, warningIfMissingMediaItemKey, filteredByStatistic, userId
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"createDeliveryAuthorization failed"
							", ingestionJobKey: {}"
							", deliveryCode: {}"
							", e.what(): {}",
							ingestionJobKey, deliveryCode, e.what()
						);

						continue;
					}

					string deliveryURL;
					string deliveryFileName;

					tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

					field = "deliveryURL";
					liveIngestionJobKeyRoot[field] = deliveryURL;

					liveIngestionJobKeyListRoot[liveIngestionJobKeyIndex] = liveIngestionJobKeyRoot;
				}

				field = "liveIngestionJobKeyList";
				deliveryAutorizationDetailsRoot[field] = liveIngestionJobKeyListRoot;
			}

			{
				string responseBody = JSONUtils::toString(deliveryAutorizationDetailsRoot);

				// SPDLOG_INFO("createDeliveryAuthorization"
				// 	", responseBody: {}",
				// 	responseBody
				// );

				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
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

void API::binaryAuthorization(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "binaryAuthorization";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		// since we are here, for sure user is authorized

		auto binaryVirtualHostNameIt = queryParameters.find("binaryVirtualHostName");
		auto binaryListenHostIt = queryParameters.find("binaryListenHost");

		// retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the progress id (set in the nginx server configuration)
		auto progressIdIt = requestDetails.find("HTTP_X_ORIGINAL_METHOD");
		auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
		if (binaryVirtualHostNameIt != queryParameters.end()
			&& binaryListenHostIt != queryParameters.end()
			&& progressIdIt != requestDetails.end()
			&& originalURIIt != requestDetails.end())
		{
			size_t ingestionJobKeyIndex = originalURIIt->second.find_last_of("/");
			if (ingestionJobKeyIndex != string::npos)
			{
				try
				{
					struct FileUploadProgressData::RequestData requestData;

					requestData._progressId = progressIdIt->second;
					requestData._binaryListenHost = binaryListenHostIt->second;
					requestData._binaryVirtualHostName = binaryVirtualHostNameIt->second;
					// requestData._binaryListenIp = binaryVirtualHostNameIt->second;
					requestData._ingestionJobKey = stoll(originalURIIt->second.substr(ingestionJobKeyIndex + 1));
					requestData._lastPercentageUpdated = 0;
					requestData._callFailures = 0;

					// Content-Range: bytes 0-99999/100000
					requestData._contentRangePresent = false;
					requestData._contentRangeStart = -1;
					requestData._contentRangeEnd = -1;
					requestData._contentRangeSize = -1;
					auto contentRangeIt = requestDetails.find("HTTP_CONTENT_RANGE");
					if (contentRangeIt != requestDetails.end())
					{
						string_view contentRange = contentRangeIt->second;
						try
						{
							parseContentRange(contentRange, requestData._contentRangeStart, requestData._contentRangeEnd,
								requestData._contentRangeSize);

							requestData._contentRangePresent = true;
						}
						catch (exception &e)
						{
							string errorMessage = std::format(
								"Content-Range is not well done. Expected format: 'Content-Range: bytes <start>-<end>/<size>'"
								", contentRange: {}",
								contentRange
							);
							SPDLOG_ERROR(errorMessage);
							throw runtime_error(errorMessage);
						}
					}

					SPDLOG_INFO(
						"Content-Range details"
						", contentRangePresent: {}"
						", contentRangeStart: {}"
						", contentRangeEnd: {}"
						", contentRangeSize: {}",
						requestData._contentRangePresent, requestData._contentRangeStart, requestData._contentRangeEnd, requestData._contentRangeSize
					);

					lock_guard<mutex> locker(_fileUploadProgressData->_mutex);

					_fileUploadProgressData->_filesUploadProgressToBeMonitored.push_back(requestData);
					SPDLOG_INFO(
						"Added upload file progress to be monitored"
						", _progressId: {}"
						", _binaryVirtualHostName: {}"
						", _binaryListenHost: {}",
						requestData._progressId, requestData._binaryVirtualHostName, requestData._binaryListenHost
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"ProgressId not found"
						", progressIdIt->second: {}",
						progressIdIt->second
					);
				}
			}
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200);
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

		throw;
	}
}

void API::deliveryAuthorizationThroughParameter(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "deliveryAuthorizationThroughParameter";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		// retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the token to be checked (set in the nginx server configuration)

		auto tokenIt = requestDetails.find("HTTP_X_ORIGINAL_METHOD");
		auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
		if (tokenIt == requestDetails.end() || originalURIIt == requestDetails.end())
		{
			string errorMessage = std::format(
				"deliveryAuthorization, not authorized"
				", token: {}"
				", URI: {}",
				(tokenIt != requestDetails.end() ? tokenIt->second : "null"),
				(originalURIIt != requestDetails.end() ? originalURIIt->second : "null")
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}

		string contentURI = originalURIIt->second;
		size_t endOfURIIndex = contentURI.find_last_of("?");
		if (endOfURIIndex == string::npos)
		{
			string errorMessage = std::format(
				"Wrong URI format"
				", contentURI: {}",
				contentURI
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}
		contentURI = contentURI.substr(0, endOfURIIndex);

		string tokenParameter = tokenIt->second;

		SPDLOG_INFO(
			"Calling checkDeliveryAuthorizationThroughParameter"
			", contentURI: {}"
			", tokenParameter: {}",
			contentURI, tokenParameter
		);

		_mmsDeliveryAuthorization->checkDeliveryAuthorizationThroughParameter(contentURI, tokenParameter);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200);
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

		throw HTTPError(403);
	}
}

void API::deliveryAuthorizationThroughPath(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "deliveryAuthorizationThroughPath";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		// retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the token to be checked (set in the nginx server configuration)

		auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
		if (originalURIIt == requestDetails.end())
		{
			string errorMessage = std::format(
				"deliveryAuthorization, not authorized"
				", URI: {}",
				(originalURIIt != requestDetails.end() ? originalURIIt->second : "null")
			);
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}
		string contentURI = originalURIIt->second;

		/* log incluso in checkDeliveryAuthorizationThroughPath
		SPDLOG_INFO(
			"deliveryAuthorizationThroughPath. Calling checkDeliveryAuthorizationThroughPath"
			", contentURI: {}",
			contentURI
		);
		*/

		_mmsDeliveryAuthorization->checkDeliveryAuthorizationThroughPath(contentURI);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200);
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

		throw HTTPError(403);
	}
}