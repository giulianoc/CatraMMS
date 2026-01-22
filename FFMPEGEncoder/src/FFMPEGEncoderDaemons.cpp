
#include "FFMPEGEncoderDaemons.h"

#include "CurlWrapper.h"
#include "Datetime.h"
#include "JSONUtils.h"
#include "JsonPath.h"
#include "MMSEngineDBFacade.h"
#include "ProcessUtility.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

FFMPEGEncoderDaemons::FFMPEGEncoderDaemons(
	json configurationRoot, mutex *liveRecordingMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability,
	mutex *liveProxyMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability
)
	: FFMPEGEncoderBase(configurationRoot)
{
	try
	{
		_liveRecordingMutex = liveRecordingMutex;
		_liveRecordingsCapability = liveRecordingsCapability;
		_liveProxyMutex = liveProxyMutex;
		_liveProxiesCapability = liveProxiesCapability;

		_monitorThreadShutdown = false;

		_monitorCheckInSeconds = JSONUtils::asInt32(configurationRoot["ffmpeg"], "monitorCheckInSeconds", 5);
		LOG_INFO(
			"Configuration item"
			", ffmpeg->monitorCheckInSeconds: {}",
			_monitorCheckInSeconds
		);

		_maxRealTimeInfoNotChangedToleranceInSeconds = 60;
		_maxRealTimeInfoTimestampDiscontinuitiesInTimeWindow = 1000; // ne ho contati 1300 in 30 secondi in un caso
	}
	catch (exception &e)
	{
		LOG_ERROR("FFMPEGEncoderDaemons builder failed"
			", exception: {}", e.what());
	}
}

FFMPEGEncoderDaemons::~FFMPEGEncoderDaemons() = default;

