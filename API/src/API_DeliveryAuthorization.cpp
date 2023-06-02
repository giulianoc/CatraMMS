
#include "API.h"
#include "JSONUtils.h"
#include <curlpp/cURLpp.hpp>
#include <regex>
#include "catralibraries/Encrypt.h"

void API::createDeliveryAuthorization(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	int64_t userKey,
	shared_ptr<Workspace> requestWorkspace,
	string clientIPAddress,
	unordered_map<string, string> queryParameters)
{
    string api = "createDeliveryAuthorization";

    _logger->info(__FILEREF__ + "Received " + api
    );

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

		if (physicalPathKey == -1
			&& ((mediaItemKey == -1 && uniqueName == "")) // || (encodingProfileKey == -1 && encodingProfileLabel == "")) commentato perchè profile -1 indica 'source profile'
			&& ingestionJobKey == -1)
		{
            string errorMessage = string("The 'physicalPathKey' or the (mediaItemKey-uniqueName)/(encodingProfileKey-encodingProfileLabel) or ingestionJobKey parameters have to be present");
            _logger->error(__FILEREF__ + errorMessage);

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
			pair<string, string> deliveryAuthorizationDetails =
				_mmsDeliveryAuthorization->createDeliveryAuthorization(
				userKey,
				requestWorkspace,
				clientIPAddress,

				mediaItemKey,
				uniqueName,
				encodingProfileKey,
				encodingProfileLabel,

				physicalPathKey,

				ingestionJobKey,
				deliveryCode,

				ttlInSeconds,
				maxRetries,
				save,
				deliveryType,

				warningIfMissingMediaItemKey,
				filteredByStatistic,
				userId
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
				Json::Value responseRoot;

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
				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
					request, "", api, 201, responseBody);
			}
        }
        catch(MediaItemKeyNotFound e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
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
            + ", e.what(): " + e.what()
        );

        throw e;
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

