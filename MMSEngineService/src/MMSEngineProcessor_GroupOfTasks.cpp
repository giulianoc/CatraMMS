
#include "MMSEngineProcessor.h"
/*
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "FFMpeg.h"
#include "GEOInfoTimes.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "PersistenceLock.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

void MMSEngineProcessor::manageGroupOfTasks(int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot)
{
	try
	{
		vector<pair<int64_t, int64_t>> referencesOutput;

		Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
		// ReferencesOutput tag is always present:
		// 1. because it is already set by the Workflow (by the user)
		// 2. because it is automatically set by API_Ingestion.cpp using the
		// list of Tasks.
		//	This is when it was not found into the Workflow
		validator.fillReferencesOutput(workspace->_workspaceKey, parametersRoot, referencesOutput);

		int64_t liveRecordingIngestionJobKey = -1;
		for (pair<int64_t, int64_t> referenceOutput : referencesOutput)
		{
			/*
			 * 2020-06-08. I saw a scenario where:
			 *	1. MediaItems were coming from a LiveRecorder with high
			 *availability
			 *	2. a media item was present during
			 *validator.fillReferencesOutput
			 *	3. just before the calling of the below statement
			 *_mmsEngineDBFacade->addIngestionJobOutput it was removed (because
			 *it was not validated
			 *	4. _mmsEngineDBFacade->addIngestionJobOutput raised an exception
			 *		Cannot add or update a child row: a foreign key constraint
			 *fails
			 *		(`vedatest`.`MMS_IngestionJobOutput`, CONSTRAINT
			 *`MMS_IngestionJobOutput_FK` FOREIGN KEY (`physicalPathKey`)
			 *REFERENCES `MMS_PhysicalPath` (`physicalPathKey`) ON DELETE
			 *CASCADE) This scenario should never happen because the
			 *EncoderVideoAudioProxy::processLiveRecorder method wait that the
			 *high availability is completely managed.
			 *
			 * Anyway to be sure we will not interrupt our workflow, we will add
			 *a try catch
			 */
			try
			{
				SPDLOG_INFO(
					string() + "References.Output" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", mediaItemKey: " + to_string(referenceOutput.first) + ", physicalPathKey: " + to_string(referenceOutput.second)
				);

				_mmsEngineDBFacade->addIngestionJobOutput(
					ingestionJobKey, referenceOutput.first, referenceOutput.second, liveRecordingIngestionJobKey
				);
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "_mmsEngineDBFacade->addIngestionJobOutput failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(referenceOutput.first) +
					", physicalPathKey: " + to_string(referenceOutput.second) + ", e.what(): " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "_mmsEngineDBFacade->addIngestionJobOutput failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(referenceOutput.first) +
					", physicalPathKey: " + to_string(referenceOutput.second)
				);
			}
		}

		/*
		 * 2019-09-23: It is not clean now how to manage the status of the
		 *GroupOfTasks:
		 *	- depend on the status of his children (first level of Tasks of the
		 *GroupOfTasks) as calculated below (now commented)?
		 *	- depend on the ReferencesOutput?
		 *
		 *	Since this is not clean, I left it always Success
		 *
		 */
		/*
		// GroupOfTasks Ingestion Status is by default Failure;
		// It will be Success if at least just one Status of the children is
		Success MMSEngineDBFacade::IngestionStatus groupOfTasksIngestionStatus
			= MMSEngineDBFacade::IngestionStatus::End_IngestionFailure;
		{
			vector<pair<int64_t, MMSEngineDBFacade::IngestionStatus>>
		groupOfTasksChildrenStatus;

			_mmsEngineDBFacade->getGroupOfTasksChildrenStatus(ingestionJobKey,
		groupOfTasksChildrenStatus);

			for (pair<int64_t, MMSEngineDBFacade::IngestionStatus>
		groupOfTasksChildStatus: groupOfTasksChildrenStatus)
			{
				int64_t childIngestionJobKey = groupOfTasksChildStatus.first;
				MMSEngineDBFacade::IngestionStatus childStatus =
		groupOfTasksChildStatus.second;

				SPDLOG_INFO(string() + "manageGroupOfTasks, child status"
						+ ", group of tasks ingestionJobKey: " +
		to_string(ingestionJobKey)
						+ ", childIngestionJobKey: " +
		to_string(childIngestionJobKey)
						+ ", IngestionStatus: " +
		MMSEngineDBFacade::toString(childStatus)
				);

				if
		(!MMSEngineDBFacade::isIngestionStatusFinalState(childStatus))
				{
					SPDLOG_ERROR(string() + "manageGroupOfTasks, child
		status is not a final status. It should never happens because when this
		GroupOfTasks is executed, all the children should be finished"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " +
		MMSEngineDBFacade::toString(childStatus)
					);

					continue;
				}

				if (childStatus ==
		MMSEngineDBFacade::IngestionStatus::End_TaskSuccess)
				{
					groupOfTasksIngestionStatus =
		MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

					break;
				}
			}
		}
		*/
		MMSEngineDBFacade::IngestionStatus groupOfTasksIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

		string errorMessage = "";
		if (groupOfTasksIngestionStatus != MMSEngineDBFacade::IngestionStatus::End_TaskSuccess)
			errorMessage = "Failed because there is no one child with Status Success";

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", IngestionStatus: " + MMSEngineDBFacade::toString(groupOfTasksIngestionStatus) + ", errorMessage: " + errorMessage
		);
		_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, groupOfTasksIngestionStatus, errorMessage);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