void FFMPEGEncoderDaemons::startMonitorThread()
{
	ThreadLogger threadLogger(spdlog::get("monitor"));

	while (!_monitorThreadShutdown)
	{
		// proxy
		try
		{
			// this is to have a copy of LiveProxyAndGrid
			vector<shared_ptr<LiveProxyAndGrid>> copiedRunningLiveProxiesCapability;

			// this is to have access to running and _proxyStart
			//	to check if it is changed. In case the process is killed, it will access
			//	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<LiveProxyAndGrid>> sourceLiveProxiesCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveProxyAndGridRunningCounter = 0;
			{
				lock_guard locker(*_liveProxyMutex);

				int liveProxyAndGridNotRunningCounter = 0;

				for (const shared_ptr<LiveProxyAndGrid>& liveProxy : *_liveProxiesCapability)
				{
					if (liveProxy->_childProcessId.isInitialized()) // running
					{
						liveProxyAndGridRunningCounter++;

						copiedRunningLiveProxiesCapability.push_back(liveProxy->cloneForMonitor());
						sourceLiveProxiesCapability.push_back(liveProxy);
					}
					else
					{
						liveProxyAndGridNotRunningCounter++;
					}
				}
				LOG_INFO(
					"liveProxyMonitor, numbers"
					", total LiveProxyAndGrid: {}"
					", liveProxyAndGridRunningCounter: {}"
					", liveProxyAndGridNotRunningCounter: {}",
					liveProxyAndGridRunningCounter + liveProxyAndGridNotRunningCounter, liveProxyAndGridRunningCounter,
					liveProxyAndGridNotRunningCounter
				);
			}
			LOG_INFO(
				"liveProxyMonitor clone"
				", copiedRunningLiveProxiesCapability.size: {}"
				", @MMS statistics@ - elapsed (millisecs): {}",
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startClone).count(),
				copiedRunningLiveProxiesCapability.size()
			);

			chrono::system_clock::time_point monitorStart = chrono::system_clock::now();

			for (int liveProxyIndex = 0; liveProxyIndex < copiedRunningLiveProxiesCapability.size(); liveProxyIndex++)
			{
				const shared_ptr<LiveProxyAndGrid>& copiedLiveProxy = copiedRunningLiveProxiesCapability[liveProxyIndex];
				const shared_ptr<LiveProxyAndGrid>& sourceLiveProxy = sourceLiveProxiesCapability[liveProxyIndex];

				// this is just for logging
				string configurationLabel;
				if (!copiedLiveProxy->_inputsRoot.empty())
				{
					json inputRoot = copiedLiveProxy->_inputsRoot[0];
					string field = "streamInput";
					if (JSONUtils::isPresent(inputRoot, field))
					{
						json streamInputRoot = inputRoot[field];
						field = "configurationLabel";
						configurationLabel = JSONUtils::asString(streamInputRoot, field, "");
					}
				}

				LOG_INFO(
					"liveProxyMonitor start"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", configurationLabel: {}"
					", encodingStart: {}"
					", sourceLiveProxy->_childProcessId: {}",
					copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
					copiedLiveProxy->_encodingStart ? Datetime::timePointAsLocalString(*(copiedLiveProxy->_encodingStart)) : "null",
					sourceLiveProxy->_childProcessId.toString()
				);

				chrono::system_clock::time_point now = chrono::system_clock::now();

				bool liveProxyWorking = true;
				string localErrorMessage;

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				int64_t liveProxyLiveTimeInSeconds = 0;
				{
					// copiedLiveProxy->_proxyStart could be a bit in the future
					if (copiedLiveProxy->_encodingStart)
					{
						if (now > *(copiedLiveProxy->_encodingStart))
							liveProxyLiveTimeInSeconds = chrono::duration_cast<chrono::seconds>(now - *(copiedLiveProxy->_encodingStart)).count();
						else // it will be negative
							liveProxyLiveTimeInSeconds = chrono::duration_cast<chrono::seconds>(now - *(copiedLiveProxy->_encodingStart)).count();
					}

					// checks are done after 3 minutes LiveProxy started,
					// in order to be sure the manifest file was already created
					// Commentato alcuni controlli possono essere fatti anche da subito. Aggiunto questo controllo per il caso specifico del manifest
					/*
					if (liveProxyLiveTimeInMinutes <= 3)
					{
						info(
							__FILEREF__ + "liveProxyMonitor. Checks are not done because too early" + ", ingestionJobKey: " +
							to_string(copiedLiveProxy->_ingestionJobKey) + ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey) +
							", liveProxyLiveTimeInMinutes: " + to_string(liveProxyLiveTimeInMinutes)
						);

						continue;
					}
					*/
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				// controlla se il lastModificationTime dell'output file di ffmpeg è cambiato
				/* 2025-11-25: ho commentato questo controllo perchè ho visto che c'è almeno uno scenario in cui
				 * il file di output non cambia, cioé nel caso di un proxy di un VOD.
				if (liveProxyWorking)
				{
					LOG_INFO(
						"liveProxyMonitor outputFFMpegFileSize check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
					);
					uintmax_t previousOutputFfmpegFileSize = copiedLiveProxy->_outputFfmpegFileSize;
					uintmax_t newOutputFfmpegFileSize = sourceLiveProxy->_ffmpeg->getOutputFFMpegFileSize();
					sourceLiveProxy->_outputFfmpegFileSize = newOutputFfmpegFileSize;
					if (previousOutputFfmpegFileSize != 0 && previousOutputFfmpegFileSize == newOutputFfmpegFileSize)
					{
						liveProxyWorking = false;

						LOG_ERROR(
							"liveProxyMonitor. output ffmpeg file size is not changing"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", previousOutputFfmpegFileSize: {}"
							", newOutputFfmpegFileSize: {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, previousOutputFfmpegFileSize, newOutputFfmpegFileSize
						);

						localErrorMessage = " restarted because of 'output ffmpeg file size is not changing'";
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}"
						", copiedLiveProxy->_proxyStart.time_since_epoch().count(): {}"
						", sourceLiveProxy->_proxyStart.time_since_epoch().count(): {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString(), copiedLiveProxy->_proxyStart.time_since_epoch().count(),
						sourceLiveProxy->_proxyStart.time_since_epoch().count()
					);

					continue;
				}
				*/

				// First health check
				//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
				//		rtmp(Proxy)/SRT(Grid):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
				// Inoltre questo controllo viene fatto se sono passati almeno 3 minuti da quando live proxy è partito,
				// in order to be sure the manifest file was already created
				bool rtmpOutputFound = false;
				if (liveProxyWorking && liveProxyLiveTimeInSeconds > 3 * 60)
				{
					LOG_INFO(
						"liveProxyMonitor manifest check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
					);

					for (int outputIndex = 0; outputIndex < copiedLiveProxy->_outputsRoot.size(); outputIndex++)
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
									{
										string manifestFilePathName = std::format("{}/{}", manifestDirectoryPath, manifestFileName);
										if (!exists(manifestFilePathName))
										{
											liveProxyWorking = false;

											LOG_ERROR(
												"liveProxyMonitor. Manifest file does not exist"
												", ingestionJobKey: {}"
												", encodingJobKey: {}"
												", manifestFilePathName: {}",
												copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, manifestFilePathName
											);

											localErrorMessage = " restarted because of 'manifest file is missing'";

											break;
										}

										int64_t lastManifestFileUpdateInSeconds;
										{
											chrono::system_clock::time_point fileLastModification =
												chrono::time_point_cast<chrono::system_clock::duration>(
													fs::last_write_time(manifestFilePathName) - fs::file_time_type::clock::now() +
													chrono::system_clock::now()
												);

											lastManifestFileUpdateInSeconds =
												chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - fileLastModification).count();
										}

										long maxLastManifestFileUpdateInSeconds = 30;

										if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
										{
											liveProxyWorking = false;

											LOG_ERROR(
												"liveProxyMonitor. Manifest file was not updated in the last {} seconds"
												", ingestionJobKey: {}"
												", encodingJobKey: {}"
												", manifestFilePathName: {}"
												", lastManifestFileUpdateInSeconds: {} seconds ago"
												", maxLastManifestFileUpdateInSeconds: {}",
												maxLastManifestFileUpdateInSeconds, copiedLiveProxy->_ingestionJobKey,
												copiedLiveProxy->_encodingJobKey, manifestFilePathName, lastManifestFileUpdateInSeconds,
												maxLastManifestFileUpdateInSeconds
											);

											localErrorMessage = " restarted because of 'manifest file was not updated'";

											break;
										}
									}
								}
							}
							catch (exception &e)
							{
								LOG_ERROR(
									"liveProxyMonitor (HLS) on manifest path name failed"
									", copiedLiveProxy->_ingestionJobKey: {}"
									", copiedLiveProxy->_encodingJobKey: {}"
									", e.what(): {}",
									copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
								);
							}
						}
						else // rtmp (Proxy) or SRT (Grid)
						{
							rtmpOutputFound = true;
						}
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				if (liveProxyWorking && rtmpOutputFound)
				{
					try
					{
						LOG_INFO(
							"liveProxyMonitor nonMonotonousDTSInOutputLog check"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", configurationLabel: {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
						);

						// First health check (rtmp), looks the log and check there is no message like
						//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to
						// 95383372. This may result in incorrect timestamps in the output file. 	This message causes proxy not working
						if (sourceLiveProxy->_callbackData->getNonMonotonousDts())
						{
							liveProxyWorking = false;

							LOG_ERROR(
								"liveProxyMonitor (rtmp). Live Proxy is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. "
								"LiveProxy (ffmpeg) is killed in order to be started again"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", copiedLiveProxy->_childProcessId: {}",
								copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, copiedLiveProxy->_childProcessId.toString()
							);

							localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
						}
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveProxyMonitor (rtmp) Non-monotonous DTS failed"
							", copiedLiveProxy->_ingestionJobKey: {}"
							", copiedLiveProxy->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
						);
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
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
				// Inoltre questo controllo viene fatto se sono passati almeno 3 minuti da quando live proxy è partito,
				// in order to be sure the manifest file was already created
				if (liveProxyWorking && liveProxyLiveTimeInSeconds > 3 * 60)
				{
					LOG_INFO(
						"liveProxyMonitor segments check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
					);

					for (int outputIndex = 0; outputIndex < copiedLiveProxy->_outputsRoot.size(); outputIndex++)
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
									{
										string manifestFilePathName = std::format("{}/{}", manifestDirectoryPath, manifestFileName);
										vector<string> chunksTooOldToBeRemoved;

										string manifestDirectoryPathName;
										{
											size_t manifestFilePathIndex = manifestFilePathName.find_last_of('/');
											if (manifestFilePathIndex == string::npos)
											{
												string errorMessage = std::format(
													"liveProxyMonitor. No manifestDirectoryPath find in the m3u8/mpd file path name"
													", copiedLiveProxy->_ingestionJobKey: {}"
													", copiedLiveProxy->_encodingJobKey: {}"
													", manifestFilePathName: {}",
													copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, manifestFilePathName
												);
												LOG_ERROR(errorMessage);

												throw runtime_error(errorMessage);
											}
											manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
										}

										chrono::system_clock::time_point lastChunkTimestamp =
											copiedLiveProxy->_encodingStart ? *(copiedLiveProxy->_encodingStart) : now;
										bool firstChunkRead = false;

										try
										{
											if (exists(manifestDirectoryPathName))
											{
												// chunks will be removed 10 minutes after the "capacity" of the playlist
												// long liveProxyChunkRetentionInSeconds =
												// 	(segmentDurationInSeconds * playlistEntriesNumber)
												// 	+ 10 * 60;	// 10 minutes
												long liveProxyChunkRetentionInSeconds = 10 * 60; // 10 minutes

												for (fs::directory_entry const &entry : fs::directory_iterator(manifestDirectoryPathName))
												{
													try
													{
														if (!entry.is_regular_file())
															continue;

														string dashPrefixInitFiles("init-stream");
														if (outputType == "DASH" &&
															entry.path().filename().string().size() >= dashPrefixInitFiles.size() &&
															0 == entry.path().filename().string().compare(
																	 0, dashPrefixInitFiles.size(), dashPrefixInitFiles
																 ))
															continue;

														{
															string segmentPathNameToBeRemoved = entry.path().string();

															chrono::system_clock::time_point fileLastModification =
																chrono::time_point_cast<chrono::system_clock::duration>(
																	fs::last_write_time(entry) - fs::file_time_type::clock::now() +
																	chrono::system_clock::now()
																);

															int64_t lastFileUpdateInSeconds =
																chrono::duration_cast<chrono::seconds>(
																								  chrono::system_clock::now() - fileLastModification).count();
															if (lastFileUpdateInSeconds > liveProxyChunkRetentionInSeconds)
															{
																LOG_INFO(
																	"liveProxyMonitor. chunk to be removed, too old"
																	", copiedLiveProxy->_ingestionJobKey: {}"
																	", copiedLiveProxy->_encodingJobKey: {}"
																	", segmentPathNameToBeRemoved: {}"
																	", lastFileUpdateInSeconds: {} seconds ago"
																	", liveProxyChunkRetentionInSeconds: {}",
																	copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey,
																	segmentPathNameToBeRemoved, lastFileUpdateInSeconds,
																	liveProxyChunkRetentionInSeconds
																);

																chunksTooOldToBeRemoved.push_back(segmentPathNameToBeRemoved);
															}

															if (!firstChunkRead || fileLastModification > lastChunkTimestamp)
																lastChunkTimestamp = fileLastModification;

															firstChunkRead = true;
														}
													}
													catch (runtime_error &e)
													{
														string errorMessage = std::format(
															"liveProxyMonitor. listing directory failed"
															", copiedLiveProxy->_ingestionJobKey: {}"
															", copiedLiveProxy->_encodingJobKey: {}"
															", manifestDirectoryPathName: {}"
															", e.what(): {}",
															copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey,
															manifestDirectoryPathName, e.what()
														);
														LOG_ERROR(errorMessage);

														// throw e;
													}
													catch (exception &e)
													{
														string errorMessage = std::format(
															"liveProxyMonitor. listing directory failed"
															", copiedLiveProxy->_ingestionJobKey: {}"
															", copiedLiveProxy->_encodingJobKey: {}"
															", manifestDirectoryPathName: {}"
															", e.what(): {}",
															copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey,
															manifestDirectoryPathName, e.what()
														);
														LOG_ERROR(errorMessage);

														// throw e;
													}
												}
											}
										}
										catch (exception &e)
										{
											LOG_ERROR(
												"liveProxyMonitor. scan LiveProxy files failed"
												", _ingestionJobKey: {}"
												", _encodingJobKey: {}"
												", manifestDirectoryPathName: {}"
												", e.what(): {}",
												copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, manifestDirectoryPathName,
												e.what()
											);
										}

										if (!firstChunkRead || lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
										{
											// if we are here, it means the ffmpeg command is not generating the ts files

											LOG_ERROR(
												"liveProxyMonitor. Chunks were not generated"
												", copiedLiveProxy->_ingestionJobKey: {}"
												", copiedLiveProxy->_encodingJobKey: {}"
												", manifestDirectoryPathName: {}"
												", firstChunkRead: {}",
												copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, manifestDirectoryPathName,
												firstChunkRead
											);

											liveProxyWorking = false;
											localErrorMessage = " restarted because of 'no segments were generated'";

											LOG_ERROR(
												"liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor. Live Proxy is not "
												"working (no segments were generated). LiveProxy (ffmpeg) is killed in order to be started again"
												", ingestionJobKey: {}"
												", encodingJobKey: {}"
												", manifestFilePathName: {}"
												", copiedLiveProxy->_childProcessId: {}",
												copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, manifestFilePathName,
												copiedLiveProxy->_childProcessId.toString()
											);

											// we killed the process, we do not care to remove the too old segments
											// since we will remove the entire directory
											break;
										}

										{
											for (string segmentPathNameToBeRemoved : chunksTooOldToBeRemoved)
											{
												try
												{
													LOG_INFO(
														"liveProxyMonitor. Remove chunk because too old"
														", copiedLiveProxy->_ingestionJobKey: {}"
														", copiedLiveProxy->_encodingJobKey: {}"
														", segmentPathNameToBeRemoved: {}",
														copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey,
														segmentPathNameToBeRemoved
													);
													fs::remove_all(segmentPathNameToBeRemoved);
												}
												catch (runtime_error &e)
												{
													LOG_ERROR(
														"liveProxyMonitor. remove failed"
														", copiedLiveProxy->_ingestionJobKey: {}"
														", copiedLiveProxy->_encodingJobKey: {}"
														", segmentPathNameToBeRemoved: {}"
														", exception: {}",
														copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey,
														segmentPathNameToBeRemoved, e.what()
													);
												}
											}
										}
									}
								}
							}
							catch (exception &e)
							{
								LOG_ERROR(
									"liveProxyMonitor (HLS) on segments (and retention) failed"
									", copiedLiveProxy->_ingestionJobKey: {}"
									", copiedLiveProxy->_encodingJobKey: {}"
									", e.what(): {}",
									copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
								);
							}
						}
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				// 2025-11-26: Diamo il tempo all'encoder di partire prima di eseguire il controllo (liveProxyLiveTimeInSeconds > 1 * 60).
				//	Questo perchè ho avuto uno scenario in cui l'encoder riprovava tante volte a partire fallendo ogni volta.
				//	ProxyStart viene aggiornata ad ogni tentativo di partenza.
				//	Ad un certo punto si è attivato anche questo controllo che, non vedendo i dati real time cambiare, killava il processo.
				//	Per questo motivo ho aggiunto il check: liveProxyLiveTimeInSeconds > 1 * 60
				if (liveProxyWorking && copiedLiveProxy->_monitoringRealTimeInfoEnabled && liveProxyLiveTimeInSeconds > 1 * 60)
				{
					// 2025-11-25: E' importante che callbackData stia raccogliendo i dati, altrimenti il controllo non è possibile farlo
					if (copiedLiveProxy->_callbackData->getFinished())
					{
						LOG_INFO(
							"liveProxyMonitor getRealTimeInfoByOutputLog check"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", configurationLabel: {}"
							", _callbackData: {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
							JSONUtils::toString(copiedLiveProxy->_callbackData->toJson())
						);

						try
						{
							// Second health check, rtmp(Proxy)/SRT(Grid), looks if the frame is increasing
							tuple<int32_t, chrono::milliseconds, size_t, double, double> newRealTimeInfo = make_tuple(
								copiedLiveProxy->_callbackData->getProcessedFrames(),
								copiedLiveProxy->_callbackData->getProcessedOutputTimestampMilliSecs(),
								copiedLiveProxy->_callbackData->getProcessedSizeKBps(),
								copiedLiveProxy->_callbackData->getBitRateKbps(),
								copiedLiveProxy->_callbackData->getFramePerSeconds()
							);

							sourceLiveProxy->_lastRealTimeInfo = newRealTimeInfo;

							// 2026-01-22: _lastRealTimeInfo è inizialmente inizializzato a 0 per tutti i campi.
							// Poichè questo controllo inizia dopo 1 minuto dall'inizio del live proxy, è ragionevole pensare
							// che dovremmo già avere dati realtime. Per cui iniziamo subito a fare il confronto anche considerando
							// la struttura inizializzata a 0.
							// Questo ci permette di gestire anche lo scenario in cui il processo è partito ma rimane "bloccato"
							// senza produrre dati real time. Questo scenario era invece non gestito in precedenza.
							// if (copiedLiveProxy->_lastRealTimeInfo)
							{
								int64_t elapsedInSecondsSinceLastChange =
									chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - copiedLiveProxy->_realTimeLastChange).count();

								if (copiedLiveProxy->_lastRealTimeInfo == newRealTimeInfo)
								{
									// real time info non sono cambiate
									if (elapsedInSecondsSinceLastChange > _maxRealTimeInfoNotChangedToleranceInSeconds)
									{
										LOG_ERROR(
											"liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor (rtmp). Live Proxy real time "
											"info are not changing. LiveProxy (ffmpeg) is killed in order to be "
											"started again"
											", ingestionJobKey: {}"
											", encodingJobKey: {}"
											", configurationLabel: {}"
											", copiedLiveProxy->_childProcessId: {}"
											", elapsedInSecondsSinceLastChange: {}"
											", _maxRealTimeInfoNotChangedToleranceInSeconds: {}",
											copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
											copiedLiveProxy->_childProcessId.toString(),
											elapsedInSecondsSinceLastChange, _maxRealTimeInfoNotChangedToleranceInSeconds
										);

										liveProxyWorking = false;
										localErrorMessage = " restarted because of 'real time info not changing'";
									}
									else
									{
										LOG_INFO(
											"liveProxyMonitor. Live Proxy real time info is not changed within the tolerance"
											", ingestionJobKey: {}"
											", encodingJobKey: {}"
											", configurationLabel: {}"
											", copiedLiveProxy->_childProcessId: {}"
											", elapsedInSecondsSinceLastChange: {}"
											", _maxRealTimeInfoNotChangedToleranceInSeconds: {}",
											copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
											copiedLiveProxy->_childProcessId.toString(), elapsedInSecondsSinceLastChange,
											_maxRealTimeInfoNotChangedToleranceInSeconds
										);
									}
								}
								else
									sourceLiveProxy->_realTimeLastChange = chrono::system_clock::now();
							}
							// else
							//	sourceLiveProxy->_realTimeLastChange = chrono::system_clock::now();
						}
						catch (exception &e)
						{
							LOG_ERROR(
								"liveProxyMonitor (rtmp) real time info check failed"
								", copiedLiveProxy->_ingestionJobKey: {}"
								", copiedLiveProxy->_encodingJobKey: {}"
								", e.what(): {}",
								copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
							);
						}
					}
					else
					{
						LOG_WARN(
							"liveProxyMonitor getRealTimeInfoByOutputLog check cannot be done because no callbackdata available"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", configurationLabel: {}"
							", _callbackData: {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
							JSONUtils::toString(copiedLiveProxy->_callbackData->toJson())
						);
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				/* 2025-12-16: Discontinuity: [vist#0:1/h264] timestamp discontinuity (stream id=0): -20390288, new offset= -9998357
				Significa che:
				- il PTS/DTS in ingresso ha fatto un salto
				- FFmpeg ha ricalcolato un offset per continuare la timeline
				- il demuxer NON è crashato
				Non è di per sé un errore, infatti spesso lo streaming funzione bene (dipende da DOVE cade la discontinuità),
				é tipico di:
				- SRT / RTMP / UDP
				- encoder upstream che riparte
				- cambi di GOP
				- encoder hardware
				FFmpeg riallinea il tempo
				- I segmenti successivi hanno timestamp coerenti
				- Il player non se ne accorge
				In alcuni casi potrebbe causare problemi quando
				- Tanti timestamp discontinuity ravvicinati
				- Salti grandi (decine di secondi)
				- Segmenti HLS generati ma con:
				- durata errata
				- EXT-X-PROGRAM-DATE-TIME instabile
				La discontinuità diventa fatale quando abbiamo:
				- Non-monotonous DTS
				- PTS < DTS
				- DTS out of order
				- Invalid NAL unit
				- PPS id out of range
				- Error muxing packet
				- av_interleaved_write_frame(): Invalid argument
				- tanti discontinuity in pochi secondi Es.: ≥ 5 volte in 30s oppure crescita continua per 60s
					(ho visto 1300 discontinuities in 30 secondi)
				Oppure:
				- HLS smette di aggiornarsi
				- playlist .m3u8 non cresce
				- segmenti .ts non vengono più creati
				 */
				if (liveProxyWorking)
				{
					LOG_INFO(
						"liveProxyMonitor too timestamp discontinuities in time window check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
					);

					try
					{
						if (copiedLiveProxy->_callbackData->getTimestampDiscontinuityCountInTimeWindow() >
							_maxRealTimeInfoTimestampDiscontinuitiesInTimeWindow)
						{
							LOG_ERROR(
								"liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor (rtmp). "
								"Too timestamp discontinuities in time window failed. LiveProxy (ffmpeg) is killed in order to be started again"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", copiedLiveProxy->_childProcessId: {}"
								", timestampDiscontinuityCountInTimeWindow: {}"
								", _maxRealTimeInfoTimestampDiscontinuitiesInTimeWindow: {}",
								copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, copiedLiveProxy->_childProcessId.toString(),
								copiedLiveProxy->_callbackData->getTimestampDiscontinuityCountInTimeWindow(), _maxRealTimeInfoTimestampDiscontinuitiesInTimeWindow
							);

							liveProxyWorking = false;
							localErrorMessage = " restarted because of too timestamp discontinuities in time window";
						}
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveProxyMonitor (rtmp) too timestamp discontinuities in time window check failed"
							", copiedLiveProxy->_ingestionJobKey: {}"
							", copiedLiveProxy->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
						);
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				if (liveProxyWorking)
				{
					LOG_INFO(
						"liveProxyMonitor forbiddenErrorInOutputLog check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
					);

					try
					{
						if (sourceLiveProxy->_callbackData->getUrlForbidden())
						{
							LOG_ERROR(
								"liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor (rtmp). Live Proxy is returning 'HTTP "
								"error 403 Forbidden'. LiveProxy (ffmpeg) is killed in order to be started again"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", copiedLiveProxy->_childProcessId: {}",
								copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, copiedLiveProxy->_childProcessId.toString()
							);

							liveProxyWorking = false;
							localErrorMessage = " restarted because of 'HTTP error 403 Forbidden'";
						}
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveProxyMonitor (rtmp) HTTP error 403 Forbidden check failed"
							", copiedLiveProxy->_ingestionJobKey: {}"
							", copiedLiveProxy->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
						);
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				if (liveProxyWorking)
				{
					LOG_INFO(
						"liveProxyMonitor Both TLS and open resource errors check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
					);

					try
					{
						if (sourceLiveProxy->_callbackData->getTlsAndOpenResourceError())
						{
							LOG_ERROR(
								"liveProxyMonitor. ProcessUtility::kill/quit/term Process. Both TLS and open resource errors detected. "
								"LiveProxy (ffmpeg) is killed in order to be started again"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", copiedLiveProxy->_childProcessId: {}",
								copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, copiedLiveProxy->_childProcessId.toString()
							);

							liveProxyWorking = false;
							localErrorMessage = " restarted because of TLS and open resource errors";
						}
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveProxyMonitor (rtmp) Both TLS and open resource errors check failed"
							", copiedLiveProxy->_ingestionJobKey: {}"
							", copiedLiveProxy->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
						);
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				if (liveProxyWorking)
				{
					LOG_INFO(
						"liveProxyMonitor Segment Failed Too Many Times check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel
					);

					try
					{
						if (sourceLiveProxy->_callbackData->getSegmentFailedTooManyTimes())
						{
							LOG_ERROR(
								"liveProxyMonitor. ProcessUtility::kill/quit/term Process. Segment Failed Too Many Times. "
								"LiveProxy (ffmpeg) is killed in order to be started again"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", copiedLiveProxy->_childProcessId: {}",
								copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, copiedLiveProxy->_childProcessId.toString()
							);

							liveProxyWorking = false;
							localErrorMessage = " restarted because of Segment Failed Too Many Times";
						}
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveProxyMonitor (rtmp) Segment Failed Too Many Times"
							", copiedLiveProxy->_ingestionJobKey: {}"
							", copiedLiveProxy->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, e.what()
						);
					}
				}

				if (!sourceLiveProxy->_childProcessId.isInitialized() || copiedLiveProxy->_encodingStart != sourceLiveProxy->_encodingStart)
				{
					LOG_INFO(
						"liveProxyMonitor. LiveProxy changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", sourceLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
						sourceLiveProxy->_childProcessId.toString()
					);

					continue;
				}

				if (!liveProxyWorking)
				{
					LOG_ERROR(
						"liveProxyMonitor. ProcessUtility::kill/quit/term Process. liveProxyMonitor. LiveProxy (ffmpeg) is killed/quit in order to "
						"be started again"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", configurationLabel: {}"
						", localErrorMessage: {}"
						// + ", channelLabel: " + copiedLiveProxy->_channelLabel
						", copiedLiveProxy->_childProcessId: {}",
						copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel, localErrorMessage,
						copiedLiveProxy->_childProcessId.toString()
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

						termProcess(sourceLiveProxy, copiedLiveProxy->_ingestionJobKey, configurationLabel, localErrorMessage, false);

						sourceLiveProxy->_callbackData->pushErrorMessage(std::format(
							"{} {}", Datetime::nowLocalTime(), localErrorMessage
						));
					}
					catch (runtime_error &e)
					{
						LOG_ERROR(
							"liveProxyMonitor. ProcessUtility::kill/quit/term Process failed"
							", copiedLiveProxy->_ingestionJobKey: {}"
							", copiedLiveProxy->_encodingJobKey: {}"
							", configurationLabel: {}"
							", copiedLiveProxy->_childProcessId: {}"
							", e.what(): {}",
							copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey, configurationLabel,
							copiedLiveProxy->_childProcessId.toString(), e.what()
						);
					}
				}

				LOG_INFO(
					"liveProxyMonitor {}/{}"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", configurationLabel: {}"
					", @MMS statistics@ - elapsed time: @{}@",
					liveProxyIndex, liveProxyAndGridRunningCounter, copiedLiveProxy->_ingestionJobKey, copiedLiveProxy->_encodingJobKey,
					configurationLabel, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - now).count()
				);
			}
			LOG_INFO(
				"liveProxyMonitor"
				", liveProxyAndGridRunningCounter: {}"
				", @MMS statistics@ - elapsed (millisecs): {}",
				liveProxyAndGridRunningCounter, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - monitorStart).count()
			);
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"liveProxyMonitor failed"
				", e.what(): {}",
				e.what()
			);
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

				for (const shared_ptr<LiveRecording>& liveRecording : *_liveRecordingsCapability)
				{
					if (liveRecording->_childProcessId.isInitialized() && liveRecording->_monitoringEnabled)
					{
						liveRecordingRunningCounter++;

						copiedRunningLiveRecordingCapability.push_back(liveRecording->cloneForMonitorAndVirtualVOD());
						sourceLiveRecordingCapability.push_back(liveRecording);
					}
					else
					{
						liveRecordingNotRunningCounter++;
					}
				}
				LOG_INFO(
					"liveRecordingMonitor, numbers"
					", total LiveRecording: {}"
					", liveRecordingRunningCounter: {}"
					", liveRecordingNotRunningCounter: {}",
					liveRecordingRunningCounter + liveRecordingNotRunningCounter, liveRecordingRunningCounter, liveRecordingNotRunningCounter
				);
			}
			LOG_INFO(
				"liveRecordingMonitor clone"
				", copiedRunningLiveRecordingCapability.size: "
				", @MMS statistics@ - elapsed (millisecs): ",
				copiedRunningLiveRecordingCapability.size(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startClone).count()
			);

			chrono::system_clock::time_point monitorStart = chrono::system_clock::now();

			for (int liveRecordingIndex = 0; liveRecordingIndex < copiedRunningLiveRecordingCapability.size(); liveRecordingIndex++)
			{
				const shared_ptr<LiveRecording>& copiedLiveRecording = copiedRunningLiveRecordingCapability[liveRecordingIndex];
				const shared_ptr<LiveRecording>& sourceLiveRecording = sourceLiveRecordingCapability[liveRecordingIndex];

				LOG_INFO(
					"liveRecordingMonitor"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", channelLabel: {}",
					copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
				);

				chrono::system_clock::time_point now = chrono::system_clock::now();

				bool liveRecorderWorking = true;
				string localErrorMessage;

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					LOG_INFO(
						"liveRecordingMonitor. LiveRecorder changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
					);

					continue;
				}

				// copiedLiveRecording->_recordingStart could be a bit in the future
				int64_t liveRecordingLiveTimeInSeconds = 0;
				if (copiedLiveRecording->_encodingStart)
				{
					if (now > *(copiedLiveRecording->_encodingStart))
						liveRecordingLiveTimeInSeconds = chrono::duration_cast<chrono::seconds>(now - *(copiedLiveRecording->_encodingStart)).count();
					else // it will be negative
						liveRecordingLiveTimeInSeconds = chrono::duration_cast<chrono::seconds>(now - *(copiedLiveRecording->_encodingStart)).count();
				}

				string field = "segmentDuration";
				int segmentDurationInSeconds = JSONUtils::asInt32(copiedLiveRecording->_ingestedParametersRoot, field, -1);

				// check is done after 5 minutes + segmentDurationInSeconds LiveRecording started,
				// in order to be sure the file was already created
				// Commentato alcuni controlli possono essere fatti anche da subito. Aggiunto questo controllo per il caso specifico del manifest
				/*
			if (liveRecordingLiveTimeInMinutes <= (segmentDurationInSeconds / 60) + 5)
			{
				info(
					__FILEREF__ + "liveRecordingMonitor. Checks are not done because too early" + ", ingestionJobKey: " +
					to_string(copiedLiveRecording->_ingestionJobKey) + ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey) +
					", channelLabel: " + copiedLiveRecording->_channelLabel +
					", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes) +
					", (segmentDurationInSeconds / 60) + 5: " + to_string((segmentDurationInSeconds / 60) + 5)
				);

				continue;
			}
			*/

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					LOG_INFO(
						"liveRecordingMonitor. LiveRecorder changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
					);

					continue;
				}

				// controlla se il lastModificationTime dell'output file di ffmpeg non è cambiato
				if (liveRecorderWorking)
				{
					LOG_INFO(
						"liveRecordingMonitor outputFFMpegFileLastModificationTime check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
					);
					uintmax_t previousOutputFfmpegFileSize = copiedLiveRecording->_lastOutputFfmpegFileSize;
					uintmax_t newOutputFfmpegFileSize = sourceLiveRecording->_ffmpeg->getOutputFFMpegFileSize();
					sourceLiveRecording->_lastOutputFfmpegFileSize = newOutputFfmpegFileSize;
					if (previousOutputFfmpegFileSize != 0 && previousOutputFfmpegFileSize == newOutputFfmpegFileSize)
					{
						liveRecorderWorking = false;

						LOG_ERROR(
							"liveRecorderMonitor. output ffmpeg file size is not changing"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", previousOutputFfmpegFileSize: {}"
							", newOutputFfmpegFileSize: {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, previousOutputFfmpegFileSize,
							newOutputFfmpegFileSize
						);

						localErrorMessage = " restarted because of 'output ffmpeg file size is not changing'";
					}
				}

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					LOG_INFO(
						"liveRecordingMonitor. LiveRecorder changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
					);

					continue;
				}

				// First health check
				//		kill if 1840699_408620.liveRecorder.list file does not exist or was not updated in the last (2 * segment duration in secs)
				// seconds
				// Inoltre questo controllo viene fatto se sono passati almeno 3 minuti da quando live recording è partito,
				// in order to be sure the manifest file was already created
				if (liveRecorderWorking && liveRecordingLiveTimeInSeconds > 3 * 60)
				{
					LOG_INFO(
						"liveRecordingMonitor. liveRecorder.list check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
					);

					try
					{
						// looking the manifests path name timestamp

						string segmentListPathName =
							copiedLiveRecording->_chunksTranscoderStagingContentsPath + copiedLiveRecording->_segmentListFileName;
						if (!exists(segmentListPathName))
						{
							liveRecorderWorking = false;

							LOG_ERROR(
								"liveRecordingMonitor. Segment list file does not exist"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", liveRecordingLiveTimeInSeconds: {}"
								", segmentListPathName: {}",
								copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, liveRecordingLiveTimeInSeconds,
								segmentListPathName
							);

							localErrorMessage = " restarted because of 'segment list file is missing or was not updated'";
						}
						else
						{
							int64_t lastSegmentListFileUpdateInSeconds;
							{
								chrono::system_clock::time_point fileLastModification = chrono::time_point_cast<chrono::system_clock::duration>(
									fs::last_write_time(segmentListPathName) - fs::file_time_type::clock::now() + chrono::system_clock::now()
								);

								lastSegmentListFileUpdateInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - fileLastModification).count();
							}

							long maxLastSegmentListFileUpdateInSeconds = segmentDurationInSeconds * 2;

							if (lastSegmentListFileUpdateInSeconds > maxLastSegmentListFileUpdateInSeconds)
							{
								liveRecorderWorking = false;

								LOG_ERROR(
									"liveRecordingMonitor. Segment list file was not updated in the last {} seconds"
									", ingestionJobKey: {}"
									", encodingJobKey: {}"
									", liveRecordingLiveTimeInSeconds: {}"
									", segmentListPathName: {}"
									", lastSegmentListFileUpdateInSeconds: {} seconds ago"
									", maxLastSegmentListFileUpdateInSeconds: {}",
									maxLastSegmentListFileUpdateInSeconds, copiedLiveRecording->_ingestionJobKey,
									copiedLiveRecording->_encodingJobKey, liveRecordingLiveTimeInSeconds, segmentListPathName,
									lastSegmentListFileUpdateInSeconds, maxLastSegmentListFileUpdateInSeconds
								);

								localErrorMessage = " restarted because of 'segment list file is missing or was not updated'";
							}
						}
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveRecordingMonitor on path name failed"
							", copiedLiveRecording->_ingestionJobKey: {}"
							", copiedLiveRecording->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, e.what()
						);
					}
				}

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					LOG_INFO(
						"liveRecordingMonitor. LiveRecorder changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
					);

					continue;
				}

				// Second health check
				//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
				//		rtmp(Proxy):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
				//			This check has to be done just once (not for each outputRoot) in case we have at least one rtmp output
				// Inoltre questo controllo viene fatto se sono passati almeno 3 minuti da quando live recording è partito,
				// in order to be sure the manifest file was already created
				bool rtmpOutputFound = false;
				if (liveRecorderWorking && liveRecordingLiveTimeInSeconds > 3 * 60)
				{
					LOG_INFO(
						"liveRecordingMonitor. manifest check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
					);

					json outputsRoot = copiedLiveRecording->_encodingParametersRoot["outputsRoot"];
					for (const auto& outputRoot : outputsRoot)
					{
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

								string manifestFilePathName = std::format("{}/{}", manifestDirectoryPath, manifestFileName);
								if (!exists(manifestFilePathName))
								{
									liveRecorderWorking = false;

									LOG_ERROR(
										"liveRecorderMonitor. Manifest file does not exist"
										", ingestionJobKey: {}"
										", encodingJobKey: {}"
										", manifestFilePathName: {}",
										copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, manifestFilePathName
									);

									localErrorMessage = " restarted because of 'manifest file is missing'";

									break;
								}

								int64_t lastManifestFileUpdateInSeconds;
								{
									chrono::system_clock::time_point fileLastModification =
										chrono::time_point_cast<chrono::system_clock::duration>(
											fs::last_write_time(manifestFilePathName) - fs::file_time_type::clock::now() +
											chrono::system_clock::now()
										);

									lastManifestFileUpdateInSeconds =
										chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - fileLastModification).count();
								}

								long maxLastManifestFileUpdateInSeconds = 45;

								if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
								{
									liveRecorderWorking = false;

									LOG_ERROR(
										"liveRecorderMonitor. Manifest file was not updated in the last {} seconds"
										", ingestionJobKey: {}"
										", encodingJobKey: {}"
										", manifestFilePathName: {}"
										", lastManifestFileUpdateInSeconds: {} seconds ago"
										", maxLastManifestFileUpdateInSeconds: {}",
										maxLastManifestFileUpdateInSeconds, copiedLiveRecording->_ingestionJobKey,
										copiedLiveRecording->_encodingJobKey, manifestFilePathName, lastManifestFileUpdateInSeconds,
										maxLastManifestFileUpdateInSeconds
									);

									localErrorMessage = " restarted because of 'manifest file was not updated'";

									break;
								}
							}
							catch (exception &e)
							{
								LOG_ERROR(
									"liveRecorderMonitor (HLS) on manifest path name failed"
									", copiedLiveRecording->_ingestionJobKey: {}"
									", copiedLiveRecording->_encodingJobKey: {}"
									", e.what(): {}",
									copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, e.what()
								);
							}
						}
						else // rtmp (Proxy)
						{
							rtmpOutputFound = true;
						}
					}
				}

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					LOG_INFO(
						"liveRecordingMonitor. LiveRecorder changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
					);

					continue;
				}

				if (liveRecorderWorking && rtmpOutputFound)
				{
					try
					{
						LOG_INFO(
							"liveRecordingMonitor. nonMonotonousDTSInOutputLog check"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", channelLabel: {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
						);

						// First health check (rtmp), looks the log and check there is no message like
						//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to
						// 95383372. This may result in incorrect timestamps in the output file. 	This message causes proxy not working
						if (sourceLiveRecording->_callbackData->getNonMonotonousDts())
						{
							liveRecorderWorking = false;

							LOG_ERROR(
								"liveRecorderMonitor (rtmp). Live Recorder is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. "
								"LiveRecorder (ffmpeg) is killed in order to be started again"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", channelLabel: {}"
								", copiedLiveRecording->_childProcessId: {}",
								copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
								copiedLiveRecording->_childProcessId.toString()
							);

							localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
						}
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveRecorderMonitor (rtmp) Non-monotonous DTS failed"
							", copiedLiveRecording->_ingestionJobKey: {}"
							", copiedLiveRecording->_encodingJobKey: {}"
							", e.what(): {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, e.what()
						);
					}
				}

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					LOG_INFO(
						"liveRecordingMonitor. LiveRecorder changed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
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
				// Inoltre questo controllo viene fatto se sono passati almeno 3 minuti da quando live recording è partito,
				// in order to be sure the manifest file was already created
				if (liveRecorderWorking && liveRecordingLiveTimeInSeconds > 3 * 60)
				{
					LOG_INFO(
						"liveRecordingMonitor. segment check"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", channelLabel: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
					);

					json outputsRoot = copiedLiveRecording->_encodingParametersRoot["outputsRoot"];
					for (const auto& outputRoot : outputsRoot)
					{
						string outputType = JSONUtils::asString(outputRoot, "outputType", "");
						string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");
						string manifestFileName = JSONUtils::asString(outputRoot, "manifestFileName", "");
						int outputPlaylistEntriesNumber = JSONUtils::asInt32(outputRoot, "playlistEntriesNumber", 10);
						int outputSegmentDurationInSeconds = JSONUtils::asInt32(outputRoot, "segmentDurationInSeconds", 10);

						if (!liveRecorderWorking)
							break;

						// if (outputType == "HLS" || outputType == "DASH")
						if (outputType == "HLS_Channel")
						{
							try
							{
								string manifestFilePathName = std::format("{}/{}", manifestDirectoryPath, manifestFileName);
								{
									vector<string> chunksTooOldToBeRemoved;

									string manifestDirectoryPathName;
									{
										size_t manifestFilePathIndex = manifestFilePathName.find_last_of('/');
										if (manifestFilePathIndex == string::npos)
										{
											string errorMessage = std::format(
												"liveRecordingMonitor. No manifestDirectoryPath find in the m3u8/mpd file path name"
												", liveRecorder->_ingestionJobKey: {}"
												", liveRecorder->_encodingJobKey: {}"
												", manifestFilePathName: {}",
												copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, manifestFilePathName
											);
											LOG_ERROR(errorMessage);

											throw runtime_error(errorMessage);
										}
										manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
									}

									chrono::system_clock::time_point lastChunkTimestamp =
										copiedLiveRecording->_encodingStart ? *(copiedLiveRecording->_encodingStart) : chrono::system_clock::now();
									bool firstChunkRead = false;

									try
									{
										if (exists(manifestDirectoryPathName))
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
												(outputSegmentDurationInSeconds * outputPlaylistEntriesNumber) + (10 * 60); // 10 minutes
											LOG_INFO(
												"liveRecordingMonitor. segment check"
												", ingestionJobKey: {}"
												", encodingJobKey: {}"
												", channelLabel: {}"
												", outputSegmentDurationInSeconds: {}"
												", outputPlaylistEntriesNumber: {}"
												", liveProxyChunkRetentionInSeconds: {}",
												copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
												copiedLiveRecording->_channelLabel, outputSegmentDurationInSeconds, outputPlaylistEntriesNumber,
												liveProxyChunkRetentionInSeconds
											);

											for (fs::directory_entry const &entry : fs::directory_iterator(manifestDirectoryPathName))
											{
												try
												{
													if (!entry.is_regular_file())
														continue;

													string dashPrefixInitFiles("init-stream");
													if (outputType == "DASH" &&
														entry.path().filename().string().size() >= dashPrefixInitFiles.size() &&
														0 == entry.path().filename().string().compare(
																 0, dashPrefixInitFiles.size(), dashPrefixInitFiles
															 ))
														continue;

													{
														string segmentPathNameToBeRemoved = entry.path();

														chrono::system_clock::time_point fileLastModification =
															chrono::time_point_cast<chrono::system_clock::duration>(
																fs::last_write_time(entry) - fs::file_time_type::clock::now() +
																chrono::system_clock::now()
															);

														int64_t lastFileUpdateInSeconds =
															chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - fileLastModification).count();
														if (lastFileUpdateInSeconds > liveProxyChunkRetentionInSeconds)
														{
															LOG_INFO(
																"liveRecordingMonitor. chunk to be removed, too old"
																", copiedLiveRecording->_ingestionJobKey: {}"
																", copiedLiveRecording->_encodingJobKey: {}"
																", segmentPathNameToBeRemoved: {}"
																", lastFileUpdateInSeconds: {} seconds ago"
																", liveProxyChunkRetentionInSeconds: {}",
																copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
																segmentPathNameToBeRemoved, lastFileUpdateInSeconds, liveProxyChunkRetentionInSeconds
															);

															chunksTooOldToBeRemoved.push_back(segmentPathNameToBeRemoved);
														}

														if (!firstChunkRead || fileLastModification > lastChunkTimestamp)
															lastChunkTimestamp = fileLastModification;

														firstChunkRead = true;
													}
												}
												catch (exception &e)
												{
													LOG_ERROR(
														"liveRecordingMonitor. listing directory failed"
														", copiedLiveRecording->_ingestionJobKey: {}"
														", copiedLiveRecording->_encodingJobKey: {}"
														", manifestDirectoryPathName: {}"
														", e.what(): {}",
														copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
														manifestDirectoryPathName, e.what()
													);
												}
											}
										}
									}
									catch (exception &e)
									{
										LOG_ERROR(
											"liveRecordingMonitor. scan LiveRecorder files failed"
											", _ingestionJobKey: {}"
											", _encodingJobKey: {}"
											", manifestDirectoryPathName: {}"
											", e.what(): {}",
											copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, manifestDirectoryPathName,
											e.what()
										);
									}

									if (!firstChunkRead || lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
									{
										// if we are here, it means the ffmpeg command is not generating the ts files

										LOG_ERROR(
											"liveRecordingMonitor. Chunks were not generated"
											", _ingestionJobKey: {}"
											", _encodingJobKey: {}"
											", manifestDirectoryPathName: {}"
											", firstChunkRead: {}",
											copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, manifestDirectoryPathName,
											firstChunkRead
										);

										liveRecorderWorking = false;
										localErrorMessage = " restarted because of 'no segments were generated'";

										LOG_ERROR(
											"liveRecordingMonitor. ProcessUtility::kill/quit/term Process. liveRecorderMonitor. Live Recorder is not "
											"working (no segments were generated). LiveRecorder (ffmpeg) is killed in order to be started again"
											", _ingestionJobKey: {}"
											", _encodingJobKey: {}"
											", manifestDirectoryPathName: {}"
											", channelLabel: {}"
											", liveRecorder->_childProcessId: {}",
											copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, manifestDirectoryPathName,
											copiedLiveRecording->_channelLabel, copiedLiveRecording->_childProcessId.toString()
										);

										// we killed the process, we do not care to remove the too old segments
										// since we will remove the entore directory
										break;
									}

									{
										for (string segmentPathNameToBeRemoved : chunksTooOldToBeRemoved)
										{
											try
											{
												LOG_INFO(
													"liveRecordingMonitor. Remove chunk because too old"
													", _ingestionJobKey: {}"
													", _encodingJobKey: {}"
													", segmentPathNameToBeRemoved: {}",
													copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
													segmentPathNameToBeRemoved
												);
												fs::remove_all(segmentPathNameToBeRemoved);
											}
											catch (exception &e)
											{
												LOG_ERROR(
													"liveRecordingMonitor. remove failed"
													", _ingestionJobKey: {}"
													", _encodingJobKey: {}"
													", segmentPathNameToBeRemoved: {}"
													", e.what: {}",
													copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
													segmentPathNameToBeRemoved, e.what()
												);
											}
										}
									}
								}
							}
							catch (exception &e)
							{
								LOG_ERROR(
									"liveRecorderMonitor (HLS) on segments (and retention) failed"
									", copiedLiveRecording->_ingestionJobKey: {}"
									", copiedLiveRecording->_encodingJobKey: {}"
									", e.what(): {}",
									copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, e.what()
								);
							}
						}
					}
				}

				if (!sourceLiveRecording->_childProcessId.isInitialized() ||
					copiedLiveRecording->_encodingStart != sourceLiveRecording->_encodingStart)
				{
					LOG_INFO(
						"liveRecordingMonitor. LiveRecorder changed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", channelLabel: {}"
						", sourceLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
						sourceLiveRecording->_childProcessId.toString()
					);

					continue;
				}

				// 2025-11-26: Diamo il tempo all'encoder di partire prima di eseguire il controllo (liveRecordingLiveTimeInSeconds > 1 * 60).
				//	Questo perchè ho avuto uno scenario in cui l'encoder riprovava tante volte a partire fallendo ogni volta.
				//	ProxyStart viene aggiornata ad ogni tentativo di partenza.
				//	Ad un certo punto si è attivato anche questo controllo che, non vedendo i dati real time cambiare, killava il processo.
				//	Per questo motivo ho aggiunto il check: liveRecordingLiveTimeInSeconds > 1 * 60
				if (liveRecorderWorking && copiedLiveRecording->_monitoringRealTimeInfoEnabled && liveRecordingLiveTimeInSeconds > 1 * 60)
				{
					// 2025-11-25: E' importante che callbackData stia raccogliendo i dati, altrimenti il controllo non è possibile farlo
					if (copiedLiveRecording->_callbackData->getFinished())
					{
						LOG_INFO(
							"liveRecordingMonitor. getRealTimeInfoByOutputLog check"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", channelLabel: {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel
						);

						try
						{
							// Second health check, rtmp(Proxy), looks if the frame is increasing
							tuple<int32_t, chrono::milliseconds, size_t, double, double> newRealTimeInfo = make_tuple(
								copiedLiveRecording->_callbackData->getProcessedFrames(),
								copiedLiveRecording->_callbackData->getProcessedOutputTimestampMilliSecs(),
								copiedLiveRecording->_callbackData->getProcessedSizeKBps(),
								copiedLiveRecording->_callbackData->getBitRateKbps(),
								copiedLiveRecording->_callbackData->getFramePerSeconds()
							);

							sourceLiveRecording->_lastRealTimeInfo = newRealTimeInfo;

							// 2026-01-22: _lastRealTimeInfo: vedi commento scritto per il Live proxy
							// if (copiedLiveRecording->_lastRealTimeInfo)
							{
								int64_t elapsedInSecondsSinceLastChange =
									chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - copiedLiveRecording->_realTimeLastChange)
										.count();

								// getTimestampDiscontinuityCount: vedi commento scritto per il Live proxy
								if (copiedLiveRecording->_lastRealTimeInfo == newRealTimeInfo)
								{
									// real time info not changed
									if (elapsedInSecondsSinceLastChange > _maxRealTimeInfoNotChangedToleranceInSeconds)
									{
										LOG_ERROR(
											"liveRecordingMonitor. ProcessUtility::kill/quit/term Process. liveRecordingMonitor (rtmp). Live "
											"Recorder real time info are not changing. LiveRecorder (ffmpeg) is killed in order to be started again"
											", ingestionJobKey: {}"
											", encodingJobKey: {}"
											", channelLabel: {}"
											", _childProcessId: {}"
											", elapsedInSecondsSinceLastChange: {}"
											", _maxRealTimeInfoNotChangedToleranceInSeconds: {}",
											copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
											copiedLiveRecording->_channelLabel, copiedLiveRecording->_childProcessId.toString(),
											elapsedInSecondsSinceLastChange,
											_maxRealTimeInfoNotChangedToleranceInSeconds
										);

										liveRecorderWorking = false;
										localErrorMessage = " restarted because of 'real time info not changing'";
									}
									else
									{
										LOG_INFO(
											"liveRecordingMonitor. Live Recorder real time info is not changed but within the tolerance"
											", ingestionJobKey: {}"
											", encodingJobKey: {}"
											", channelLabel: {}"
											", _childProcessId: {}"
											", elapsedInSecondsSinceLastChange: {}"
											", _maxRealTimeInfoNotChangedToleranceInSeconds: {}",
											copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
											copiedLiveRecording->_channelLabel, copiedLiveRecording->_childProcessId.toString(),
											elapsedInSecondsSinceLastChange, _maxRealTimeInfoNotChangedToleranceInSeconds
										);
									}
								}
								else
									sourceLiveRecording->_realTimeLastChange = chrono::system_clock::now();
							}
							// else
							//	sourceLiveRecording->_realTimeLastChange = chrono::system_clock::now();
						}
						catch (exception &e)
						{
							LOG_ERROR(
								"liveRecorderMonitor (rtmp) real time info check failed"
								", copiedLiveRecording->_ingestionJobKey: {}"
								", copiedLiveRecording->_encodingJobKey: {}"
								", e.what(): {}",
								copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, e.what()
							);
						}
					}
					else
					{
						LOG_WARN(
							"liveRecordingMonitor, getRealTimeInfoByOutputLog check cannot be done because no callbackdata available"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", channelLabel: {}"
							", _callbackData: {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
							JSONUtils::toString(copiedLiveRecording->_callbackData->toJson())
						);
					}
				}

				if (!liveRecorderWorking)
				{
					LOG_ERROR(
						"liveRecordingMonitor. ProcessUtility::kill/quit/term Process. liveRecordingMonitor. Live Recording is not working (segment "
						"list file is missing or was not updated). LiveRecording (ffmpeg) is killed in order to be started again"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", liveRecordingLiveTimeInSeconds: {}"
						", channelLabel: {}"
						", copiedLiveRecording->_childProcessId: {}",
						copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, liveRecordingLiveTimeInSeconds,
						copiedLiveRecording->_channelLabel, copiedLiveRecording->_childProcessId.toString()
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

						termProcess(sourceLiveRecording, copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_channelLabel,
							localErrorMessage, false);
						sourceLiveRecording->_callbackData->pushErrorMessage(std::format(
							"{} {}{}", Datetime::nowLocalTime(),
							sourceLiveRecording->_channelLabel, localErrorMessage
						));
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"liveRecordingMonitor. ProcessUtility::kill/quit Process failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", channelLabel: {}"
							", copiedLiveRecording->_childProcessId: {}"
							", e.what(): {}",
							copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey, copiedLiveRecording->_channelLabel,
							copiedLiveRecording->_childProcessId.toString(), e.what()
						);
					}
				}

				LOG_INFO(
					"liveRecordingMonitor {}/{}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", channelLabel: {}"
					", @MMS statistics@ - elapsed time: @{}@",
					liveRecordingIndex, liveRecordingRunningCounter, copiedLiveRecording->_ingestionJobKey, copiedLiveRecording->_encodingJobKey,
					copiedLiveRecording->_channelLabel, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - now).count()
				);
			}
			LOG_INFO(
				"liveRecordingMonitor"
				", liveRecordingRunningCounter: {}"
				", @MMS statistics@ - elapsed (millisecs): {}",
				liveRecordingRunningCounter, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - monitorStart).count()
			);
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"liveRecordingMonitor failed"
				", e.what(): {}",
				e.what()
			);
		}

		this_thread::sleep_for(chrono::seconds(_monitorCheckInSeconds));
	}
}