void API::createBulkOfDeliveryAuthorization(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	int64_t userKey,
	shared_ptr<Workspace> requestWorkspace,
	string clientIPAddress,
	unordered_map<string, string> queryParameters,
	string requestBody)
{
    string api = "createBulkOfDeliveryAuthorization";

    _logger->info(__FILEREF__ + "Received " + api
    );

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
		Json::Value deliveryAutorizationDetailsRoot;
        try
        {
			deliveryAutorizationDetailsRoot = JSONUtils::toJson(-1, -1, requestBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + e.what());

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
				Json::Value mediaItemKeyListRoot = deliveryAutorizationDetailsRoot[field];
				for (int mediaItemKeyIndex = 0;
					mediaItemKeyIndex < mediaItemKeyListRoot.size();
					mediaItemKeyIndex++)
				{
					Json::Value mediaItemKeyRoot = mediaItemKeyListRoot[mediaItemKeyIndex];

					field = "mediaItemKey";
					int64_t mediaItemKey = JSONUtils::asInt64(mediaItemKeyRoot, field, -1);
					field = "encodingProfileKey";
					int64_t encodingProfileKey = JSONUtils::asInt64(mediaItemKeyRoot,
						field, -1);
					field = "encodingProfileLabel";
					string encodingProfileLabel = JSONUtils::asString(mediaItemKeyRoot, field, "");

					field = "deliveryType";
					string deliveryType = JSONUtils::asString(mediaItemKeyRoot, field, "");

					field = "filteredByStatistic";
					bool filteredByStatistic  = JSONUtils::asBool(mediaItemKeyRoot, field, false);

					field = "userId";
					string userId = JSONUtils::asString(mediaItemKeyRoot, field, "");

					pair<string, string> deliveryAuthorizationDetails;
					try
					{
						bool warningIfMissingMediaItemKey = true;
						deliveryAuthorizationDetails =
							_mmsDeliveryAuthorization->createDeliveryAuthorization(
							userKey,
							requestWorkspace,
							clientIPAddress,

							mediaItemKey,
							"",	// uniqueName,
							encodingProfileKey,
							encodingProfileLabel,

							-1,	// physicalPathKey,

							-1,	// ingestionJobKey,
							-1,	// deliveryCode,

							ttlInSeconds,
							maxRetries,
							save,
							deliveryType,
							warningIfMissingMediaItemKey,
							filteredByStatistic,
							userId
						);
					}
					catch (MediaItemKeyNotFound e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (runtime_error e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (exception e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}

					string deliveryURL;
					string deliveryFileName;

					tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

					field = "deliveryURL";
					mediaItemKeyRoot[field] = deliveryURL;

					mediaItemKeyListRoot[mediaItemKeyIndex] = mediaItemKeyRoot;
				}

				field = "mediaItemKeyList";
				deliveryAutorizationDetailsRoot[field] = mediaItemKeyListRoot;
			}

			field = "uniqueNameList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				Json::Value uniqueNameListRoot = deliveryAutorizationDetailsRoot[field];
				for (int uniqueNameIndex = 0; uniqueNameIndex < uniqueNameListRoot.size();
					uniqueNameIndex++)
				{
					Json::Value uniqueNameRoot = uniqueNameListRoot[uniqueNameIndex];

					field = "uniqueName";
					string uniqueName = JSONUtils::asString(uniqueNameRoot, field, "");
					field = "encodingProfileKey";
					int64_t encodingProfileKey = JSONUtils::asInt64(uniqueNameRoot, field, -1);
					field = "encodingProfileLabel";
					string encodingProfileLabel = JSONUtils::asString(uniqueNameRoot, field, "");

					field = "deliveryType";
					string deliveryType = JSONUtils::asString(uniqueNameRoot, field, "");

					field = "filteredByStatistic";
					bool filteredByStatistic  = JSONUtils::asBool(uniqueNameRoot, field, false);

					field = "userId";
					string userId = JSONUtils::asString(uniqueNameRoot, field, "");

					pair<string, string> deliveryAuthorizationDetails;
					try
					{
						bool warningIfMissingMediaItemKey = true;
						deliveryAuthorizationDetails =
							_mmsDeliveryAuthorization->createDeliveryAuthorization(
							userKey,
							requestWorkspace,
							clientIPAddress,

							-1,	// mediaItemKey,
							uniqueName,
							encodingProfileKey,
							encodingProfileLabel,

							-1,	// physicalPathKey,

							-1,	// ingestionJobKey,
							-1,	// deliveryCode,

							ttlInSeconds,
							maxRetries,
							save,
							deliveryType,
							warningIfMissingMediaItemKey,
							filteredByStatistic,
							userId
						);
					}
					catch (MediaItemKeyNotFound e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", uniqueName: " + uniqueName
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (runtime_error e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", uniqueName: " + uniqueName
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (exception e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", uniqueName: " + uniqueName
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}

					string deliveryURL;
					string deliveryFileName;

					tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

					field = "deliveryURL";
					uniqueNameRoot[field] = deliveryURL;

					uniqueNameListRoot[uniqueNameIndex] = uniqueNameRoot;
				}

				field = "uniqueNameList";
				deliveryAutorizationDetailsRoot[field] = uniqueNameListRoot;
			}

			field = "liveIngestionJobKeyList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				Json::Value liveIngestionJobKeyListRoot = deliveryAutorizationDetailsRoot[field];
				for (int liveIngestionJobKeyIndex = 0;
					liveIngestionJobKeyIndex < liveIngestionJobKeyListRoot.size();
					liveIngestionJobKeyIndex++)
				{
					Json::Value liveIngestionJobKeyRoot = liveIngestionJobKeyListRoot[liveIngestionJobKeyIndex];

					field = "ingestionJobKey";
					int64_t ingestionJobKey = JSONUtils::asInt64(liveIngestionJobKeyRoot, field, -1);
					field = "deliveryCode";
					int64_t deliveryCode = JSONUtils::asInt64(liveIngestionJobKeyRoot, field, -1);

					field = "deliveryType";
					string deliveryType = JSONUtils::asString(liveIngestionJobKeyRoot, field, "");

					field = "filteredByStatistic";
					bool filteredByStatistic  = JSONUtils::asBool(liveIngestionJobKeyRoot, field, false);

					field = "userId";
					string userId = JSONUtils::asString(liveIngestionJobKeyRoot, field, "");

					pair<string, string> deliveryAuthorizationDetails;
					try
					{
						bool warningIfMissingMediaItemKey = false;
						deliveryAuthorizationDetails =
							_mmsDeliveryAuthorization->createDeliveryAuthorization(
							userKey,
							requestWorkspace,
							clientIPAddress,

							-1,	// mediaItemKey,
							"",	// uniqueName,
							-1,	// encodingProfileKey,
							"",	// encodingProfileLabel,

							-1,	// physicalPathKey,

							ingestionJobKey,
							deliveryCode,

							ttlInSeconds,
							maxRetries,
							save,
							deliveryType,
							warningIfMissingMediaItemKey,
							filteredByStatistic,
							userId
						);
					}
					catch (MediaItemKeyNotFound e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", deliveryCode: " + to_string(deliveryCode)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (runtime_error e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", deliveryCode: " + to_string(deliveryCode)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (exception e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", deliveryCode: " + to_string(deliveryCode)
							+ ", e.what(): " + e.what()
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

				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
					request, "", api, 201, responseBody);
			}
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
            + ", e.what(): " + e.what()
        );

        throw e;
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

int64_t API::checkDeliveryAuthorizationThroughParameter(
		string contentURI, string tokenParameter)
{
	int64_t tokenComingFromURL;
	try
	{
		_logger->info(__FILEREF__ + "checkDeliveryAuthorizationThroughParameter, received"
			+ ", contentURI: " + contentURI
			+ ", tokenParameter: " + tokenParameter
		);

		string firstPartOfToken;
		string secondPartOfToken;
		{
			// token formats:
			// scenario in case of .ts (hls) delivery: <encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			// scenario in case of .m4s (dash) delivery: ---<cookie: encription of 'token'>
			//		both encryption were built in 'manageHTTPStreamingManifest'
			// scenario in case of .m3u8 delivery:
			//		case 1. master .m3u8: <token>---
			//		case 2. secondary .m3u8: <encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			// scenario in case of any other delivery: <token>---
			//		Any other delivery includes for example also the delivery/download of a .ts file, not part
			//		of an hls delivery. In this case we will have just a token as any normal download

			string separator = "---";
			size_t endOfTokenIndex = tokenParameter.rfind(separator);
			if (endOfTokenIndex == string::npos)
			{
				string errorMessage = string("Wrong token format, no --- is present")
					+ ", contentURI: " + contentURI
					+ ", tokenParameter: " + tokenParameter
				;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			firstPartOfToken = tokenParameter.substr(0, endOfTokenIndex);
			secondPartOfToken = tokenParameter.substr(endOfTokenIndex + separator.length());
		}

		// end with
		string tsExtension(".ts");		// hls
		string m4sExtension(".m4s");	// dash
		string m3u8Extension(".m3u8");	// m3u8
		if (
			(secondPartOfToken != "")	// secondPartOfToken is the cookie
			&&
			(
			(contentURI.size() >= tsExtension.size() && 0 == contentURI.compare(
			contentURI.size()-tsExtension.size(), tsExtension.size(), tsExtension))
			||
			(contentURI.size() >= m4sExtension.size() && 0 == contentURI.compare(
			contentURI.size()-m4sExtension.size(), m4sExtension.size(), m4sExtension))
			||
			(contentURI.size() >= m3u8Extension.size() && 0 == contentURI.compare(
			contentURI.size()-m3u8Extension.size(), m3u8Extension.size(), m3u8Extension)
			&& secondPartOfToken != "")
			)
		)
		{
			// .ts/m4s content to be authorized

			string encryptedToken = firstPartOfToken;
			string cookie = secondPartOfToken;

			if (cookie == "")
			{
				string errorMessage = string("cookie is wrong")
					+ ", contentURI: " + contentURI
					+ ", cookie: " + cookie
					+ ", firstPartOfToken: " + firstPartOfToken
					+ ", secondPartOfToken: " + secondPartOfToken
				;
				_logger->info(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			// manifestLineAndToken comes from ts URL
			string manifestLineAndToken = Encrypt::opensslDecrypt(encryptedToken);
			string manifestLine;
			// int64_t tokenComingFromURL;
			{
				string separator = "+++";
				size_t beginOfTokenIndex = manifestLineAndToken.rfind(separator);
				if (beginOfTokenIndex == string::npos)
				{
					string errorMessage = string("Wrong parameter format")
						+ ", contentURI: " + contentURI
						+ ", manifestLineAndToken: " + manifestLineAndToken
						;
					_logger->info(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				manifestLine = manifestLineAndToken.substr(0, beginOfTokenIndex);
				string sTokenComingFromURL = manifestLineAndToken.substr(beginOfTokenIndex + separator.length());
				tokenComingFromURL = stoll(sTokenComingFromURL);
			}

			string sTokenComingFromCookie = Encrypt::opensslDecrypt(cookie);
			int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

			_logger->info(__FILEREF__ + "check token info"
				+ ", encryptedToken: " + encryptedToken
				+ ", manifestLineAndToken: " + manifestLineAndToken
				+ ", manifestLine: " + manifestLine
				+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				+ ", cookie: " + cookie
				+ ", sTokenComingFromCookie: " + sTokenComingFromCookie
				+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
				+ ", contentURI: " + contentURI
			);

			if (tokenComingFromCookie != tokenComingFromURL

					// i.e., contentURI: /MMSLive/1/94/94446.ts, manifestLine: 94446.ts
					// 2020-02-04: commented because it does not work in case of dash
					// contentURI: /MMSLive/1/109/init-stream0.m4s
					// manifestLine: chunk-stream$RepresentationID$-$Number%05d$.m4s
					// || contentURI.find(manifestLine) == string::npos
			)
			{
				string errorMessage = string("Wrong parameter format")
					+ ", contentURI: " + contentURI
					+ ", manifestLine: " + manifestLine
					+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
					;
				_logger->info(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			_logger->info(__FILEREF__ + "token authorized"
				+ ", contentURI: " + contentURI
				+ ", manifestLine: " + manifestLine
				+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
			);

			// authorized
			// string responseBody;
			// sendSuccess(request, 200, responseBody);
		}
		/*
		else if (
			(contentURI.size() >= m4sExtension.size() && 0 == contentURI.compare(
			contentURI.size()-m4sExtension.size(), m4sExtension.size(), m4sExtension))
		)
		{
			// .m4s content to be authorized

			string encryptedToken = firstPartOfToken;
			string cookie = secondPartOfToken;

			// manifestLineAndToken comes from ts URL
			//string manifestLineAndToken = Encrypt::decrypt(encryptedToken);
			//string manifestLine;
			//int64_t tokenComingFromURL;
			//{
			//	string separator = "+++";
			//	size_t beginOfTokenIndex = manifestLineAndToken.rfind(separator);
			//	if (beginOfTokenIndex == string::npos)
			//	{
			//		string errorMessage = string("Wrong parameter format")
			//			+ ", contentURI: " + contentURI
			//			+ ", manifestLineAndToken: " + manifestLineAndToken
			//			;
			//		_logger->info(__FILEREF__ + errorMessage);
//
//					throw runtime_error(errorMessage);
//				}
//				manifestLine = manifestLineAndToken.substr(0, beginOfTokenIndex);
//				string sTokenComingFromURL = manifestLineAndToken.substr(beginOfTokenIndex + separator.length());
//				tokenComingFromURL = stoll(sTokenComingFromURL);
//			}

			string sTokenComingFromCookie = Encrypt::decrypt(cookie);
			int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

//			if (tokenComingFromCookie != tokenComingFromURL
//
//					// i.e., contentURI: /MMSLive/1/94/94446.ts, manifestLine: 94446.ts
//					|| contentURI.find(manifestLine) == string::npos)
//			{
//				string errorMessage = string("Wrong parameter format")
//					+ ", contentURI: " + contentURI
//					+ ", manifestLine: " + manifestLine
//					+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
//					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
//					;
//				_logger->info(__FILEREF__ + errorMessage);
//
//				throw runtime_error(errorMessage);
//			}

			_logger->info(__FILEREF__ + "token authorized"
				+ ", contentURI: " + contentURI
				// + ", manifestLine: " + manifestLine
				// + ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
			);

			string responseBody;
			sendSuccess(request, 200, responseBody);
		}
		*/
		else
		{
			tokenComingFromURL = stoll(firstPartOfToken);
			if (_mmsEngineDBFacade->checkDeliveryAuthorization(tokenComingFromURL, contentURI))
			{
				_logger->info(__FILEREF__ + "token authorized"
					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				);

				// authorized

				// string responseBody;
				// sendSuccess(request, 200, responseBody);
			}
			else
			{
				string errorMessage = string("Not authorized: token invalid")
					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
                          ;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("Not authorized");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch(exception e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}

	return tokenComingFromURL;
}

int64_t API::checkDeliveryAuthorizationThroughPath(
	string contentURI)
{
	int64_t tokenComingFromURL = -1;
	try
	{
		_logger->info(__FILEREF__ + "checkDeliveryAuthorizationThroughPath, received"
			+ ", contentURI: " + contentURI
		);

		string tokenLabel = "/token_";

		size_t startTokenIndex = contentURI.find("/token_");
		if (startTokenIndex == string::npos)
		{
			string errorMessage = string("Wrong token format")
				+ ", contentURI: " + contentURI
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		startTokenIndex += tokenLabel.size();

		size_t endTokenIndex = contentURI.find(",", startTokenIndex);
		if (endTokenIndex == string::npos)
		{
			string errorMessage = string("Wrong token format")
				+ ", contentURI: " + contentURI
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t endExpirationIndex = contentURI.find("/", endTokenIndex);
		if (endExpirationIndex == string::npos)
		{
			string errorMessage = string("Wrong token format")
				+ ", contentURI: " + contentURI
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string tokenSigned = contentURI.substr(startTokenIndex, endTokenIndex - startTokenIndex);
		string sExpirationTime = contentURI.substr(endTokenIndex + 1, endExpirationIndex - (endTokenIndex + 1));
		time_t expirationTime = stoll(sExpirationTime);



		string contentURIToBeVerified;

		size_t endContentURIIndex = contentURI.find("?", endExpirationIndex);
		if (endContentURIIndex == string::npos)
			contentURIToBeVerified = contentURI.substr(endExpirationIndex);
		else
			contentURIToBeVerified = contentURI.substr(endExpirationIndex, endContentURIIndex - endExpirationIndex);

		string m3u8Suffix(".m3u8");
		string tsSuffix(".ts");
		if (contentURIToBeVerified.size() >= m3u8Suffix.size()
			&& 0 == contentURIToBeVerified.compare(contentURIToBeVerified.size()-m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
		{
			{
				size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
				if (endPathIndex != string::npos)
					contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
			}

			string md5Base64 = _mmsDeliveryAuthorization->getSignedMMSPath(contentURIToBeVerified, expirationTime);

			_logger->info(__FILEREF__ + "Authorization through path (m3u8)"
				+ ", contentURI: " + contentURI
				+ ", contentURIToBeVerified: " + contentURIToBeVerified
				+ ", expirationTime: " + to_string(expirationTime)
				+ ", tokenSigned: " + tokenSigned
				+ ", md5Base64: " + md5Base64
			);

			if (md5Base64 != tokenSigned)
			{
				// we still try removing again the last directory to manage the scenario
				// of multi bitrate encoding or multi audio tracks (one director for each audio)
				{
					size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
					if (endPathIndex != string::npos)
						contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
				}

				string md5Base64 = _mmsDeliveryAuthorization->getSignedMMSPath(contentURIToBeVerified, expirationTime);

				_logger->info(__FILEREF__ + "Authorization through path (m3u8 2)"
					+ ", contentURI: " + contentURI
					+ ", contentURIToBeVerified: " + contentURIToBeVerified
					+ ", expirationTime: " + to_string(expirationTime)
					+ ", tokenSigned: " + tokenSigned
					+ ", md5Base64: " + md5Base64
				);

				if (md5Base64 != tokenSigned)
				{
					string errorMessage = string("Wrong token (m3u8)")
						+ ", md5Base64: " + md5Base64
						+ ", tokenSigned: " + tokenSigned
					;
					_logger->warn(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
		else if (contentURIToBeVerified.size() >= tsSuffix.size()
			&& 0 == contentURIToBeVerified.compare(contentURIToBeVerified.size()-tsSuffix.size(), tsSuffix.size(), tsSuffix)
		)
		{
			// 2022-11-29: ci sono 3 casi per il download di un .ts:
			//	1. download NON dall'interno di un m3u8
			//	2. download dall'interno di un m3u8 single bitrate
			//	3. download dall'interno di un m3u8 multi bitrate

			{
				// check caso 1.
				string md5Base64 = _mmsDeliveryAuthorization->getSignedMMSPath(contentURIToBeVerified,
					expirationTime);

				_logger->info(__FILEREF__ + "Authorization through path"
					+ ", contentURI: " + contentURI
					+ ", contentURIToBeVerified: " + contentURIToBeVerified
					+ ", expirationTime: " + to_string(expirationTime)
					+ ", tokenSigned: " + tokenSigned
					+ ", md5Base64: " + md5Base64
				);

				if (md5Base64 != tokenSigned)
				{
					// potremmo essere nel caso 2 o caso 3

					{
						size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
						if (endPathIndex != string::npos)
							contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
					}

					// check caso 2.
					md5Base64 = _mmsDeliveryAuthorization->getSignedMMSPath(contentURIToBeVerified,
						expirationTime);

					_logger->info(__FILEREF__ + "Authorization through path (ts 1)"
						+ ", contentURI: " + contentURI
						+ ", contentURIToBeVerified: " + contentURIToBeVerified
						+ ", expirationTime: " + to_string(expirationTime)
						+ ", tokenSigned: " + tokenSigned
						+ ", md5Base64: " + md5Base64
					);

					if (md5Base64 != tokenSigned)
					{
						// dovremmo essere nel caso 3

						// we still try removing again the last directory to manage
						// the scenario of multi bitrate encoding
						{
							size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
							if (endPathIndex != string::npos)
								contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
						}

						// check caso 3.
						string md5Base64 = _mmsDeliveryAuthorization->getSignedMMSPath(contentURIToBeVerified,
							expirationTime);

						_logger->info(__FILEREF__ + "Authorization through path (ts 2)"
							+ ", contentURI: " + contentURI
							+ ", contentURIToBeVerified: " + contentURIToBeVerified
							+ ", expirationTime: " + to_string(expirationTime)
							+ ", tokenSigned: " + tokenSigned
							+ ", md5Base64: " + md5Base64
						);

						if (md5Base64 != tokenSigned)
						{
							// non siamo in nessuno dei 3 casi

							string errorMessage = string("Wrong token (ts)")
								+ ", md5Base64: " + md5Base64
								+ ", tokenSigned: " + tokenSigned
							;
							_logger->warn(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}
			}
		}
		else
		{
			string md5Base64 = _mmsDeliveryAuthorization->getSignedMMSPath(contentURIToBeVerified, expirationTime);

			_logger->info(__FILEREF__ + "Authorization through path"
				+ ", contentURI: " + contentURI
				+ ", contentURIToBeVerified: " + contentURIToBeVerified
				+ ", expirationTime: " + to_string(expirationTime)
				+ ", tokenSigned: " + tokenSigned
				+ ", md5Base64: " + md5Base64
			);

			if (md5Base64 != tokenSigned)
			{
				string errorMessage = string("Wrong token")
					+ ", md5Base64: " + md5Base64
					+ ", tokenSigned: " + tokenSigned
				;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}


		time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
		if (expirationTime < utcNow)
		{
			string errorMessage = string("Token expired")
				+ ", expirationTime: " + to_string(expirationTime)
				+ ", utcNow: " + to_string(utcNow)
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("Not authorized");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch(exception e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}

	return tokenComingFromURL;
}

