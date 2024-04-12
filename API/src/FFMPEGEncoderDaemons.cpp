
#include "FFMPEGEncoderDaemons.h"

#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineDBFacade.h"
#include <sstream>
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/DateTime.h"

FFMPEGEncoderDaemons::FFMPEGEncoderDaemons(
	json configurationRoot,
	mutex* liveRecordingMutex,
	vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>* liveRecordingsCapability,
	mutex* liveProxyMutex,
	vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>>* liveProxiesCapability,
	mutex* cpuUsageMutex,
	deque<int>* cpuUsage,
	shared_ptr<spdlog::logger> logger):
	FFMPEGEncoderBase(configurationRoot, logger)
{
	try
	{
		_liveRecordingMutex			= liveRecordingMutex;
		_liveRecordingsCapability	= liveRecordingsCapability;
		_liveProxyMutex				= liveProxyMutex;
		_liveProxiesCapability		= liveProxiesCapability;
        _cpuUsageMutex				= cpuUsageMutex;
		_cpuUsage					= cpuUsage;

		_monitorThreadShutdown = false;
		_cpuUsageThreadShutdown = false;

		_monitorCheckInSeconds =  JSONUtils::asInt(configurationRoot["ffmpeg"], "monitorCheckInSeconds", 5);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", ffmpeg->monitorCheckInSeconds: " + to_string(_monitorCheckInSeconds)
		);
	}
	catch(runtime_error& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

FFMPEGEncoderDaemons::~FFMPEGEncoderDaemons()
{
	try
	{
	}
	catch(runtime_error& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

void FFMPEGEncoderDaemons::startMonitorThread()
{

	while(!_monitorThreadShutdown)
	{
		// proxy
		try
		{
			// this is to have a copy of LiveProxyAndGrid
			vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> copiedRunningLiveProxiesCapability;

			// this is to have access to running and _proxyStart
			//	to check if it is changed. In case the process is killed, it will access
			//	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> sourceLiveProxiesCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveProxyAndGridRunningCounter = 0;
			{
				lock_guard<mutex> locker(*_liveProxyMutex);

				int liveProxyAndGridNotRunningCounter = 0;

				for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
				{
					if (liveProxy->_childPid != 0)	// running
					{
						liveProxyAndGridRunningCounter++;

						copiedRunningLiveProxiesCapability.push_back(
							liveProxy->cloneForMonitor());
						sourceLiveProxiesCapability.push_back(
                            liveProxy);
					}
					else
					{
						liveProxyAndGridNotRunningCounter++;
					}
				}
				_logger->info(__FILEREF__ + "liveProxyMonitor, numbers"
					+ ", total LiveProxyAndGrid: " + to_string(liveProxyAndGridRunningCounter + liveProxyAndGridNotRunningCounter)
					+ ", liveProxyAndGridRunningCounter: " + to_string(liveProxyAndGridRunningCounter)
					+ ", liveProxyAndGridNotRunningCounter: " + to_string(liveProxyAndGridNotRunningCounter)
				);
			}
			_logger->info(__FILEREF__ + "liveProxyMonitor clone"
				+ ", copiedRunningLiveProxiesCapability.size: " + to_string(copiedRunningLiveProxiesCapability.size())
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - startClone).count())
			);

			chrono::system_clock::time_point monitorStart = chrono::system_clock::now();

			for (int liveProxyIndex = 0;
				liveProxyIndex < copiedRunningLiveProxiesCapability.size();
				liveProxyIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> copiedLiveProxy
					= copiedRunningLiveProxiesCapability[liveProxyIndex];
				shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> sourceLiveProxy
					= sourceLiveProxiesCapability[liveProxyIndex];

				// this is just for logging
				string configurationLabel;
				if (copiedLiveProxy->_inputsRoot.size() > 0)
				{
					json inputRoot = copiedLiveProxy->_inputsRoot[0];
					string field = "streamInput";
					if (JSONUtils::isMetadataPresent(inputRoot, field))
					{
						json streamInputRoot = inputRoot[field];
						field = "configurationLabel";
						configurationLabel = JSONUtils::asString(streamInputRoot, field, "");
					}
				}

				_logger->info(__FILEREF__ + "liveProxyMonitor start"
					+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
					+ ", configurationLabel: " + configurationLabel
					+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
					+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): "
						+ to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
					+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): "
						+ to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
				);

				chrono::system_clock::time_point now = chrono::system_clock::now();

				bool liveProxyWorking = true;
				string localErrorMessage;

				if (sourceLiveProxy->_childPid == 0 ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				{
					// copiedLiveProxy->_proxyStart could be a bit in the future
					int64_t liveProxyLiveTimeInMinutes;
					if (now > copiedLiveProxy->_proxyStart)
						liveProxyLiveTimeInMinutes = chrono::duration_cast<
							chrono::minutes>(now - copiedLiveProxy->_proxyStart).count();
					else	// it will be negative
						liveProxyLiveTimeInMinutes = chrono::duration_cast<
							chrono::minutes>(now - copiedLiveProxy->_proxyStart).count();

					// checks are done after 3 minutes LiveProxy started,
					// in order to be sure the manifest file was already created
					if (liveProxyLiveTimeInMinutes <= 3)
					{
						_logger->info(__FILEREF__
							+ "liveProxyMonitor. Checks are not done because too early"
							+ ", ingestionJobKey: "
								+ to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", encodingJobKey: "
								+ to_string(copiedLiveProxy->_encodingJobKey)
							+ ", liveProxyLiveTimeInMinutes: "
								+ to_string(liveProxyLiveTimeInMinutes)
						);

						continue;
					}
				}

				if (sourceLiveProxy->_childPid == 0 ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				// First health check
				//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
				//		rtmp(Proxy)/SRT(Grid):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
				bool rtmpOutputFound = false;
				if (liveProxyWorking)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor manifest check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					for(int outputIndex = 0; outputIndex < copiedLiveProxy->_outputsRoot.size();
						outputIndex++)
					{
						json outputRoot = copiedLiveProxy->_outputsRoot[outputIndex];

						string outputType = JSONUtils::asString(outputRoot, "outputType", "");

						if (!liveProxyWorking)
							break;

						// if (outputType == "HLS" || outputType == "DASH")
						if (outputType == "HLS_Channel")
						{
							string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
							string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");

							try
							{
								// First health check (HLS/DASH) looking the manifests path name timestamp
								{
									string manifestFilePathName =
										manifestDirectoryPath + "/" + manifestFileName;
									{
										if(!fs::exists(manifestFilePathName))
										{
											liveProxyWorking = false;

											_logger->error(__FILEREF__ + "liveProxyMonitor. Manifest file does not exist"
												+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName
											);

											localErrorMessage = " restarted because of 'manifest file is missing'";

											break;
										}
										else
										{
											int64_t lastManifestFileUpdateInSeconds;
											{
												chrono::system_clock::time_point fileLastModification =
													chrono::time_point_cast<chrono::system_clock::duration>(
														fs::last_write_time(manifestFilePathName) - fs::file_time_type::clock::now() + chrono::system_clock::now());
												chrono::system_clock::time_point now = chrono::system_clock::now();

												lastManifestFileUpdateInSeconds =
													chrono::duration_cast<chrono::seconds>(now - fileLastModification).count();
											}

											long maxLastManifestFileUpdateInSeconds = 30;

											if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
											{
												liveProxyWorking = false;

												_logger->error(__FILEREF__ + "liveProxyMonitor. Manifest file was not updated "
													+ "in the last " + to_string(maxLastManifestFileUpdateInSeconds) + " seconds"
													+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
													+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
													+ ", manifestFilePathName: " + manifestFilePathName
													+ ", lastManifestFileUpdateInSeconds: " + to_string(lastManifestFileUpdateInSeconds) + " seconds ago"
													+ ", maxLastManifestFileUpdateInSeconds: " + to_string(maxLastManifestFileUpdateInSeconds)
												);

												localErrorMessage = " restarted because of 'manifest file was not updated'";

												break;
											}
										}
									}
								}
							}
							catch(runtime_error& e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on manifest path name failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception& e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on manifest path name failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
						else	// rtmp (Proxy) or SRT (Grid)
						{
							rtmpOutputFound = true;
						}
					}
				}

				if (sourceLiveProxy->_childPid == 0 ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveProxyWorking && rtmpOutputFound)
				{
					try
					{
						_logger->info(__FILEREF__ + "liveProxyMonitor nonMonotonousDTSInOutputLog check"
							+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
						);

						// First health check (rtmp), looks the log and check there is no message like
						//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
						//	This message causes proxy not working
						if (sourceLiveProxy->_ffmpeg->nonMonotonousDTSInOutputLog())
						{
							liveProxyWorking = false;

							_logger->error(__FILEREF__ + "liveProxyMonitor (rtmp). Live Proxy is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. LiveProxy (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
								// + ", channelLabel: " + copiedLiveProxy->_channelLabel
								+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							);

							localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
						}
					}
					catch(runtime_error& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) Non-monotonous DTS failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) Non-monotonous DTS failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (sourceLiveProxy->_childPid == 0 ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				// Second health 
				//		HLS/DASH:	kill if segments were not generated
				//					frame increasing check
				//					it is also implemented the retention of segments too old (10 minutes)
				//						This is already implemented by the HLS parameters (into the ffmpeg command)
				//						We do it for the DASH option and in case ffmpeg does not work
				//		rtmp(Proxy)/SRT(Grid):		frame increasing check
				if (liveProxyWorking)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor segments check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					for(int outputIndex = 0; outputIndex < copiedLiveProxy->_outputsRoot.size();
						outputIndex++)
					{
						json outputRoot = copiedLiveProxy->_outputsRoot[outputIndex];

						string outputType = JSONUtils::asString(outputRoot, "outputType", "");

						if (!liveProxyWorking)
							break;

						// if (outputType == "HLS" || outputType == "DASH")
						if (outputType == "HLS_Channel")
						{
							string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
							string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");

							try
							{
								/*
								int64_t liveProxyLiveTimeInMinutes =
									chrono::duration_cast<chrono::minutes>(now - copiedLiveProxy->_proxyStart).count();

								// check id done after 3 minutes LiveProxy started, in order to be sure
								// segments were already created
								// 1. get the timestamp of the last generated file
								// 2. fill the vector with the chunks (pathname) to be removed because too old
								//		(10 minutes after the "capacity" of the playlist)
								// 3. kill ffmpeg in case no segments were generated
								if (liveProxyLiveTimeInMinutes > 3)
								*/
								{
									string manifestFilePathName =
										manifestDirectoryPath + "/" + manifestFileName;
									{
										vector<string>	chunksTooOldToBeRemoved;
										bool chunksWereNotGenerated = false;

										string manifestDirectoryPathName;
										{
											size_t manifestFilePathIndex = manifestFilePathName.find_last_of("/");
											if (manifestFilePathIndex == string::npos)
											{
												string errorMessage = __FILEREF__ + "liveProxyMonitor. No manifestDirectoryPath find in the m3u8/mpd file path name"
													+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
													+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
													+ ", manifestFilePathName: " + manifestFilePathName;
												_logger->error(errorMessage);

												throw runtime_error(errorMessage);
											}
											manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
										}

										chrono::system_clock::time_point lastChunkTimestamp = copiedLiveProxy->_proxyStart;
										bool firstChunkRead = false;

										try
										{
											if (fs::exists(manifestDirectoryPathName))
											{
												// chunks will be removed 10 minutes after the "capacity" of the playlist
												// long liveProxyChunkRetentionInSeconds =
												// 	(segmentDurationInSeconds * playlistEntriesNumber)
												// 	+ 10 * 60;	// 10 minutes
												long liveProxyChunkRetentionInSeconds = 10 * 60;	// 10 minutes

												for (fs::directory_entry const& entry: fs::directory_iterator(manifestDirectoryPathName))
												{
													try
													{
														if (!entry.is_regular_file())
															continue;

														string dashPrefixInitFiles ("init-stream");
														if (outputType == "DASH" &&
															entry.path().filename().string().size() >= dashPrefixInitFiles.size()
																&& 0 == entry.path().filename().string().compare(0, dashPrefixInitFiles.size(), dashPrefixInitFiles)
														)
															continue;

														{
															string segmentPathNameToBeRemoved = entry.path().string();

															chrono::system_clock::time_point fileLastModification =
																chrono::time_point_cast<chrono::system_clock::duration>(
																	fs::last_write_time(entry) - fs::file_time_type::clock::now() + chrono::system_clock::now());
															chrono::system_clock::time_point now = chrono::system_clock::now();

															int64_t lastFileUpdateInSeconds = chrono::duration_cast<chrono::seconds>(now - fileLastModification).count();
															if (lastFileUpdateInSeconds > liveProxyChunkRetentionInSeconds)
															{
																_logger->info(__FILEREF__ + "liveProxyMonitor. chunk to be removed, too old"
																	+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
																	+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
																	+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
																	+ ", lastFileUpdateInSeconds: " + to_string(lastFileUpdateInSeconds) + " seconds ago"
																	+ ", liveProxyChunkRetentionInSeconds: " + to_string(liveProxyChunkRetentionInSeconds)
																);

																chunksTooOldToBeRemoved.push_back(segmentPathNameToBeRemoved);
															}

															if (!firstChunkRead
																|| fileLastModification > lastChunkTimestamp)
																lastChunkTimestamp = fileLastModification;

															firstChunkRead = true;
														}
													}
													catch(runtime_error& e)
													{
														string errorMessage = __FILEREF__ + "liveProxyMonitor. listing directory failed"
															+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
															+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
															+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
															+ ", e.what(): " + e.what()
														;
														_logger->error(errorMessage);

														// throw e;
													}
													catch(exception& e)
													{
														string errorMessage = __FILEREF__ + "liveProxyMonitor. listing directory failed"
															+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
															+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
															+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
															+ ", e.what(): " + e.what()
														;
														_logger->error(errorMessage);

														// throw e;
													}
												}
											}
										}
										catch(runtime_error& e)
										{
											_logger->error(__FILEREF__ + "liveProxyMonitor. scan LiveProxy files failed"
												+ ", _ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", _encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
												+ ", e.what(): " + e.what()
											);
										}
										catch(...)
										{
											_logger->error(__FILEREF__ + "liveProxyMonitor. scan LiveProxy files failed"
												+ ", _ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", _encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
											);
										}
				
										if (!firstChunkRead
											|| lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
										{
											// if we are here, it means the ffmpeg command is not generating the ts files

											_logger->error(__FILEREF__ + "liveProxyMonitor. Chunks were not generated"
												+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
												+ ", firstChunkRead: " + to_string(firstChunkRead)
											);

											chunksWereNotGenerated = true;

											liveProxyWorking = false;
											localErrorMessage = " restarted because of 'no segments were generated'";

											_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor. Live Proxy is not working (no segments were generated). LiveProxy (ffmpeg) is killed in order to be started again"
												+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName
												// + ", channelLabel: " + copiedLiveProxy->_channelLabel
												+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
											);


											// we killed the process, we do not care to remove the too old segments
											// since we will remove the entore directory
											break;
										}

										{
											for (string segmentPathNameToBeRemoved: chunksTooOldToBeRemoved)
											{
												try
												{
													_logger->info(__FILEREF__ + "liveProxyMonitor. Remove chunk because too old"
														+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
														+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
														+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
													fs::remove_all(segmentPathNameToBeRemoved);
												}
												catch(runtime_error& e)
												{
													_logger->error(__FILEREF__ + "liveProxyMonitor. remove failed"
														+ ", _ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
														+ ", _encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
														+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
														+ ", e.what(): " + e.what()
													);
												}
											}
										}
									}
								}
							}
							catch(runtime_error& e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on segments (and retention) failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception& e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on segments (and retention) failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
					}
				}

				if (sourceLiveProxy->_childPid == 0 ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveProxyWorking) // && rtmpOutputFound)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor isFrameIncreasing check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					try
					{
						// Second health check, rtmp(Proxy)/SRT(Grid), looks if the frame is increasing
						int maxMilliSecondsToWait = 3000;
						if (!sourceLiveProxy->_ffmpeg->isSizeOrFrameIncreasing(maxMilliSecondsToWait))
						{
							_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor (rtmp). Live Proxy size/frame is not increasing'. LiveProxy (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
								+ ", configurationLabel: " + configurationLabel
								+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							);

							liveProxyWorking = false;

							localErrorMessage = " restarted because of 'size/frame is not increasing'";
						}
					}
					catch(FFMpegEncodingStatusNotAvailable& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) size/frame increasing check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) size/frame increasing check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) size/frame increasing check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (sourceLiveProxy->_childPid == 0 ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveProxyWorking) // && rtmpOutputFound)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor forbiddenErrorInOutputLog check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					try
					{
						if (sourceLiveProxy->_ffmpeg->forbiddenErrorInOutputLog())
						{
							_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor (rtmp). Live Proxy is returning 'HTTP error 403 Forbidden'. LiveProxy (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
								// + ", channelLabel: " + copiedLiveProxy->_channelLabel
								+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							);

							liveProxyWorking = false;
							localErrorMessage = " restarted because of 'HTTP error 403 Forbidden'";
						}
					}
					catch(FFMpegEncodingStatusNotAvailable& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) HTTP error 403 Forbidden check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) HTTP error 403 Forbidden check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception& e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) HTTP error 403 Forbidden check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (sourceLiveProxy->_childPid == 0 ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_childPid: " + to_string(sourceLiveProxy->_childPid)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (!liveProxyWorking)
				{
					_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor. LiveProxy (ffmpeg) is killed/quit in order to be started again"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", localErrorMessage: " + localErrorMessage
						// + ", channelLabel: " + copiedLiveProxy->_channelLabel
						+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
					);

					try
					{
						// 2021-12-14: switched from quit to kill because it seems
						//		ffmpeg didn't terminate (in case of quit) when he was
						//		failing. May be because it could not finish his sample/frame
						//		to process. The result is that the channels were not restarted.
						//		This is an ipothesys, not 100% sure
						// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liveProxy
						// 2023-02-18: using SIGQUIT, the process was not stopped, it worked with SIGTERM
						//	SIGTERM now is managed by FFMpeg.cpp too
						// ProcessUtility::killProcess(sourceLiveProxy->_childPid);
						// sourceLiveProxy->_killedBecauseOfNotWorking = true;
						// ProcessUtility::quitProcess(sourceLiveProxy->_childPid);
						ProcessUtility::termProcess(sourceLiveProxy->_childPid);
						{
							char strDateTime [64];
							{
								time_t utcTime = chrono::system_clock::to_time_t(
									chrono::system_clock::now());
								tm tmDateTime;
								localtime_r (&utcTime, &tmDateTime);
								sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
									tmDateTime. tm_year + 1900,
									tmDateTime. tm_mon + 1,
									tmDateTime. tm_mday,
									tmDateTime. tm_hour,
									tmDateTime. tm_min,
									tmDateTime. tm_sec);
							}
							sourceLiveProxy->_errorMessage = string(strDateTime) + " "
								// + liveProxy->_channelLabel
								+ localErrorMessage;
						}
					}
					catch(runtime_error& e)
					{
						string errorMessage = string("liveProxyMonitor. ProcessUtility::kill/quit/term Process failed")
							+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
							+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							+ ", e.what(): " + e.what()
								;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				_logger->info(__FILEREF__ + "liveProxyMonitor "
					+ to_string(liveProxyIndex) + "/" + to_string(liveProxyAndGridRunningCounter)
					+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
					+ ", configurationLabel: " + configurationLabel
					+ ", @MMS statistics@ - elapsed time: @" + to_string(
						chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - now).count()) + "@"
				);
			}
			_logger->info(__FILEREF__ + "liveProxyMonitor"
				+ ", liveProxyAndGridRunningCounter: " + to_string(liveProxyAndGridRunningCounter)
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - monitorStart).count())
			);
		}
		catch(runtime_error& e)
		{
			string errorMessage = string ("liveProxyMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception& e)
		{
			string errorMessage = string ("liveProxyMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		// recording
		try
		{
			// this is to have a copy of LiveRecording
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> copiedRunningLiveRecordingCapability;

			// this is to have access to running and _proxyStart
			//	to check if it is changed. In case the process is killed, it will access
			//	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> sourceLiveRecordingCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveRecordingRunningCounter = 0;
			{
				lock_guard<mutex> locker(*_liveRecordingMutex);

				int liveRecordingNotRunningCounter = 0;

				for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording: *_liveRecordingsCapability)
				{
					if (liveRecording->_childPid != 0 && liveRecording->_monitoringEnabled)
					{
						liveRecordingRunningCounter++;

						copiedRunningLiveRecordingCapability.push_back(
							liveRecording->cloneForMonitorAndVirtualVOD());
						sourceLiveRecordingCapability.push_back(
                            liveRecording);
					}
					else
					{
						liveRecordingNotRunningCounter++;
					}
				}
				_logger->info(__FILEREF__ + "liveRecordingMonitor, numbers"
					+ ", total LiveRecording: " + to_string(liveRecordingRunningCounter
						+ liveRecordingNotRunningCounter)
					+ ", liveRecordingRunningCounter: " + to_string(liveRecordingRunningCounter)
					+ ", liveRecordingNotRunningCounter: " + to_string(liveRecordingNotRunningCounter)
				);
			}
			_logger->info(__FILEREF__ + "liveRecordingMonitor clone"
				+ ", copiedRunningLiveRecordingCapability.size: " + to_string(copiedRunningLiveRecordingCapability.size())
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - startClone).count())
			);

			chrono::system_clock::time_point monitorStart = chrono::system_clock::now();

			for (int liveRecordingIndex = 0;
				liveRecordingIndex < copiedRunningLiveRecordingCapability.size();
				liveRecordingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveRecording> copiedLiveRecording
					= copiedRunningLiveRecordingCapability[liveRecordingIndex];
				shared_ptr<FFMPEGEncoderBase::LiveRecording> sourceLiveRecording
					= sourceLiveRecordingCapability[liveRecordingIndex];

				_logger->info(__FILEREF__ + "liveRecordingMonitor"
					+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
					+ ", channelLabel: " + copiedLiveRecording->_channelLabel
				);

				chrono::system_clock::time_point now = chrono::system_clock::now();

				bool liveRecorderWorking = true;
				string localErrorMessage;

				if (sourceLiveRecording->_childPid == 0 ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_childPid: " + to_string(sourceLiveRecording->_childPid)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// copiedLiveRecording->_recordingStart could be a bit in the future
				int64_t liveRecordingLiveTimeInMinutes;
				if (now > copiedLiveRecording->_recordingStart)
					liveRecordingLiveTimeInMinutes = chrono::duration_cast<chrono::minutes>(
						now - copiedLiveRecording->_recordingStart).count();
				else
					liveRecordingLiveTimeInMinutes = 0;

				string field = "segmentDuration";
				int segmentDurationInSeconds = JSONUtils::asInt(copiedLiveRecording->_ingestedParametersRoot, field, -1);

				// check is done after 5 minutes + segmentDurationInSeconds LiveRecording started,
				// in order to be sure the file was already created
				if (liveRecordingLiveTimeInMinutes <= (segmentDurationInSeconds / 60) + 5)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. Checks are not done because too early"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", liveRecordingLiveTimeInMinutes: "
							+ to_string(liveRecordingLiveTimeInMinutes)
						+ ", (segmentDurationInSeconds / 60) + 5: "
							+ to_string((segmentDurationInSeconds / 60) + 5)
					);

					continue;
				}

				if (sourceLiveRecording->_childPid == 0 ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_childPid: " + to_string(sourceLiveRecording->_childPid)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// First health check
				//		kill if 1840699_408620.liveRecorder.list file does not exist or was not updated in the last (2 * segment duration in secs) seconds
				if (liveRecorderWorking)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. liveRecorder.list check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					try
					{
						// looking the manifests path name timestamp

						string segmentListPathName = copiedLiveRecording->_chunksTranscoderStagingContentsPath
							+ copiedLiveRecording->_segmentListFileName;

						{
							// 2022-05-26: in case the file does not exist, try again to make sure
							//	it really does not exist
							bool segmentListFileExistence = fs::exists(segmentListPathName);

							if (!segmentListFileExistence)
							{
								int sleepTimeInSeconds = 5;

								_logger->warn(__FILEREF__
									+ "liveRecordingMonitor. Segment list file does not exist, let's check again"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
									+ ", segmentListPathName: " + segmentListPathName
									+ ", sleepTimeInSeconds: " + to_string(sleepTimeInSeconds)
								);

								this_thread::sleep_for(chrono::seconds(sleepTimeInSeconds));

								segmentListFileExistence = fs::exists(segmentListPathName);
							}

							if(!segmentListFileExistence)
							{
								liveRecorderWorking = false;

								_logger->error(__FILEREF__ + "liveRecordingMonitor. Segment list file does not exist"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
									+ ", segmentListPathName: " + segmentListPathName
								);

								localErrorMessage = " restarted because of 'segment list file is missing or was not updated'";
							}
							else
							{
								int64_t lastSegmentListFileUpdateInSeconds;
								{
									chrono::system_clock::time_point fileLastModification =
										chrono::time_point_cast<chrono::system_clock::duration>(
											fs::last_write_time(segmentListPathName) - fs::file_time_type::clock::now() + chrono::system_clock::now());
									chrono::system_clock::time_point now = chrono::system_clock::now();

									lastSegmentListFileUpdateInSeconds =
										chrono::duration_cast<chrono::seconds>(now - fileLastModification).count();
								}

								long maxLastSegmentListFileUpdateInSeconds
									= segmentDurationInSeconds * 2;

								if (lastSegmentListFileUpdateInSeconds > maxLastSegmentListFileUpdateInSeconds)
								{
									liveRecorderWorking = false;

									_logger->error(__FILEREF__ + "liveRecordingMonitor. Segment list file was not updated "
										+ "in the last " + to_string(maxLastSegmentListFileUpdateInSeconds) + " seconds"
										+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
										+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
										+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
										+ ", segmentListPathName: " + segmentListPathName
										+ ", lastSegmentListFileUpdateInSeconds: " + to_string(lastSegmentListFileUpdateInSeconds) + " seconds ago"
										+ ", maxLastSegmentListFileUpdateInSeconds: " + to_string(maxLastSegmentListFileUpdateInSeconds)
									);

									localErrorMessage = " restarted because of 'segment list file is missing or was not updated'";
								}
							}
						}
					}
					catch(runtime_error& e)
					{
						string errorMessage = string ("liveRecordingMonitor on path name failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception& e)
					{
						string errorMessage = string ("liveRecordingMonitor on path name failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (sourceLiveRecording->_childPid == 0 ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_childPid: " + to_string(sourceLiveRecording->_childPid)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// Second health check
				//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
				//		rtmp(Proxy):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
				//			This check has to be done just once (not for each outputRoot) in case we have at least one rtmp output
				bool rtmpOutputFound = false;
				if (liveRecorderWorking)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. manifest check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					json outputsRoot = copiedLiveRecording->_encodingParametersRoot["outputsRoot"];
					for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
					{
						json outputRoot = outputsRoot[outputIndex];

						string outputType = JSONUtils::asString(outputRoot, "outputType", "");
						string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
						string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");

						if (!liveRecorderWorking)
							break;

						// if (outputType == "HLS" || outputType == "DASH")
						if (outputType == "HLS_Channel")
						{
							try
							{
								// First health check (HLS/DASH) looking the manifests path name timestamp

								string manifestFilePathName =
									manifestDirectoryPath + "/" + manifestFileName;
								{
									if(!fs::exists(manifestFilePathName))
									{
										liveRecorderWorking = false;

										_logger->error(__FILEREF__ + "liveRecorderMonitor. Manifest file does not exist"
											+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestFilePathName: " + manifestFilePathName
										);

										localErrorMessage = " restarted because of 'manifest file is missing'";

										break;
									}
									else
									{
										int64_t lastManifestFileUpdateInSeconds;
										{
											chrono::system_clock::time_point fileLastModification =
												chrono::time_point_cast<chrono::system_clock::duration>(
													fs::last_write_time(manifestFilePathName) - fs::file_time_type::clock::now() + chrono::system_clock::now());
											chrono::system_clock::time_point now = chrono::system_clock::now();

											lastManifestFileUpdateInSeconds =
												chrono::duration_cast<chrono::seconds>(now - fileLastModification).count();
										}

										long maxLastManifestFileUpdateInSeconds = 45;

										if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
										{
											liveRecorderWorking = false;

											_logger->error(__FILEREF__ + "liveRecorderMonitor. Manifest file was not updated "
												+ "in the last " + to_string(maxLastManifestFileUpdateInSeconds) + " seconds"
												+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName
												+ ", lastManifestFileUpdateInSeconds: " + to_string(lastManifestFileUpdateInSeconds) + " seconds ago"
												+ ", maxLastManifestFileUpdateInSeconds: " + to_string(maxLastManifestFileUpdateInSeconds)
											);

											localErrorMessage = " restarted because of 'manifest file was not updated'";

											break;
										}
									}
								}
							}
							catch(runtime_error& e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on manifest path name failed")
									+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception& e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on manifest path name failed")
									+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
						else	// rtmp (Proxy) 
						{
							rtmpOutputFound = true;
						}
					}
				}

				if (sourceLiveRecording->_childPid == 0||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_childPid: " + to_string(sourceLiveRecording->_childPid)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveRecorderWorking && rtmpOutputFound)
				{
					try
					{
						_logger->info(__FILEREF__ + "liveRecordingMonitor. nonMonotonousDTSInOutputLog check"
							+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						);

						// First health check (rtmp), looks the log and check there is no message like
						//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
						//	This message causes proxy not working
						if (sourceLiveRecording->_ffmpeg->nonMonotonousDTSInOutputLog())
						{
							liveRecorderWorking = false;

							_logger->error(__FILEREF__ + "liveRecorderMonitor (rtmp). Live Recorder is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. LiveRecorder (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
								+ ", channelLabel: " + copiedLiveRecording->_channelLabel
								+ ", copiedLiveRecording->_childPid: " + to_string(copiedLiveRecording->_childPid)
							);

							localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
						}
					}
					catch(runtime_error& e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) Non-monotonous DTS failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception& e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) Non-monotonous DTS failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (sourceLiveRecording->_childPid == 0 ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_childPid: " + to_string(sourceLiveRecording->_childPid)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// Thirth health 
				//		HLS/DASH:	kill if segments were not generated
				//					frame increasing check
				//					it is also implemented the retention of segments too old (10 minutes)
				//						This is already implemented by the HLS parameters (into the ffmpeg command)
				//						We do it for the DASH option and in case ffmpeg does not work
				//		rtmp(Proxy):		frame increasing check
				//			This check has to be done just once (not for each outputRoot) in case we have at least one rtmp output
				if (liveRecorderWorking)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. segment check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					json outputsRoot = copiedLiveRecording->_encodingParametersRoot["outputsRoot"];
					for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
					{
						json outputRoot = outputsRoot[outputIndex];

						string outputType = JSONUtils::asString(outputRoot, "outputType", "");
						string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
						string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");
						int outputPlaylistEntriesNumber = JSONUtils::asInt(outputRoot,
							"playlistEntriesNumber", 10);
						int outputSegmentDurationInSeconds = JSONUtils::asInt(outputRoot,
							"segmentDurationInSeconds", 10);

						if (!liveRecorderWorking)
							break;

						// if (outputType == "HLS" || outputType == "DASH")
						if (outputType == "HLS_Channel")
						{
							try
							{
								string manifestFilePathName =
									manifestDirectoryPath + "/" + manifestFileName;
								{
									vector<string>	chunksTooOldToBeRemoved;
									bool chunksWereNotGenerated = false;

									string manifestDirectoryPathName;
									{
										size_t manifestFilePathIndex = manifestFilePathName.find_last_of("/");
										if (manifestFilePathIndex == string::npos)
										{
											string errorMessage = __FILEREF__ + "liveRecordingMonitor. No manifestDirectoryPath find in the m3u8/mpd file path name"
												+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
												+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName;
											_logger->error(errorMessage);

											throw runtime_error(errorMessage);
										}
										manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
									}

									chrono::system_clock::time_point lastChunkTimestamp = copiedLiveRecording->_recordingStart;
									bool firstChunkRead = false;

									try
									{
										if (fs::exists(manifestDirectoryPathName))
										{
											// chunks will be removed 10 minutes after the "capacity" of the playlist
											// 2022-05-26: it was 10 minutes fixed. This is an error
											// in case of LiveRecorderVirtualVOD because, in this scenario,
											// the segments have to be present according
											// LiveRecorderVirtualVODMaxDuration (otherwise we will have an error
											// during the building of the VirtualVOD (segments not found).
											// For this reason the retention has to consider segment duration
											// and playlistEntriesNumber
											// long liveProxyChunkRetentionInSeconds = 10 * 60;	// 10 minutes
											long liveProxyChunkRetentionInSeconds =
												(outputSegmentDurationInSeconds * outputPlaylistEntriesNumber)
												+ (10 * 60);	// 10 minutes
											_logger->info(__FILEREF__
												+ "liveRecordingMonitor. segment check"
												+ ", ingestionJobKey: " + to_string(
													copiedLiveRecording->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(
													copiedLiveRecording->_encodingJobKey)
												+ ", channelLabel: "
													+ copiedLiveRecording->_channelLabel
												+ ", outputSegmentDurationInSeconds: "
													+ to_string(outputSegmentDurationInSeconds)
												+ ", outputPlaylistEntriesNumber: "
													+ to_string(outputPlaylistEntriesNumber)
												+ ", liveProxyChunkRetentionInSeconds: "
													+ to_string(liveProxyChunkRetentionInSeconds)
											);

											for (fs::directory_entry const& entry: fs::directory_iterator(manifestDirectoryPathName))
											{
												try
												{
													if (!entry.is_regular_file())
														continue;

													string dashPrefixInitFiles ("init-stream");
													if (outputType == "DASH" &&
														entry.path().filename().string().size() >= dashPrefixInitFiles.size()
															&& 0 == entry.path().filename().string().compare(0, dashPrefixInitFiles.size(), dashPrefixInitFiles)
													)
														continue;

													{
														string segmentPathNameToBeRemoved = entry.path();

														chrono::system_clock::time_point fileLastModification =
															chrono::time_point_cast<chrono::system_clock::duration>(
																fs::last_write_time(entry) - fs::file_time_type::clock::now() + chrono::system_clock::now());
														chrono::system_clock::time_point now = chrono::system_clock::now();

														int64_t lastFileUpdateInSeconds = chrono::duration_cast<chrono::seconds>(now - fileLastModification).count();
														if (lastFileUpdateInSeconds > liveProxyChunkRetentionInSeconds)
														{
															_logger->info(__FILEREF__ + "liveRecordingMonitor. chunk to be removed, too old"
																+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
																+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
																+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
																+ ", lastFileUpdateInSeconds: " + to_string(lastFileUpdateInSeconds) + " seconds ago"
																+ ", liveProxyChunkRetentionInSeconds: " + to_string(liveProxyChunkRetentionInSeconds)
															);

															chunksTooOldToBeRemoved.push_back(segmentPathNameToBeRemoved);
														}

														if (!firstChunkRead
															|| fileLastModification > lastChunkTimestamp)
															lastChunkTimestamp = fileLastModification;

														firstChunkRead = true;
													}
												}
												catch(runtime_error& e)
												{
													string errorMessage = __FILEREF__ + "liveRecordingMonitor. listing directory failed"
														+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
														+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
														+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
														+ ", e.what(): " + e.what()
													;
													_logger->error(errorMessage);

													// throw e;
												}
												catch(exception& e)
												{
													string errorMessage = __FILEREF__ + "liveRecordingMonitor. listing directory failed"
														+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
														+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
														+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
														+ ", e.what(): " + e.what()
													;
													_logger->error(errorMessage);

													// throw e;
												}
											}
										}
									}
									catch(runtime_error& e)
									{
										_logger->error(__FILEREF__ + "liveRecordingMonitor. scan LiveRecorder files failed"
											+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
											+ ", e.what(): " + e.what()
										);
									}
									catch(...)
									{
										_logger->error(__FILEREF__ + "liveRecordingMonitor. scan LiveRecorder files failed"
											+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
										);
									}
			
									if (!firstChunkRead
										|| lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
									{
										// if we are here, it means the ffmpeg command is not generating the ts files

										_logger->error(__FILEREF__ + "liveRecorderMonitor. Chunks were not generated"
											+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
											+ ", firstChunkRead: " + to_string(firstChunkRead)
										);

										chunksWereNotGenerated = true;

										liveRecorderWorking = false;
										localErrorMessage = " restarted because of 'no segments were generated'";

										_logger->error(__FILEREF__ + "liveRecordingMonitor. ProcessUtility::kill/quit/term Process. liveRecorderMonitor. Live Recorder is not working (no segments were generated). LiveRecorder (ffmpeg) is killed in order to be started again"
											+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestFilePathName: " + manifestFilePathName
											+ ", channelLabel: " + copiedLiveRecording->_channelLabel
											+ ", liveRecorder->_childPid: " + to_string(copiedLiveRecording->_childPid)
										);


										// we killed the process, we do not care to remove the too old segments
										// since we will remove the entore directory
										break;
									}

									{
										for (string segmentPathNameToBeRemoved: chunksTooOldToBeRemoved)
										{
											try
											{
												_logger->info(__FILEREF__ + "liveRecorderMonitor. Remove chunk because too old"
													+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
													+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
													+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
												fs::remove_all(segmentPathNameToBeRemoved);
											}
											catch(runtime_error& e)
											{
												_logger->error(__FILEREF__ + "liveRecordingMonitor. remove failed"
													+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
													+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
													+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
													+ ", e.what(): " + e.what()
												);
											}
										}
									}
								}
							}
							catch(runtime_error& e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on segments (and retention) failed")
									+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception& e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on segments (and retention) failed")
									+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
					}
				}

				if (sourceLiveRecording->_childPid == 0 ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_childPid: " + to_string(sourceLiveRecording->_childPid)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveRecorderWorking && copiedLiveRecording->_monitoringFrameIncreasingEnabled) // && rtmpOutputFound)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. isSizeOrFrameIncreasing check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					try
					{
						// Second health check, rtmp(Proxy), looks if the frame is increasing
						int maxMilliSecondsToWait = 3000;
						if (!sourceLiveRecording->_ffmpeg->isSizeOrFrameIncreasing(
							maxMilliSecondsToWait))
						{
							_logger->error(__FILEREF__ + "liveRecordingMonitor. ProcessUtility::kill/quit/term Process. liveRecorderMonitor (rtmp). Live Recorder size/frame is not increasing'. LiveRecorder (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
								+ ", channelLabel: " + copiedLiveRecording->_channelLabel
								+ ", _childPid: " + to_string(copiedLiveRecording->_childPid)
							);

							liveRecorderWorking = false;

							localErrorMessage = " restarted because of 'size/frame is not increasing'";
						}
					}
					catch(FFMpegEncodingStatusNotAvailable& e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) size/frame increasing check failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error& e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) size/frame increasing check failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception& e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) size/frame increasing check failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (!liveRecorderWorking)
				{
					_logger->error(__FILEREF__ + "liveRecordingMonitor. ProcessUtility::kill/quit/term Process. liveRecordingMonitor. Live Recording is not working (segment list file is missing or was not updated). LiveRecording (ffmpeg) is killed in order to be started again"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", copiedLiveRecording->_childPid: " + to_string(copiedLiveRecording->_childPid)
					);

					try
					{
						// 2021-12-14: switched from quit to kill because it seems
						//		ffmpeg didn't terminate (in case of quit) when he was
						//		failing. May be because it could not finish his sample/frame
						//		to process. The result is that the channels were not restarted.
						//		This is an ipothesys, not 100% sure
						// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liverecording
						// 2023-02-18: using SIGQUIT, the process was not stopped, it worked with SIGTERM
						//	SIGTERM now is managed by FFMpeg.cpp too
						// ProcessUtility::killProcess(sourceLiveRecording->_childPid);
						// sourceLiveRecording->_killedBecauseOfNotWorking = true;
						// ProcessUtility::quitProcess(sourceLiveRecording->_childPid);
						ProcessUtility::termProcess(sourceLiveRecording->_childPid);
						{
							char strDateTime [64];
							{
								time_t utcTime = chrono::system_clock::to_time_t(
									chrono::system_clock::now());
								tm tmDateTime;
								localtime_r (&utcTime, &tmDateTime);
								sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
									tmDateTime. tm_year + 1900,
									tmDateTime. tm_mon + 1,
									tmDateTime. tm_mday,
									tmDateTime. tm_hour,
									tmDateTime. tm_min,
									tmDateTime. tm_sec);
							}
							sourceLiveRecording->_errorMessage = string(strDateTime) + " "
								+ sourceLiveRecording->_channelLabel +
								localErrorMessage;
						}
					}
					catch(runtime_error& e)
					{
						string errorMessage = string("liveRecordingMonitor. ProcessUtility::kill/quit Process failed")
							+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", channelLabel: " + copiedLiveRecording->_channelLabel
							+ ", copiedLiveRecording->_childPid: " + to_string(copiedLiveRecording->_childPid)
							+ ", e.what(): " + e.what()
								;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				_logger->info(__FILEREF__ + "liveRecordingMonitor "
					+ to_string(liveRecordingIndex) + "/" + to_string(liveRecordingRunningCounter)
					+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
					+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					+ ", @MMS statistics@ - elapsed time: @" + to_string(
						chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - now).count()
					) + "@"
				);
			}
			_logger->info(__FILEREF__ + "liveRecordingMonitor"
				+ ", liveRecordingRunningCounter: " + to_string(liveRecordingRunningCounter)
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - monitorStart).count())
			);
		}
		catch(runtime_error& e)
		{
			string errorMessage = string ("liveRecordingMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception& e)
		{
			string errorMessage = string ("liveRecordingMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		this_thread::sleep_for(chrono::seconds(_monitorCheckInSeconds));
	}
}

void FFMPEGEncoderDaemons::stopMonitorThread()
{

	_monitorThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_monitorCheckInSeconds));
}

void FFMPEGEncoderDaemons::startCPUUsageThread()
{

	int64_t counter = 0;

	while(!_cpuUsageThreadShutdown)
	{
		this_thread::sleep_for(chrono::milliseconds(200));

		try
		{
			lock_guard<mutex> locker(*_cpuUsageMutex);

			_cpuUsage->pop_back();
			_cpuUsage->push_front(_getCpuUsage.getCpuUsage());
			// *_cpuUsage = _getCpuUsage.getCpuUsage();

			if (++counter % 100 == 0)
			{
				string lastCPUUsage;
				for(int cpuUsage: *_cpuUsage)
					lastCPUUsage += (to_string(cpuUsage) + " ");

				_logger->info(__FILEREF__ + "cpuUsageThread"
					+ ", lastCPUUsage: " + lastCPUUsage
				);
			}
		}
		catch(runtime_error& e)
		{
			string errorMessage = string ("cpuUsage thread failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception& e)
		{
			string errorMessage = string ("cpuUsage thread failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
	}
}

void FFMPEGEncoderDaemons::stopCPUUsageThread()
{

	_cpuUsageThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(1));
}

