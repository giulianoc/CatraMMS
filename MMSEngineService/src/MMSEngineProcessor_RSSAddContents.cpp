
#include "CurlWrapper.h"
#include "Datetime.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "XMLWrapper.h"
#include "JsonPath.h"
#include "MMSEngineProcessor.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"

#include <numbers>

using namespace std;
using json = nlohmann::json;

void MMSEngineProcessor::manageRSSAddContentsTask(int64_t ingestionJobKey, const shared_ptr<Workspace>& workspace, const json& parametersRoot)
{
	try
	{
		LOG_INFO(
			"manageRSSAddContents"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", _processorsThreadsNumber.use_count(): {}",
			_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
		);

		constexpr int retentionInMinutes = 60 * 24 * 5; // 5 days

		// recupero le info dal feed rss
		vector<tuple<string, string, string, string>> rssContents;
		string rssTitle;
		{
			vector<pair<string, string>> nameServices;

			XMLWrapper rss;
			rss.loadXML(JsonPath(&parametersRoot)["rssFeedURL"].as<string>(),
				nameServices,
				JsonPath(&parametersRoot)["timeoutInSeconds"].as<int16_t>(30)
			);

			xmlNodePtr rootNode = rss.asRootNode();

			bool emptyOnError = true;
			rssTitle = rss.asText("/rss/channel/title/text()", rootNode, emptyOnError);

			xmlXPathObjectPtr resultToBeFreed = rss.xPath("/rss/channel/item", rootNode, emptyOnError);
			struct XpGuard {
				xmlXPathObjectPtr _resultToBeFreed;
				~XpGuard()
				{
					if(_resultToBeFreed)
						xmlXPathFreeObject(_resultToBeFreed);
				}
			} guard{resultToBeFreed};
			for (int nodeIndex = 0; nodeIndex < resultToBeFreed->nodesetval->nodeNr; nodeIndex++)
			{
				xmlNodePtr itemNode = resultToBeFreed->nodesetval->nodeTab[nodeIndex];

				/*
				<title><![CDATA[Carburanti, corsa prezzi mette alle strette governo]]></title>
				<link><![CDATA[https://www.adnkronos.com/multimedia/news-to-go/carburanti-corsa-prezzi-mette-alle-strette-governo_3Mo3PkFx6BHjTDFnPU68Ms]]></link>
				<description><![CDATA[<p> (Adnkronos) - La corsa dei prezzi di diesel e benzina mette alle strette il governo. Sono attese infatti misure contro il caro carburanti in Cdm. Il Consiglio dei ministri è convocato oggi, alle 17.30 a Palazzo Chigi, si legge in una nota. </p>
	]]></description>
				<category>multimedia/news-to-go</category>
				<author>webinfo@adnkronos.com (Web Info)</author>
				<enclosure type="video/mp4" length="0" url="https://video.adnkronos.com/Assets/Video/20260727/carburanti costo_video-hd.mp4"></enclosure>
				<LinkedVideo>https://www.adnkronos.com/multimedia/news-to-go/carburanti-corsa-prezzi-mette-alle-strette-governo_3Mo3PkFx6BHjTDFnPU68Ms</LinkedVideo>
				<enclosure type="image/jpeg" url="https://video.adnkronos.com/Assets/Video/20260727/Thumbs/carburanti costo_video-hd.jpg">
					02a7-213453cb0e1b-920c7a33e961-1000
					<caption></caption>
					<credit></credit>
				</enclosure>
				<guid>https://www.adnkronos.com/multimedia/news-to-go/carburanti-corsa-prezzi-mette-alle-strette-governo_3Mo3PkFx6BHjTDFnPU68Ms</guid>
				<pubDate>Mon, 27 Jul 2026 13:55:00 +0200</pubDate>
				<uuid>02a7-213453cb0e1b-920c7a33e961-1000</uuid>
				*/
				string title = rss.asText("title/text()", itemNode, emptyOnError);

				string videoURL = rss.asAttribute("enclosure[@type='video/mp4']", "url", itemNode, emptyOnError);
				if (videoURL.empty())
					continue;

				string imageURL = rss.asAttribute("enclosure[@type='image/jpeg']", "url", itemNode, emptyOnError);

				// string htmlDescription = rss.asText("description/text()", itemNode, emptyOnError);

				string uuid = rss.asText("uuid/text()", itemNode, emptyOnError);

				try
				{
					auto [mediaItemKey, _] = _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
						workspace->_workspaceKey, uuid);
					// se il mediaItem è presente viene aggiornato il suo retention
					_mmsEngineDBFacade->updateMediaItem(workspace->_workspaceKey, mediaItemKey, false, "", false,
						"", true, retentionInMinutes,
						false, nullptr, false, "",
						nullptr, false);
				}
				catch (MediaItemKeyNotFound &e)
				{
					rssContents.emplace_back(title, videoURL, imageURL, uuid);
				}
			}
		}

		LOG_INFO(
			"Preparing workflow to ingest..."
			", _processorIdentifier: {}"
			", ingestionJobKey: {}",
			_processorIdentifier, ingestionJobKey
		);

		json onSuccessRoot = nullptr;
		json onErrorRoot = nullptr;
		json onCompleteRoot = nullptr;
		int64_t userKey;
		string apiKey;
		{
			if (JsonPath(&parametersRoot)["internalMMS"].exists())
			{
				json internalMMSRoot = JsonPath(&parametersRoot)["internalMMS"].as<json>();

				if (JsonPath(&internalMMSRoot)["credentials"].exists())
				{
					json credentialsRoot = JsonPath(&internalMMSRoot)["credentials"].as<json>();

					userKey = JsonPath(&credentialsRoot)["userKey"].as<int64_t>();

					auto apiKeyEncrypted = JsonPath(&credentialsRoot)["apiKey"].as<string>();
					apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
				}

				if (JsonPath(&internalMMSRoot)["events"].exists())
				{
					json eventsRoot = JsonPath(&internalMMSRoot)["events"].as<json>();

					if (JsonPath(&eventsRoot)["onSuccess"].exists())
						onSuccessRoot = JsonPath(&eventsRoot)["onSuccess"].as<json>();
					if (JsonPath(&eventsRoot)["onError"].exists())
						onErrorRoot = JsonPath(&eventsRoot)["onError"].as<json>();
					if (JsonPath(&eventsRoot)["onComplete"].exists())
						onCompleteRoot = JsonPath(&eventsRoot)["onComplete"].as<json>();
				}
			}
		}

		// create workflow to ingest
		// creo tanti Add-Content tasks e li aggiungo al GroupOfTasks
		json addContentTasksGroupParametersRoot;
		{
			json tasksRoot = json::array();
			for (auto &[title, videoURL, imageURL, uuid]: rssContents)
			{
				json addContentRoot;
				addContentRoot["type"] = "Add-Content";
				addContentRoot["label"] = std::format("From RSS feed: {}", title);

				// aggiungere l'immagine

				json addContentParametersRoot;
				addContentParametersRoot["title"] = title;
				addContentParametersRoot["sourceURL"] = videoURL;
				addContentParametersRoot["fileFormat"] = "mp4";
				addContentParametersRoot["retention"] = std::format("{}m", retentionInMinutes);
				addContentParametersRoot["uniqueName"] = uuid;
				addContentRoot["parameters"] = addContentParametersRoot;

				tasksRoot.push_back(addContentRoot);
			}
			addContentTasksGroupParametersRoot["tasks"] = tasksRoot;
		}

		// creo il groupOfTaks
		json addContentTasksGroupRoot;
		{
			addContentTasksGroupRoot["type"] = "GroupOfTasks";

			addContentTasksGroupParametersRoot["executionType"] = "parallel";
			addContentTasksGroupRoot["parameters"] = addContentTasksGroupParametersRoot;

			if (onSuccessRoot != nullptr)
				addContentTasksGroupRoot["onSuccess"] = onSuccessRoot;
			if (onErrorRoot != nullptr)
				addContentTasksGroupRoot["onError"] = onErrorRoot;
			if (onCompleteRoot != nullptr)
				addContentTasksGroupRoot["onComplete"] = onCompleteRoot;
		}

		// creo il workflow root
		json workflowRoot;
		{
			workflowRoot["label"] = std::format(
				"RSS feed ({}): {}", Datetime::dateTimeFormat(chrono::system_clock::now()), rssTitle
			);

			workflowRoot["type"] = "Workflow";
			workflowRoot["task"] = addContentTasksGroupRoot;
		}

		string workflowMetadata = JSONUtils::toString(workflowRoot);

		vector<string> otherHeaders;
		json workflowResponseRoot = CurlWrapper::httpPostStringAndGetJson(
			_mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey),
			workflowMetadata, "application/json", // contentType
			otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey)
		);

		LOG_INFO(
			"Update IngestionJob"
			", ingestionJobKey: {}"
			", IngestionStatus: End_TaskSuccess"
			", errorMessage: ",
			ingestionJobKey
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"manageRSSAddContents failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", exception: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		LOG_INFO(
			"Update IngestionJob"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", IngestionStatus: End_IngestionFailure"
			", errorMessage: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
				e.what());
		}
		catch (exception &ex)
		{
			LOG_INFO(
				"Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", exception: {}",
				_processorIdentifier, ingestionJobKey, ex.what()
			);
		}

		throw;
	}
}