bool FFMPEGEncoderDaemons::exists(const string& pathName, const int retries, const int waitInSeconds)
{
	for (int i = 0; i < retries; i++)
	{
		if (fs::exists(pathName))
			return true;
		if (i + 1 < retries)
			this_thread::sleep_for(chrono::seconds(waitInSeconds));
	}

	return false;
}

void FFMPEGEncoderDaemons::stopMonitorThread()
{

	_monitorThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_monitorCheckInSeconds));
}

void FFMPEGEncoderDaemons::termProcess(
	const shared_ptr<FFMPEGEncoderBase::Encoding>& selectedEncoding, int64_t ingestionJobKey, string label, string message, bool kill
)
{
	try
	{
		// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liveProxy
		// 2023-02-18: using SIGQUIT, the process was not stopped, it worked with SIGTERM SIGTERM now is managed by FFMpeg.cpp too
		chrono::system_clock::time_point start = chrono::system_clock::now();
		ProcessUtility::ProcessId previousChildProcessId = selectedEncoding->_childProcessId;
		if (!previousChildProcessId.isInitialized())
			return;
		constexpr long secondsToWait = 10;
		int counter = 0;
		do
		{
			if (!selectedEncoding->_childProcessId.isInitialized() || selectedEncoding->_childProcessId != previousChildProcessId)
				break;

			if (kill)
				ProcessUtility::killProcess(previousChildProcessId);
			else
				ProcessUtility::termProcess(previousChildProcessId);
			LOG_INFO(
				"ProcessUtility::termProcess"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", label: {}"
				", message: {}"
				", previousChildPid: {}"
				", selectedEncoding->_childProcessId: {}"
				", kill: {}"
				", counter: {}",
				ingestionJobKey, selectedEncoding->_encodingJobKey, label, message, previousChildProcessId.toString(),
				selectedEncoding->_childProcessId.toString(), kill, counter++
			);
			this_thread::sleep_for(chrono::seconds(1));
			// ripete il loop se la condizione è true
		} while (selectedEncoding->_childProcessId == previousChildProcessId && chrono::system_clock::now() - start <= chrono::seconds(secondsToWait)
		);
	}
	catch (exception& e)
	{
		LOG_ERROR(
			"termProcess failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", label: {}"
			", message: {}"
			", kill: {}"
			", exception: {}",
			ingestionJobKey, selectedEncoding->_encodingJobKey, label, message, kill, e.what()
		);

		throw;
	}
}
