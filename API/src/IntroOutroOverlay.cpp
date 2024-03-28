
#include "IntroOutroOverlay.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void IntroOutroOverlay::encodeContent(
	json metadataRoot)
{
    string api = "introOutroOverlay";

	_logger->info(__FILEREF__ + "Received " + api
		+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		+ ", requestBody: " + JSONUtils::toString(metadataRoot)
	);

    try
    {
        // json metadataRoot = JSONUtils::toJson(
		// 	-1, _encodingJobKey, requestBody);

		// int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);                 
		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);                  
		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];                       
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];                       

		json encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		string introSourceFileExtension;
		{
			string field = "introSourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			introSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string mainSourceFileExtension;
		{
			string field = "mainSourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mainSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string outroSourceFileExtension;
		{
			string field = "outroSourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			outroSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string introSourceAssetPathName;
		string mainSourceAssetPathName;
		string outroSourceAssetPathName;
		string encodedStagingAssetPathName;

		if (externalEncoder)
		{
			{
				string field = "introSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				introSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = introSourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = introSourceAssetPathName.substr(
							0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "Creating directory"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", directoryPathName: " + directoryPathName
						);
						fs::create_directories(directoryPathName);
						fs::permissions(directoryPathName,
							fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
							| fs::perms::group_read | fs::perms::group_exec
							| fs::perms::others_read | fs::perms::others_exec,
							fs::perm_options::replace);
					}
				}

				field = "introSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string introSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				introSourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					introSourceFileExtension,
					introSourcePhysicalDeliveryURL,
					introSourceAssetPathName);
			}

			{
				string field = "mainSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				mainSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = mainSourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = mainSourceAssetPathName.substr(
							0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "Creating directory"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", directoryPathName: " + directoryPathName
						);
						fs::create_directories(directoryPathName);
						fs::permissions(directoryPathName,
							fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
							| fs::perms::group_read | fs::perms::group_exec
							| fs::perms::others_read | fs::perms::others_exec,
							fs::perm_options::replace);
					}
				}

				field = "mainSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string mainSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				mainSourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					mainSourceFileExtension,
					mainSourcePhysicalDeliveryURL,
					mainSourceAssetPathName);
			}

			{
				string field = "outroSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				outroSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = outroSourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = outroSourceAssetPathName.substr(
							0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "Creating directory"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", directoryPathName: " + directoryPathName
						);
						fs::create_directories(directoryPathName);
						fs::permissions(directoryPathName,
							fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
							| fs::perms::group_read | fs::perms::group_exec
							| fs::perms::others_read | fs::perms::others_exec,
							fs::perm_options::replace);
					}
				}

				field = "outroSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string outroSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				outroSourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					outroSourceFileExtension,
					outroSourcePhysicalDeliveryURL,
					outroSourceAssetPathName);
			}

			string field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(
						0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "Creating directory"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", directoryPathName: " + directoryPathName
					);
					fs::create_directories(directoryPathName);
					fs::permissions(directoryPathName,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
						| fs::perms::group_read | fs::perms::group_exec
						| fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace);
				}
			}
		}
		else
		{
			string field = "introSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			introSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "mainSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mainSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "outroSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			outroSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "encodedNFSStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		int64_t introSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"introSourceDurationInMilliSeconds", -1);                 
		int64_t mainSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"mainSourceDurationInMilliSeconds", -1);                 
		int64_t outroSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"outroSourceDurationInMilliSeconds", -1);                 

		int introOverlayDurationInSeconds = JSONUtils::asInt(ingestedParametersRoot,
			"introOverlayDurationInSeconds", -1);                 
		int outroOverlayDurationInSeconds = JSONUtils::asInt(ingestedParametersRoot,
			"outroOverlayDurationInSeconds", -1);                 
		bool muteIntroOverlay = JSONUtils::asInt(ingestedParametersRoot, "muteIntroOverlay", true);                 
		bool muteOutroOverlay = JSONUtils::asInt(ingestedParametersRoot, "muteOutroOverlay", true);                 

		// 2023-06-01: ho notato che il comando ffmpeg "accumula" lip-sync ed il problema è
		//	evidente con video di durata >= 10min. Dipende anche tanto dalla codifica del sorgente,
		//	un sorgente "non compresso" ritarda il problema del lip-sync
		// Per questo motivo, se la durata del sorgente è abbastanza lunga (vedi controllo sotto), dividiamo il sorgente in tre parti:
		//	la prima e l'ultima di 60 secondi e quella centrale rimanente
		// In questo modo applichiamo intro alla prima parte, l'outro all'ultima parte,
		// la parte centrale la codifichiamo utilizzando lo stesso profilo e poi concateniamo
		// le tre parti risultanti
		int introOutroDurationInSeconds = 60;
		if (mainSourceDurationInMilliSeconds >=
			(introOverlayDurationInSeconds + introOutroDurationInSeconds
			+ outroOverlayDurationInSeconds + introOutroDurationInSeconds) * 1000)
		{
			/*
				E' necessario che l'ultimo chunk sia sufficientemente lungo per
				poter fare l'outroOverlay.
				Infatti ho visto che, nel caso di un video di 600 secondi, avendo un periodo del chunk
				di 60 secondi, l'ultimo chunk è capitato di pochi secondi e l'outro overlay è fallito.

				Per evitare questo problema, dobbiamo assicurarci che il periodo sia tale che
					"durata main video" modulo "periodo" sia il piu vicino possibile alla metà del periodo
				Questo perchè la durata dei chunks sarà intorno al Periodo, un po piu e un po meno.
				Per ogni chunk ci sarà una piccola perdita (milliseconds) in negativo o in positivo.
				I nostri calcoli mirano ad avere un ultimo chunk con durata il piu vicino possibile alla metà del periodo
				cosi' da minimizzare la probabilità che l'ultimo chunk non abbia durata sufficiente per
				fare l'outro overlay.
			*/
			// il prossimo algoritmo cerca di massimizzare il modulo
			int selectedChunkPeriodInSeconds;
			long selectedDistanceFromHalf = -1;
			{
				int candidateChunkPeriodInSeconds = introOutroDurationInSeconds;
				for(int index = 0; index < 10; index++)
				{
					long mod = mainSourceDurationInMilliSeconds % (candidateChunkPeriodInSeconds * 1000);
					if (selectedDistanceFromHalf == -1)
					{
						selectedDistanceFromHalf = abs(mod - ((candidateChunkPeriodInSeconds * 1000) / 2));
						selectedChunkPeriodInSeconds = candidateChunkPeriodInSeconds;
					}
					else
					{
						int64_t currentDistanceFromHalf = abs(mod - ((candidateChunkPeriodInSeconds * 1000) / 2));
						if (currentDistanceFromHalf < selectedDistanceFromHalf)
						{
							_logger->info(__FILEREF__ + "selectedChunkPeriodInSeconds, changing period"
								+ ", ingestionJobKey: " + to_string(_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(_encodingJobKey)
								+ ", prev selectedChunkPeriodInSeconds: " + to_string(selectedChunkPeriodInSeconds)
								+ ", new selectedChunkPeriodInSeconds: " + to_string(candidateChunkPeriodInSeconds)
								+ ", prev selectedDistanceFromHalf: " + to_string(selectedDistanceFromHalf)
								+ ", new selectedDistanceFromHalf: " + to_string(currentDistanceFromHalf)
							);

							selectedDistanceFromHalf = currentDistanceFromHalf;
							selectedChunkPeriodInSeconds = candidateChunkPeriodInSeconds;
						}
					}

					candidateChunkPeriodInSeconds++;
				}
			}
			_logger->info(__FILEREF__ + "selectedChunkPeriodInSeconds"
				+ ", ingestionJobKey: " + to_string(_ingestionJobKey)
				+ ", encodingJobKey: " + to_string(_encodingJobKey)
				+ ", mainSourceDurationInMilliSeconds: " + to_string(mainSourceDurationInMilliSeconds)
				+ ", selectedChunkPeriodInSeconds: " + to_string(selectedChunkPeriodInSeconds)
				+ ", selectedDistanceFromHalf: " + to_string(selectedDistanceFromHalf)
			);

			// implementazione che utilizza lo split del video
			string stagingBasePath;

			// ci serve un path dello staging locale al transcoder.
			// Utilizziamo solo il path di encodedTranscoderStagingAssetPathName che 
			// sarebbe il path locale dell'asset path name in caso di transcoder remoto
			// In questa directory creiamo una sottodirectory perchè avremo tanti files
			string field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string encodedTranscoderStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field);

			size_t endOfDirectoryIndex = encodedTranscoderStagingAssetPathName.find_last_of("/");
			if (endOfDirectoryIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "encodedTranscoderStagingAssetPathName is not well formed"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", encodedTranscoderStagingAssetPathName: " + encodedTranscoderStagingAssetPathName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			stagingBasePath = encodedTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);
			stagingBasePath += ("/introOutroSplit_" + to_string(_ingestionJobKey) + "_" + to_string(_encodingJobKey));

			// aggiunto try/catch qui perchè in caso di eccezione bisogna eliminare la dir stagingBasePath
			try
			{
				string chunkBaseFileName = "sourceChunk";
				_encoding->_ffmpeg->splitVideoInChunks(
					_ingestionJobKey,
					mainSourceAssetPathName,
					selectedChunkPeriodInSeconds,
					stagingBasePath,
					chunkBaseFileName);

				string destFileFormat = JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");

				vector<string> concatSourcePhysicalPaths;

				bool filesAreFinished = false;
				int currentFileIndex = 0;
				while(!filesAreFinished)
				{
					char currentCounter [64];
					sprintf (currentCounter, "%04d", currentFileIndex);
					string currentFile = stagingBasePath + "/" + chunkBaseFileName
						+ "_" + currentCounter + mainSourceFileExtension;

					char nextCounter [64];
					sprintf (nextCounter, "%04d", currentFileIndex + 1);
					string nextFile = stagingBasePath + "/" + chunkBaseFileName
						+ "_" + nextCounter + mainSourceFileExtension;

					// il file sorgente è piu lungo di 2 volte il periodo (introOutroDurationInSeconds)
					// Per cui sappiamo sicuramente che abbiamo almeno due chunks
					if (currentFileIndex == 0)
					{
						// primo file
						if (fs::exists(currentFile))
						{
							int64_t currentFileDurationInMilliSeconds;
							{
								vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
								vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
								tuple<int64_t, long, json> mediaInfo = _encoding->_ffmpeg->getMediaInfo(
									_ingestionJobKey,
									true,	// isMMSAssetPathName
									-1,		// timeoutInSeconds,		// used only in case of URL
									currentFile,
									videoTracks,
									audioTracks);
								tie(currentFileDurationInMilliSeconds, ignore, ignore) = mediaInfo;
							}
							string introPathName = stagingBasePath + "/" + "destChunk"
								+ "_" + currentCounter + "." + destFileFormat;
							concatSourcePhysicalPaths.push_back(introPathName);
							_encoding->_ffmpeg->introOverlay(
								introSourceAssetPathName, introSourceDurationInMilliSeconds,
								currentFile, currentFileDurationInMilliSeconds,

								introOverlayDurationInSeconds,
								muteIntroOverlay,

								encodingProfileDetailsRoot,

								introPathName,

								_encodingJobKey,
								_ingestionJobKey,
								&(_encoding->_childPid));
						}
						else
						{
							string errorMessage = __FILEREF__ + "chunk file is not present"
								+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", currentFile: " + currentFile;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (!fs::exists(nextFile))
					{
						// ultimo file
						if (fs::exists(currentFile))
						{
							int64_t currentFileDurationInMilliSeconds;
							{
								vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
								vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
								tuple<int64_t, long, json> mediaInfo = _encoding->_ffmpeg->getMediaInfo(
									_ingestionJobKey,
									true,	// isMMSAssetPathName
									-1,		// timeoutInSeconds,		// used only in case of URL
									currentFile,
									videoTracks,
									audioTracks);
								tie(currentFileDurationInMilliSeconds, ignore, ignore) = mediaInfo;
							}
							string outroPathName = stagingBasePath + "/" + "destChunk"
								+ "_" + currentCounter + "." + destFileFormat;
							concatSourcePhysicalPaths.push_back(outroPathName);
							_encoding->_ffmpeg->outroOverlay(
								currentFile, currentFileDurationInMilliSeconds,
								outroSourceAssetPathName, outroSourceDurationInMilliSeconds,

								outroOverlayDurationInSeconds,
								muteOutroOverlay,

								encodingProfileDetailsRoot,

								outroPathName,

								_encodingJobKey,
								_ingestionJobKey,
								&(_encoding->_childPid));
						}
						else
						{
							string errorMessage = __FILEREF__ + "chunk file is not present"
								+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", currentFile: " + currentFile;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						filesAreFinished = true;
					}
					else
					{
						// file intermedio
						if (fs::exists(currentFile))
						{
							int64_t currentFileDurationInMilliSeconds;
							{
								vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
								vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
								tuple<int64_t, long, json> mediaInfo = _encoding->_ffmpeg->getMediaInfo(
									_ingestionJobKey,
									true,	// isMMSAssetPathName
									-1,		// timeoutInSeconds,		// used only in case of URL
									currentFile,
									videoTracks,
									audioTracks);
								tie(currentFileDurationInMilliSeconds, ignore, ignore) = mediaInfo;
							}
							string encodedPathName = stagingBasePath + "/" + "destChunk"
								+ "_" + currentCounter + "." + destFileFormat;
							concatSourcePhysicalPaths.push_back(encodedPathName);
							_encoding->_ffmpeg->encodeContent(
								currentFile, currentFileDurationInMilliSeconds,
								encodedPathName,
								encodingProfileDetailsRoot,
								true,
								nullptr, nullptr,
								-1, -1,
								nullptr,
								-1, _encodingJobKey, _ingestionJobKey,
								&(_encoding->_childPid));
						}
						else
						{
							string errorMessage = __FILEREF__ + "chunk file is not present"
								+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", currentFile: " + currentFile;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}

					currentFileIndex++;
				}

				_encoding->_ffmpeg->concat(
					_ingestionJobKey,
					true,
					concatSourcePhysicalPaths,
					encodedStagingAssetPathName);

				_logger->info(__FILEREF__ + "removing temporary directory"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", stagingBasePath: " + stagingBasePath
				);
				fs::remove_all(stagingBasePath);
			}
			catch(runtime_error& e)
			{
				_logger->error(__FILEREF__ + "Intro outro procedure failed"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", e.what(): " + e.what()
				);

				if (fs::exists(stagingBasePath))
				{
					_logger->info(__FILEREF__ + "removing temporary directory"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", stagingBasePath: " + stagingBasePath
					);
					fs::remove_all(stagingBasePath);
				}

				throw e;
			}

			/* implementazione che divide il video in tre parti tramite cutWithoutEncoding su keyframes
			string stagingBasePath;

			// ci serve un path dello staging locale al transcoder.
			// Utilizziamo solo il path di encodedTranscoderStagingAssetPathName che 
			// sarebbe il path locale dell'asset path name in caso di transcoder remoto
			string field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string encodedTranscoderStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field);

			size_t endOfDirectoryIndex = encodedTranscoderStagingAssetPathName.find_last_of("/");
			if (endOfDirectoryIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "encodedTranscoderStagingAssetPathName is not well formed"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", encodedTranscoderStagingAssetPathName: " + encodedTranscoderStagingAssetPathName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			stagingBasePath = encodedTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);
			if (!fs::exists(stagingBasePath))
			{
				_logger->info(__FILEREF__ + "Creating directory"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", stagingBasePath: " + stagingBasePath
				);
				fs::create_directories(stagingBasePath);
				fs::permissions(stagingBasePath,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
					| fs::perms::group_read | fs::perms::group_exec
					| fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace);
			}

			stagingBasePath += ("/introOutroSplit_"
				+ to_string(_ingestionJobKey) + "_" + to_string(_encodingJobKey) + "_");

			string mainBeginPathName = stagingBasePath + "mainBegin" + mainSourceFileExtension;
			if (fs::exists(mainBeginPathName))
				fs::remove_all(mainBeginPathName);
			double startTimeInSeconds = 0.0;
			double endTimeInSeconds = introOverlayDurationInSeconds + introOutroDurationInSeconds;
			int64_t mainBeginDurationInMilliSeconds = (introOverlayDurationInSeconds + introOutroDurationInSeconds) * 1000;
			{
				// 537%+8, cerco nell'intervallo a partire da (startTimeInSeconds - 4) fino a +8 secondi
				// Sto assumendo che nel file sorgente, ci sia un KeyFrame al max ogni 4 second1
				long beginOfInterval = startTimeInSeconds - 4 < 0 ? 0 : startTimeInSeconds - 4;
				string startKeyFramesSeekingInterval = to_string(beginOfInterval) + "%+8";
				beginOfInterval = endTimeInSeconds - 4 < 0 ? 0 : endTimeInSeconds - 4;
				string endKeyFramesSeekingInterval = to_string(beginOfInterval) + "%+8";
				_encoding->_ffmpeg->cutWithoutEncoding(
					_ingestionJobKey,
					mainSourceAssetPathName,
					true,	// isVideo
					"KeyFrameSeekingInterval",
					startKeyFramesSeekingInterval,
					endKeyFramesSeekingInterval,
					to_string(startTimeInSeconds),
					to_string(endTimeInSeconds),
					-1,	// framesNumber
					mainBeginPathName);
			}

			string mainEndPathName = stagingBasePath + "mainEnd" + mainSourceFileExtension;
			if (fs::exists(mainEndPathName))
				fs::remove_all(mainEndPathName);
			startTimeInSeconds = (mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds);
			endTimeInSeconds = mainSourceDurationInMilliSeconds / 1000;
			int64_t mainEndDurationInMilliSeconds =
				(
					(mainSourceDurationInMilliSeconds / 1000)
					- ((mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds))
				) * 1000;
			{
				// 537%+8, cerco nell'intervallo a partire da (startTimeInSeconds - 4) fino a +8 secondi
				// Sto assumendo che nel file sorgente, ci sia un KeyFrame al max ogni 4 second1
				long beginOfInterval = startTimeInSeconds - 4 < 0 ? 0 : startTimeInSeconds - 4;
				string startKeyFramesSeekingInterval = to_string(beginOfInterval) + "%+8";
				beginOfInterval = endTimeInSeconds - 4 < 0 ? 0 : endTimeInSeconds - 4;
				string endKeyFramesSeekingInterval = to_string(beginOfInterval) + "%+8";
				_encoding->_ffmpeg->cutWithoutEncoding(
					_ingestionJobKey,
					mainSourceAssetPathName,
					true,	// isVideo
					"KeyFrameSeekingInterval",
					startKeyFramesSeekingInterval,
					endKeyFramesSeekingInterval,
					to_string(startTimeInSeconds),
					to_string(endTimeInSeconds),
					-1,		// framesNumber
					mainEndPathName);
			}

			string mainCenterPathName = stagingBasePath + "mainCenter" + mainSourceFileExtension;
			if (fs::exists(mainCenterPathName))
				fs::remove_all(mainCenterPathName);
			startTimeInSeconds = introOverlayDurationInSeconds + introOutroDurationInSeconds;
			endTimeInSeconds = (mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds);
			int64_t mainCenterDurationInMilliSeconds =
				(
					((mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds))
					- (introOverlayDurationInSeconds + introOutroDurationInSeconds)
				) * 1000;
			{
				// 537%+8, cerco nell'intervallo a partire da (startTimeInSeconds - 4) fino a +8 secondi
				// Sto assumendo che nel file sorgente, ci sia un KeyFrame al max ogni 4 second1
				long beginOfInterval = startTimeInSeconds - 4 < 0 ? 0 : startTimeInSeconds - 4;
				string startKeyFramesSeekingInterval = to_string(beginOfInterval) + "%+8";
				beginOfInterval = endTimeInSeconds - 4 < 0 ? 0 : endTimeInSeconds - 4;
				string endKeyFramesSeekingInterval = to_string(beginOfInterval) + "%+8";
				_encoding->_ffmpeg->cutWithoutEncoding(
					_ingestionJobKey,
					mainSourceAssetPathName,
					true,	// isVideo
					"KeyFrameSeekingInterval",
					startKeyFramesSeekingInterval,
					endKeyFramesSeekingInterval,
					to_string(startTimeInSeconds),
					to_string(endTimeInSeconds),
					-1,		// framesNumber
					mainCenterPathName);
			}

			string destFileFormat = JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");

			string mainIntroPathName = stagingBasePath + "mainIntro." + destFileFormat;
			if (fs::exists(mainIntroPathName))
				fs::remove_all(mainIntroPathName);
			_encoding->_ffmpeg->introOverlay(
				introSourceAssetPathName, introSourceDurationInMilliSeconds,
				mainBeginPathName, mainBeginDurationInMilliSeconds,

				introOverlayDurationInSeconds,
				muteIntroOverlay,

				encodingProfileDetailsRoot,

				mainIntroPathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));

			string mainOutroPathName = stagingBasePath + "mainOutro." + destFileFormat;
			if (fs::exists(mainOutroPathName))
				fs::remove_all(mainOutroPathName);
			_encoding->_ffmpeg->outroOverlay(
				mainEndPathName, mainEndDurationInMilliSeconds,
				outroSourceAssetPathName, outroSourceDurationInMilliSeconds,

				outroOverlayDurationInSeconds,
				muteOutroOverlay,

				encodingProfileDetailsRoot,

				mainOutroPathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));

			string mainCenterEncodedPathName = stagingBasePath + "mainCenterEncoded." + destFileFormat;
			if (fs::exists(mainCenterEncodedPathName))
				fs::remove_all(mainCenterEncodedPathName);
			_encoding->_ffmpeg->encodeContent(
				mainCenterPathName, mainCenterDurationInMilliSeconds,
				mainCenterEncodedPathName,
				encodingProfileDetailsRoot,
				true,
				nullptr, nullptr,
				-1, -1,
				-1, _encodingJobKey, _ingestionJobKey,
				&(_encoding->_childPid));


			vector<string> sourcePhysicalPaths;
			sourcePhysicalPaths.push_back(mainIntroPathName);
			sourcePhysicalPaths.push_back(mainCenterEncodedPathName);
			sourcePhysicalPaths.push_back(mainOutroPathName);
			_encoding->_ffmpeg->concat(
				_ingestionJobKey,
				true,
				sourcePhysicalPaths,
				encodedStagingAssetPathName);

			_logger->info(__FILEREF__ + "removing temporary files"
				+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
				+ ", mainBeginPathName: " + mainBeginPathName
				+ ", mainEndPathName: " + mainEndPathName
				+ ", mainCenterPathName: " + mainCenterPathName
				+ ", mainIntroPathName: " + mainIntroPathName
				+ ", mainOutroPathName: " + mainOutroPathName
				+ ", mainCenterEncodedPathName: " + mainCenterEncodedPathName
			);
			fs::remove_all(mainBeginPathName);
			fs::remove_all(mainEndPathName);
			fs::remove_all(mainCenterPathName);
			fs::remove_all(mainIntroPathName);
			fs::remove_all(mainOutroPathName);
			fs::remove_all(mainCenterEncodedPathName);
			*/
		}
		else
		{
			_encoding->_ffmpeg->introOutroOverlay(
				introSourceAssetPathName, introSourceDurationInMilliSeconds,
				mainSourceAssetPathName, mainSourceDurationInMilliSeconds,
				outroSourceAssetPathName, outroSourceDurationInMilliSeconds,

				introOverlayDurationInSeconds, outroOverlayDurationInSeconds,
				muteIntroOverlay, muteOutroOverlay,

				encodingProfileDetailsRoot,

				encodedStagingAssetPathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));
		}

		_encoding->_ffmpegTerminatedSuccessful = true;

        _logger->info(__FILEREF__ + "introOutroOverlay encoding content finished"
            + ", _ingestionJobKey: " + to_string(_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
        );

		if (externalEncoder)
		{
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", introSourceAssetPathName: " + introSourceAssetPathName
				);

				fs::remove_all(introSourceAssetPathName);
			}

			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", mainSourceAssetPathName: " + mainSourceAssetPathName
				);

				fs::remove_all(mainSourceAssetPathName);
			}

			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", outroSourceAssetPathName: " + outroSourceAssetPathName
				);

				fs::remove_all(outroSourceAssetPathName);
			}

			string workflowLabel =
				JSONUtils::asString(ingestedParametersRoot, "title", "")
				+ " (add introOutroOverlay from external transcoder)"
			;

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot,
				"encodingProfileKey", -1);

			// string encodingProfileFileFormat =
			// 	JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");

			uploadLocalMediaToMMS(
				_ingestionJobKey,
				_encodingJobKey,
				ingestedParametersRoot,
				encodingProfileDetailsRoot,
				encodingParametersRoot,
				mainSourceFileExtension,
				encodedStagingAssetPathName,
				workflowLabel,
				"External Transcoder",	// ingester
				// 2023-04-21: Il file generato da introOutroOverlay ha un payload generato dall'encodingProfileKey,
				//	pero', nello stesso tempo, ha un fileFormat dato dal mainSource (vedi come viene costruito
				//	l'encodedFileName in MMSEngineService.cpp).
				//	Per cui possiamo indicare encodingProfileKey quando il file encodato viene aggiunto in MMS
				//	solo se mainSourceFileExtension coincide con il fileFormat dell'encodingProfileKey
				//	fileFormat è ad esempio "mp4", mainSourceFileExtension è per esempio ".ts"
				// 2023-04-22: Penso che l'encodingProfileKey debba essere considerato indipendentemente
				//	se il fileFormat sia uguale o diverso
				encodingProfileKey
			);
		}
    }
	catch(FFMpegEncodingKilledByUser& e)
	{
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + JSONUtils::toString(metadataRoot)
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_killedByUser			= true;

		throw e;
    }
    catch(runtime_error& e)
    {
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + JSONUtils::toString(metadataRoot)
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
    catch(exception& e)
    {
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + JSONUtils::toString(metadataRoot)
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
}
