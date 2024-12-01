
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
#include "catralibraries/StringUtils.h"

void MMSEngineProcessor::localCopyContentThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "localCopyContentThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be copied" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string localPath;
		string localFileName;
		{
			string field = "LocalPath";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			localPath = JSONUtils::asString(parametersRoot, field, "");

			field = "LocalFileName";
			localFileName = JSONUtils::asString(parametersRoot, field, "");
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;
				string fileFormat;
				int64_t physicalPathKey;
				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key, encodingProfileKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) =
						physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;
				}
				else
				{
					physicalPathKey = key;

					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;
				}

				copyContent(ingestionJobKey, mmsAssetPathName, localPath, localFileName);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "local copy Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = fmt::format(
					"local copy Content failed"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencyIndex: {}"
					", dependencies.size(): {}",
					_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size()
				);
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::copyContent(int64_t ingestionJobKey, string mmsAssetPathName, string localPath, string localFileName)
{

	try
	{
		SPDLOG_INFO(
			string() + "copyContent" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		string localPathName = localPath;
		if (localFileName != "")
		{
			string cleanedFileName;
			{
				cleanedFileName.resize(localFileName.size());
				transform(
					localFileName.begin(), localFileName.end(), cleanedFileName.begin(),
					[](unsigned char c)
					{
						if (c == '/')
							return (int)' ';
						else
							return (int)c;
					}
				);

				string fileFormat;
				{
					size_t extensionIndex = mmsAssetPathName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = string() +
											  "No fileFormat (extension of the file) found in "
											  "mmsAssetPathName" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
				}

				string suffix = "." + fileFormat;
				// if (cleanedFileName.size() >= suffix.size() &&
				//	0 == cleanedFileName.compare(cleanedFileName.size() - suffix.size(), suffix.size(), suffix))
				if (cleanedFileName.ends_with(suffix))
					;
				else
					cleanedFileName += suffix;

				string prefix = "MMS ";
				cleanedFileName = prefix + cleanedFileName;
			}

			if (localPathName.back() != '/')
				localPathName += "/";
			localPathName += cleanedFileName;
		}

		SPDLOG_INFO(
			string() + "Coping" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", mmsAssetPathName: " + mmsAssetPathName + ", localPath: " + localPath + ", localFileName: " + localFileName +
			", localPathName: " + localPathName
		);

		fs::copy(mmsAssetPathName, localPathName, fs::copy_options::recursive);
	}
	catch (runtime_error &e)
	{
		string errorMessage = string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName +
							  ", localPath: " + localPath + ", localFileName: " + localFileName + ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName +
							  ", localPath: " + localPath + ", localFileName: " + localFileName + ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

void MMSEngineProcessor::moveMediaSourceFileThread(
	shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, int m3u8TarGzOrStreaming, int64_t ingestionJobKey,
	shared_ptr<Workspace> workspace
)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "moveMediaSourceFileThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "moveMediaSourceFileThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
		string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
		// 0: no m3u8
		// 1: m3u8 by .tar.gz
		// 2: m3u8 by streaming (it will be saved as .mp4)
		if (m3u8TarGzOrStreaming == 1)
			destBinaryPathName = destBinaryPathName + ".tar.gz";

		string movePrefix("move://");
		string mvPrefix("mv://");
		// if (!(sourceReferenceURL.size() >= movePrefix.size() && 0 == sourceReferenceURL.compare(0, movePrefix.size(), movePrefix)) &&
		// 	!(sourceReferenceURL.size() >= mvPrefix.size() && 0 == sourceReferenceURL.compare(0, mvPrefix.size(), mvPrefix)))
		if (!sourceReferenceURL.starts_with(movePrefix) && !sourceReferenceURL.starts_with(mvPrefix))
		{
			string errorMessage = string("sourceReferenceURL is not a move reference") +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourceReferenceURL: " + sourceReferenceURL;

			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourcePathName;
		if (sourceReferenceURL.starts_with(movePrefix))
			// if (sourceReferenceURL.size() >= movePrefix.size() && 0 == sourceReferenceURL.compare(0, movePrefix.size(), movePrefix))
			sourcePathName = sourceReferenceURL.substr(movePrefix.length());
		else
			sourcePathName = sourceReferenceURL.substr(mvPrefix.length());

		SPDLOG_INFO(
			string() + "Moving" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName
		);

		int64_t elapsedInSeconds = MMSStorage::move(ingestionJobKey, sourcePathName, destBinaryPathName, _logger);

		// 0: no m3u8
		// 1: m3u8 by .tar.gz
		// 2: m3u8 by streaming (it will be saved as .mp4)
		if (m3u8TarGzOrStreaming)
		{
			try
			{
				_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
					ingestionJobKey, destBinaryPathName, workspaceIngestionRepository, sourcePathName
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed") +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL;

				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey)
			// + ", movingCompleted: " + to_string(true)
			+ ", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName +
			", @MMS MOVE statistics@ - movingDuration (secs): @" + to_string(elapsedInSeconds) + "@"
		);
		_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, true);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "Moving failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			string() + "Moving failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}

void MMSEngineProcessor::copyMediaSourceFileThread(
	shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, int m3u8TarGzOrStreaming, int64_t ingestionJobKey,
	shared_ptr<Workspace> workspace
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "copyMediaSourceFileThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "copyMediaSourceFileThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
		string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
		// 0: no m3u8
		// 1: m3u8 by .tar.gz
		// 2: m3u8 by streaming (it will be saved as .mp4)
		if (m3u8TarGzOrStreaming == 1)
			destBinaryPathName = destBinaryPathName + ".tar.gz";

		string copyPrefix("copy://");
		string cpPrefix("cp://");
		if (!(sourceReferenceURL.size() >= copyPrefix.size() && 0 == sourceReferenceURL.compare(0, copyPrefix.size(), copyPrefix)) &&
			!(sourceReferenceURL.size() >= cpPrefix.size() && 0 == sourceReferenceURL.compare(0, cpPrefix.size(), cpPrefix)))
		{
			string errorMessage = string("sourceReferenceURL is not a copy reference") +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourceReferenceURL: " + sourceReferenceURL;

			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourcePathName;
		if (sourceReferenceURL.size() >= copyPrefix.size() && 0 == sourceReferenceURL.compare(0, copyPrefix.size(), copyPrefix))
			sourcePathName = sourceReferenceURL.substr(copyPrefix.length());
		else
			sourcePathName = sourceReferenceURL.substr(cpPrefix.length());

		SPDLOG_INFO(
			string() + "Coping" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName
		);

		chrono::system_clock::time_point startCoping = chrono::system_clock::now();
		fs::copy(sourcePathName, destBinaryPathName, fs::copy_options::recursive);
		chrono::system_clock::time_point endCoping = chrono::system_clock::now();

		if (m3u8TarGzOrStreaming == 1)
		{
			try
			{
				_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
					ingestionJobKey, destBinaryPathName, workspaceIngestionRepository, sourcePathName
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed") +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL;

				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey)
			// + ", movingCompleted: " + to_string(true)
			+ ", sourcePathName: " + sourcePathName + ", destBinaryPathName: " + destBinaryPathName +
			", @MMS COPY statistics@ - copingDuration (secs): @" +
			to_string(chrono::duration_cast<chrono::seconds>(endCoping - startCoping).count()) + "@"
		);

		_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, true);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			string() + "Coping failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}
