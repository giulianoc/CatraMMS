
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
		throw HTTPError(500);
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
		throw HTTPError(500);
	}
}
