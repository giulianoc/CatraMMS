
#include "API.h"
#include "JSONUtils.h"
// #include "catralibraries/Encrypt.h"
#include "catralibraries/StringUtils.h"
#include <curlpp/cURLpp.hpp>
#include <regex>

void API::createDeliveryAuthorization(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
	shared_ptr<Workspace> requestWorkspace, string clientIPAddress, unordered_map<string, string> queryParameters
)
{
	string api = "createDeliveryAuthorization";

	SPDLOG_INFO("Received {}", api);

	try
	{
		int64_t physicalPathKey = -1;
		auto physicalPathKeyIt = queryParameters.find("physicalPathKey");
		if (physicalPathKeyIt != queryParameters.end())
		{
			physicalPathKey = stoll(physicalPathKeyIt->second);
		}

		int64_t mediaItemKey = -1;
		auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
		if (mediaItemKeyIt != queryParameters.end())
		{
			mediaItemKey = stoll(mediaItemKeyIt->second);
			if (mediaItemKey == 0)
				mediaItemKey = -1;
		}

		string uniqueName;
		auto uniqueNameIt = queryParameters.find("uniqueName");
		if (uniqueNameIt != queryParameters.end())
		{
			uniqueName = uniqueNameIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(uniqueName, regex(plus), plusDecoded);

			uniqueName = curlpp::unescape(firstDecoding);
		}

		int64_t encodingProfileKey = -1;
		auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
		if (encodingProfileKeyIt != queryParameters.end())
		{
			encodingProfileKey = stoll(encodingProfileKeyIt->second);
			if (encodingProfileKey == 0)
				encodingProfileKey = -1;
		}

		string encodingProfileLabel;
		auto encodingProfileLabelIt = queryParameters.find("encodingProfileLabel");
		if (encodingProfileLabelIt != queryParameters.end())
		{
			encodingProfileLabel = encodingProfileLabelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(encodingProfileLabel, regex(plus), plusDecoded);

			encodingProfileLabel = curlpp::unescape(firstDecoding);
		}

		// this is for live authorization
		int64_t ingestionJobKey = -1;
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt != queryParameters.end())
		{
			ingestionJobKey = stoll(ingestionJobKeyIt->second);
		}

		// this is for live authorization
		int64_t deliveryCode = -1;
		auto deliveryCodeIt = queryParameters.find("deliveryCode");
		if (deliveryCodeIt != queryParameters.end())
		{
			deliveryCode = stoll(deliveryCodeIt->second);
		}

		if (physicalPathKey == -1 &&
			((mediaItemKey == -1 && uniqueName == "")
			) // || (encodingProfileKey == -1 && encodingProfileLabel == "")) commentato perchè profile -1 indica 'source profile'
			&& ingestionJobKey == -1)
		{
			string errorMessage = string("The 'physicalPathKey' or the (mediaItemKey-uniqueName)/(encodingProfileKey-encodingProfileLabel) or "
										 "ingestionJobKey parameters have to be present");
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		int ttlInSeconds = _defaultTTLInSeconds;
		auto ttlInSecondsIt = queryParameters.find("ttlInSeconds");
		if (ttlInSecondsIt != queryParameters.end() && ttlInSecondsIt->second != "")
		{
			ttlInSeconds = stol(ttlInSecondsIt->second);
		}

		int maxRetries = _defaultMaxRetries;
		auto maxRetriesIt = queryParameters.find("maxRetries");
		if (maxRetriesIt != queryParameters.end() && maxRetriesIt->second != "")
		{
			maxRetries = stol(maxRetriesIt->second);
		}

		bool redirect = _defaultRedirect;
		auto redirectIt = queryParameters.find("redirect");
		if (redirectIt != queryParameters.end())
		{
			if (redirectIt->second == "true")
				redirect = true;
			else
				redirect = false;
		}

		bool save = false;
		auto saveIt = queryParameters.find("save");
		if (saveIt != queryParameters.end())
		{
			if (saveIt->second == "true")
				save = true;
			else
				save = false;
		}

		string deliveryType;
		auto deliveryTypeIt = queryParameters.find("deliveryType");
		if (deliveryTypeIt != queryParameters.end())
			deliveryType = deliveryTypeIt->second;

		bool filteredByStatistic = false;
		auto filteredByStatisticIt = queryParameters.find("filteredByStatistic");
		if (filteredByStatisticIt != queryParameters.end())
		{
			if (filteredByStatisticIt->second == "true")
				filteredByStatistic = true;
			else
				filteredByStatistic = false;
		}

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end())
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = curlpp::unescape(firstDecoding);
		}

		try
		{
			bool warningIfMissingMediaItemKey = false;
			pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
				userKey, requestWorkspace, clientIPAddress,

				mediaItemKey, uniqueName, encodingProfileKey, encodingProfileLabel,

				physicalPathKey,

				ingestionJobKey, deliveryCode,

				ttlInSeconds, maxRetries, save, deliveryType,

				warningIfMissingMediaItemKey, filteredByStatistic, userId
			);

			string deliveryURL;
			string deliveryFileName;

			tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

			if (redirect)
			{
				sendRedirect(request, deliveryURL);
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
		catch (MediaItemKeyNotFound &e)
		{
			SPDLOG_ERROR(
				"{} failed"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = fmt::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"{} failed"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = fmt::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"{} failed"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = string("Internal server error");
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = string("Internal server error");
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::createBulkOfDeliveryAuthorization(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
	shared_ptr<Workspace> requestWorkspace, string clientIPAddress, unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "createBulkOfDeliveryAuthorization";

	SPDLOG_INFO("Received {}", api);

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

			sendError(request, 400, e.what());

			throw runtime_error(e.what());
		}

		try
		{
			int ttlInSeconds = _defaultTTLInSeconds;
			auto ttlInSecondsIt = queryParameters.find("ttlInSeconds");
			if (ttlInSecondsIt != queryParameters.end() && ttlInSecondsIt->second != "")
				ttlInSeconds = stol(ttlInSecondsIt->second);

			int maxRetries = _defaultMaxRetries;
			auto maxRetriesIt = queryParameters.find("maxRetries");
			if (maxRetriesIt != queryParameters.end() && maxRetriesIt->second != "")
				maxRetries = stol(maxRetriesIt->second);

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

					string requestKey = fmt::format(
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
								userKey, requestWorkspace, clientIPAddress,

								mediaItemKey,
								"", // uniqueName,
								encodingProfileKey, encodingProfileLabel,

								-1, // physicalPathKey,

								-1, // ingestionJobKey,
								-1, // deliveryCode,

								ttlInSeconds, maxRetries, save, deliveryType, warningIfMissingMediaItemKey, filteredByStatistic, userId
							);
						}
						catch (MediaItemKeyNotFound &e)
						{
							SPDLOG_WARN(
								"createDeliveryAuthorization failed"
								", mediaItemKey: {}"
								", encodingProfileKey: {}"
								", e.what(): {}",
								mediaItemKey, encodingProfileKey, e.what()
							);

							continue;
						}
						catch (runtime_error &e)
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

					string requestKey = fmt::format(
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
								userKey, requestWorkspace, clientIPAddress,

								-1, // mediaItemKey,
								uniqueName, encodingProfileKey, encodingProfileLabel,

								-1, // physicalPathKey,

								-1, // ingestionJobKey,
								-1, // deliveryCode,

								ttlInSeconds, maxRetries, save, deliveryType, warningIfMissingMediaItemKey, filteredByStatistic, userId
							);
						}
						catch (MediaItemKeyNotFound &e)
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
						catch (runtime_error &e)
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
							userKey, requestWorkspace, clientIPAddress,

							-1, // mediaItemKey,
							"", // uniqueName,
							-1, // encodingProfileKey,
							"", // encodingProfileLabel,

							-1, // physicalPathKey,

							ingestionJobKey, deliveryCode,

							ttlInSeconds, maxRetries, save, deliveryType, warningIfMissingMediaItemKey, filteredByStatistic, userId
						);
					}
					catch (MediaItemKeyNotFound &e)
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
					catch (runtime_error &e)
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
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = fmt::format("Internal server error: {}", e.what());
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

			string errorMessage = string("Internal server error");
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = string("Internal server error");
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}
