
#include <fstream>
#include "MMSStorage.h"
#include "JSONUtils.h"
#include "catralibraries/System.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/ProcessUtility.h"

MMSStorage::MMSStorage(
	bool noFileSystemAccess,
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	Json::Value configuration,
	shared_ptr<spdlog::logger> logger) 
{

	try
	{
		_noFileSystemAccess	= noFileSystemAccess;
		_mmsEngineDBFacade	= mmsEngineDBFacade;
		_logger             = logger;
		_configuration		= configuration;

		_hostName = System::getHostName();

		_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(configuration["storage"],
			"waitingNFSSync_maxMillisecondsToWait", 60000);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", storage->_waitingNFSSync_maxMillisecondsToWait: " + to_string(_waitingNFSSync_maxMillisecondsToWait)
		);

		_storage = JSONUtils::asString(configuration["storage"], "path", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", storage->path: " + _storage.string()
		);

		_freeSpaceToLeaveInEachPartitionInMB = JSONUtils::asInt(configuration["storage"], "freeSpaceToLeaveInEachPartitionInMB", 100);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", storage->freeSpaceToLeaveInEachPartitionInMB: " + to_string(_freeSpaceToLeaveInEachPartitionInMB)
		);

		if (!_noFileSystemAccess)
		{
			MMSStorage::createDirectories(configuration, _logger);

			refreshPartitionsFreeSizes() ;
		}
	}
	catch(runtime_error& e)
	{
		_logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		_logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
}

MMSStorage::~MMSStorage(void) {
}

void MMSStorage::createDirectories(
	Json::Value configuration,
	shared_ptr<spdlog::logger> logger) 
{

	/* 2022-12-22: controllo non aggiunto perchè è un metodo static
		E' il chiamante che si deve assicurare che ci sia accesso al file system
	if (noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", noFileSystemAccess: " + to_string(noFileSystemAccess)
		;
		logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	*/

	fs::path storage = JSONUtils::asString(configuration["storage"], "path", "");
	logger->info(__FILEREF__ + "Configuration item"
		+ ", storage->path: " + storage.string()
	);

	// 2023-02-13: scenario: fs::permissions è fallito, genera un eccezione e la creazione delle
	//	successive directory è fallita.
	//	Per evitare questo si aggiungono i try/catch in modo che tutte le directory siano create
	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", ingestionRootRepository: " + MMSStorage::getIngestionRootRepository(storage).string()
		);
		fs::create_directories(MMSStorage::getIngestionRootRepository(storage));
		fs::permissions(MMSStorage::getIngestionRootRepository(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}

	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", mmsRootRepository: " + MMSStorage::getMMSRootRepository(storage).string()
		);
		fs::create_directories(MMSStorage::getMMSRootRepository(storage));
		fs::permissions(MMSStorage::getMMSRootRepository(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", mmsRootRepository: " + MMSStorage::getMMSRootRepository(storage).string()
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", mmsRootRepository: " + MMSStorage::getMMSRootRepository(storage).string()
			+ ", e.what(): " + e.what()
		);
	}

	fs::path MMS_0000Path = MMSStorage::getMMSRootRepository(storage) / "MMS_0000";
	try
	{
		// create MMS_0000 in case it does not exist (first running of MMS)
		{
			logger->info(__FILEREF__ + "Creating directory (if needed)"
				+ ", MMS_0000 Path: " + MMS_0000Path.string()
			);
			fs::create_directories(MMS_0000Path);
			fs::permissions(MMS_0000Path,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
				| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
				| fs::perms::others_exec,
				fs::perm_options::replace);
		}
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", MMS_0000 Path: " + MMS_0000Path.string()
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", MMS_0000 Path: " + MMS_0000Path.string()
			+ ", e.what(): " + e.what()
		);
	}

	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", stagingRootRepository: " + MMSStorage::getStagingRootRepository(storage).string()
		);
		fs::create_directories(MMSStorage::getStagingRootRepository(storage));
		fs::permissions(MMSStorage::getStagingRootRepository(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}

	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", transcoderStagingRootRepository: " + MMSStorage::getTranscoderStagingRootRepository(storage).string()
		);
		fs::create_directories(MMSStorage::getTranscoderStagingRootRepository(storage));
		fs::permissions(MMSStorage::getTranscoderStagingRootRepository(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}

	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", liveRootRepository: " + MMSStorage::getLiveRootRepository(storage).string()
		);
		fs::create_directories(MMSStorage::getLiveRootRepository(storage));
		fs::permissions(MMSStorage::getLiveRootRepository(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}

	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", ffmpegArea: " + getFFMPEGArea(storage).string()
		);
		fs::create_directories(MMSStorage::getFFMPEGArea(storage));
		fs::permissions(MMSStorage::getFFMPEGArea(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}

	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", ffmpegEndlessRecursivePlaylistArea: "
				+ getFFMPEGEndlessRecursivePlaylistArea(storage).string()
		);
		fs::create_directories(MMSStorage::getFFMPEGEndlessRecursivePlaylistArea(storage));
		fs::permissions(MMSStorage::getFFMPEGEndlessRecursivePlaylistArea(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}

	try
	{
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", nginxArea: " + getNginxArea(storage).string()
		);
		fs::create_directories(MMSStorage::getNginxArea(storage));
		fs::permissions(MMSStorage::getNginxArea(storage),
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec 
			| fs::perms::others_read | fs::perms::others_write | fs::perms::others_exec,
			fs::perm_options::replace);
	}
	catch(runtime_error& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception& e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
}

fs::path MMSStorage::getMMSRootRepository(fs::path storage) {
    return storage / "MMSRepository";
}

fs::path MMSStorage::getMMSRootRepository() {
    return MMSStorage::getMMSRootRepository(_storage);
}

fs::path MMSStorage::getIngestionRootRepository(fs::path storage) {
    return storage / "IngestionRepository" / "users";
}

tuple<int64_t, fs::path, int, string, string, int64_t, string>
	MMSStorage::getPhysicalPathDetails(
		int64_t mediaItemKey, int64_t encodingProfileKey,
		bool warningIfMissing, bool fromMaster)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
			string, string, string, string, int64_t, bool> storageDetails =
				_mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey,
					warningIfMissing, fromMaster);

		int64_t physicalPathKey;
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		int mmsPartitionNumber;
		shared_ptr<Workspace> workspace;
		string relativePath;
		string fileName;
		int64_t sizeInBytes;
		string deliveryFileName;
		string title;
		bool externalReadOnlyStorage;
		tie(physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath, 
			fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage)
			= storageDetails;

		_logger->info(__FILEREF__ + "getMMSAssetPathName ..."
			+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
			+ ", workspaceDirectoryName: " + workspace->_directoryName
			+ ", relativePath: " + relativePath
			+ ", fileName: " + fileName
		);
		fs::path physicalPath = getMMSAssetPathName(
			externalReadOnlyStorage,
			mmsPartitionNumber,
			workspace->_directoryName,
			relativePath,
			fileName);
    
		return make_tuple(physicalPathKey, physicalPath, mmsPartitionNumber,
				relativePath, fileName, sizeInBytes, deliveryFileName);
    }
    catch(MediaItemKeyNotFound& e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
			+ ", e.what(): " + e.what()
        ;
		if (warningIfMissing)
			_logger->warn(__FILEREF__ + errorMessage);
		else
			_logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(runtime_error& e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception& e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

tuple<fs::path, int, string, string, int64_t, string> MMSStorage::getPhysicalPathDetails(
	int64_t physicalPathKey, bool fromMaster)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
			string, string, string, string, int64_t, bool> storageDetails =
			_mmsEngineDBFacade->getStorageDetails(physicalPathKey, fromMaster);

		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		int mmsPartitionNumber;
		shared_ptr<Workspace> workspace;
		string relativePath;
		string fileName;
		string deliveryFileName;
		string title;
		int64_t sizeInBytes;
		bool externalReadOnlyStorage;
		tie(ignore, deliveryTechnology, mmsPartitionNumber, workspace, relativePath, fileName, 
            deliveryFileName, title, sizeInBytes, externalReadOnlyStorage) = storageDetails;

		_logger->info(__FILEREF__ + "getMMSAssetPathName ..."
			+ ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage)
			+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
			+ ", workspaceDirectoryName: " + workspace->_directoryName
			+ ", relativePath: " + relativePath
			+ ", fileName: " + fileName
		);
		fs::path physicalPath = getMMSAssetPathName(
			externalReadOnlyStorage,
			mmsPartitionNumber,
			workspace->_directoryName,
			relativePath,
			fileName);

		return make_tuple(physicalPath, mmsPartitionNumber, relativePath,
			fileName, sizeInBytes, deliveryFileName);
    }
    catch(MediaItemKeyNotFound& e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(runtime_error& e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception& e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

tuple<string, int, string, string> MMSStorage::getVODDeliveryURI(
	int64_t physicalPathKey, bool save, shared_ptr<Workspace> requestWorkspace)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
			string, string, string, string, int64_t, bool> storageDetails =
			_mmsEngineDBFacade->getStorageDetails(physicalPathKey, false /* fromMaster */);

		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		int mmsPartitionNumber;
		shared_ptr<Workspace> contentWorkspace;
		string relativePath;
		string fileName;
		string deliveryFileName;
		string title;
		bool externalReadOnlyStorage;
		tie(ignore, deliveryTechnology, mmsPartitionNumber, contentWorkspace, relativePath,
			fileName, deliveryFileName, title, ignore, externalReadOnlyStorage)
			= storageDetails;

		if (save)
		{
			if (deliveryFileName == "")
				deliveryFileName = title;

			if (deliveryFileName != "")
			{
				// use the extension of fileName
				size_t extensionIndex = fileName.find_last_of(".");
				if (extensionIndex != string::npos)
					deliveryFileName.append(fileName.substr(extensionIndex));
			}
		}

		if (contentWorkspace->_workspaceKey != requestWorkspace->_workspaceKey)
		{
			string errorMessage =
				string ("Workspace of the content and Workspace of the requester is different")
				+ ", contentWorkspace->_workspaceKey: " + to_string(contentWorkspace->_workspaceKey)
				+ ", requestWorkspace->_workspaceKey: " + to_string(requestWorkspace->_workspaceKey)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string deliveryURI;
		if (externalReadOnlyStorage)
		{
			deliveryURI =
				string("/ExternalStorage_")
				+ contentWorkspace->_directoryName
				+ relativePath
				+ fileName;
		}
		else
		{
			char pMMSPartitionName [64];


			sprintf(pMMSPartitionName, "/MMS_%04d/", mmsPartitionNumber);

			deliveryURI =
				pMMSPartitionName
				+ contentWorkspace->_directoryName
				+ relativePath
				+ fileName;
		}

		return make_tuple(title, mmsPartitionNumber, deliveryFileName, deliveryURI);
    }
    catch(MediaItemKeyNotFound& e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(runtime_error& e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception& e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

tuple<string, int, int64_t, string, string> MMSStorage::getVODDeliveryURI(
		int64_t mediaItemKey, int64_t encodingProfileKey, bool save,
		shared_ptr<Workspace> requestWorkspace)
{
	try
	{
		bool warningIfMissing = false;
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int,
			shared_ptr<Workspace>,string,string,string,string,int64_t, bool> storageDetails =
			_mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey,
			warningIfMissing, false /* fromMaster */);

		int64_t physicalPathKey;
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		int mmsPartitionNumber;
		shared_ptr<Workspace> contentWorkspace;
		string relativePath;
		string fileName;
		string deliveryFileName;
		string title;
		bool externalReadOnlyStorage;
		tie(physicalPathKey, deliveryTechnology, mmsPartitionNumber, contentWorkspace,
				relativePath, fileName, deliveryFileName, title, ignore,
				externalReadOnlyStorage) = storageDetails;

		if (save)
		{
			if (deliveryFileName == "")
				deliveryFileName = title;

			if (deliveryFileName != "")
			{
				// use the extension of fileName
				size_t extensionIndex = fileName.find_last_of(".");
				if (extensionIndex != string::npos)
					deliveryFileName.append(fileName.substr(extensionIndex));
			}
		}

		if (contentWorkspace->_workspaceKey != requestWorkspace->_workspaceKey)
		{
			string errorMessage =
				string ("Workspace of the content and Workspace of the requester is different")
				+ ", contentWorkspace->_workspaceKey: " + to_string(contentWorkspace->_workspaceKey)
				+ ", requestWorkspace->_workspaceKey: " + to_string(requestWorkspace->_workspaceKey)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string deliveryURI;
		{
			char pMMSPartitionName [64];


			sprintf(pMMSPartitionName, "/MMS_%04d/", mmsPartitionNumber);

			deliveryURI =
				pMMSPartitionName
				+ contentWorkspace->_directoryName
				+ relativePath
				+ fileName;
		}

		return make_tuple(title, mmsPartitionNumber, physicalPathKey,
			deliveryFileName, deliveryURI);
    }
    catch(MediaItemKeyNotFound& e)
    {
		// warn perchè già loggato come error in MMSEngineDBFacade_Postgres.cpp
		SPDLOG_WARN("getDeliveryURI failed"
			", mediaItemKey: {}"
			", encodingProfileKey: {}"
			", e.what(): {}",
			mediaItemKey, encodingProfileKey, e.what());
        
        throw e;
    }
    catch(runtime_error& e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception& e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

fs::path MMSStorage::getLiveDeliveryAssetPathName(
	string directoryId, string liveFileExtension, shared_ptr<Workspace> requestWorkspace)
{
	tuple<fs::path, fs::path, string> liveDeliveryDetails = getLiveDeliveryDetails(
		directoryId, liveFileExtension, requestWorkspace);

	fs::path deliveryPath;
	fs::path deliveryPathName;
	string deliveryFileName;

	tie(deliveryPathName, deliveryPath, deliveryFileName) = liveDeliveryDetails;

	fs::path deliveryAssetPathName = MMSStorage::getMMSRootRepository(_storage)
		/ (deliveryPathName.string().size() > 0 && deliveryPathName.string().front() == '/'
			? deliveryPathName.string().substr(1) : deliveryPathName.string());

	return deliveryAssetPathName;
}

fs::path MMSStorage::getLiveDeliveryAssetPath(
	string directoryId, shared_ptr<Workspace> requestWorkspace)
{
	string liveFileExtension = "xxx";

	tuple<fs::path, fs::path, string> liveDeliveryDetails = getLiveDeliveryDetails(
		directoryId, liveFileExtension, requestWorkspace);

	fs::path deliveryPath;
	fs::path deliveryPathName;
	string deliveryFileName;

	tie(deliveryPathName, deliveryPath, deliveryFileName) = liveDeliveryDetails;

	fs::path deliveryAssetPath = MMSStorage::getMMSRootRepository(_storage)
		/ (deliveryPath.string().size() > 0 && deliveryPath.string().front() == '/'
			? deliveryPath.string().substr(1) : deliveryPath.string());

	return deliveryAssetPath;
}

tuple<fs::path, fs::path, string> MMSStorage::getLiveDeliveryDetails(
		string directoryId, string liveFileExtension,
		shared_ptr<Workspace> requestWorkspace)
{
	fs::path deliveryPath;
	fs::path deliveryPathName;
	string deliveryFileName;

	try
	{
		// if (liveURLType == "LiveProxy")
		{
			deliveryFileName = directoryId + "." + liveFileExtension;

			deliveryPath = "/";
			deliveryPath /= MMSStorage::getDirectoryForLiveContents();
			deliveryPath /= requestWorkspace->_directoryName;
			deliveryPath /= directoryId;

			deliveryPathName = deliveryPath / deliveryFileName;
		}
    }
    catch(runtime_error& e)
    {
        string errorMessage = string("getLiveDeliveryDetails failed")
            + ", directoryId: " + directoryId
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception& e)
    {
        string errorMessage = string("getLiveDeliveryDetails failed")
            + ", directoryId: " + directoryId
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }

	return make_tuple(deliveryPathName, deliveryPath, deliveryFileName);
}

fs::path MMSStorage::getWorkspaceIngestionRepository(shared_ptr<Workspace> workspace)
{
	// 2022-12-22: ho dovuto aggiungere questo controllo (noFileSystemAccess) perchè sotto, se la directory non esiste,
	//	viene creata. Probabilmente questa directory deve essere creata quando viene creato il workspace
	//	e questo metodo non dovrebbe crearla se manca. In questo scenario, il controllo su noFileSystemAccess
	//	non avrebbe piu senso
	if (_noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

    fs::path workspaceIngestionDirectory = MMSStorage::getIngestionRootRepository(_storage);
    workspaceIngestionDirectory /= workspace->_directoryName;
    
    if (!fs::exists(workspaceIngestionDirectory)) 
    {
        _logger->info(__FILEREF__ + "Create directory"
            + ", workspaceIngestionDirectory: " + workspaceIngestionDirectory.string()
        );
		fs::create_directories(workspaceIngestionDirectory);
		fs::permissions(workspaceIngestionDirectory,
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_exec 
			| fs::perms::others_read | fs::perms::others_exec,
			fs::perm_options::replace);
    }

    return workspaceIngestionDirectory;
}

fs::path MMSStorage::getStagingRootRepository(fs::path storage) {
    return storage / "MMSWorkingAreaRepository/Staging";
}

fs::path MMSStorage::getTranscoderStagingRootRepository(fs::path storage) {
    return storage / "MMSTranscoderWorkingAreaRepository/Staging";
}

string MMSStorage::getDirectoryForLiveContents() {
    return "MMSLive";
}

fs::path MMSStorage::getLiveRootRepository(fs::path storage) {
	return storage / "MMSRepository" / MMSStorage::getDirectoryForLiveContents();
}

    
fs::path MMSStorage::getFFMPEGArea(fs::path storage) {
	return storage / "MMSTranscoderWorkingAreaRepository/ffmpeg";
}

fs::path MMSStorage::getFFMPEGEndlessRecursivePlaylistArea(fs::path storage) {
	return storage / "MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist";
}

fs::path MMSStorage::getNginxArea(fs::path storage) {
	return storage / "MMSWorkingAreaRepository/nginx";
}

/*
fs::path MMSStorage::getRepository(RepositoryType rtRepositoryType) 
{

    switch (rtRepositoryType) 
    {
        case RepositoryType::MMSREP_REPOSITORYTYPE_MMSCUSTOMER:
        {
            return MMSStorage::getMMSRootRepository(_storage);
        }
        case RepositoryType::MMSREP_REPOSITORYTYPE_STAGING:
        {
            return MMSStorage::getStagingRootRepository(_storage);
        }
        case RepositoryType::MMSREP_REPOSITORYTYPE_INGESTION:
        {
            return MMSStorage::getIngestionRootRepository(_storage);
        }
        default:
        {
			string errorMessage = string("Wrong argument")                                                      
                    + ", rtRepositoryType: " + to_string(static_cast<int>(rtRepositoryType));

			_logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }
}
*/

fs::path MMSStorage::getMMSAssetPathName(
	bool externalReadOnlyStorage,
	int partitionKey,
	string workspaceDirectoryName,
	string relativePath, // using '/'
	string fileName)
{
	fs::path assetPathName;

	if (externalReadOnlyStorage)
	{
		assetPathName = MMSStorage::getMMSRootRepository(_storage)
			/ ("ExternalStorage_" + workspaceDirectoryName)
			/ (relativePath.size() > 0 && relativePath.front() == '/' ? relativePath.substr(1) : relativePath)
			/ fileName;
	}
	else
	{
		fs::path partitionPathName = _mmsEngineDBFacade->getPartitionPathName(partitionKey);
		assetPathName = partitionPathName / workspaceDirectoryName
		/ (relativePath.size() > 0 && relativePath.front() == '/' ? relativePath.substr(1) : relativePath)
		/ fileName;
	}

    return assetPathName;
}


fs::path MMSStorage::getStagingAssetPathName(
	// neededForTranscoder=true uses a faster file system i.e. for recording
	bool neededForTranscoder,

	string workspaceDirectoryName,

	// it is a prefix of the directory name because I saw two different threads got the same dir name,
	// even if the directory name generated here contains the datetime including millisecs. 
	// Same dir name created a problem when the directory was removed by one thread because 
	// it was still used by the other thread
	string directoryNamePrefix,
	string relativePath,
	string fileName,                // may be empty ("")
	long long llMediaItemKey,       // used only if fileName is ""
	long long llPhysicalPathKey,    // used only if fileName is ""
	bool removeLinuxPathIfExist)
{
    char pUniqueFileName [256];
    string localFileName;
    tm tmDateTime;
    unsigned long ulMilliSecs;
    char pDateTime [64];
    fs::path assetPathName;


	if (_noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

    DateTime::get_tm_LocalTime(&tmDateTime, &ulMilliSecs);

    if (fileName == "") 
    {
        sprintf(pUniqueFileName,
                "%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu_%lld_%lld_%s",
                (unsigned long) (tmDateTime. tm_year + 1900),
                (unsigned long) (tmDateTime. tm_mon + 1),
                (unsigned long) (tmDateTime. tm_mday),
                (unsigned long) (tmDateTime. tm_hour),
                (unsigned long) (tmDateTime. tm_min),
                (unsigned long) (tmDateTime. tm_sec),
                ulMilliSecs,
                llMediaItemKey,
                llPhysicalPathKey,
                _hostName.c_str());

        localFileName = pUniqueFileName;
    } 
    else 
    {
        localFileName = fileName;
    }

    sprintf(pDateTime,
            "%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu",
            (unsigned long) (tmDateTime. tm_year + 1900),
            (unsigned long) (tmDateTime. tm_mon + 1),
            (unsigned long) (tmDateTime. tm_mday),
            (unsigned long) (tmDateTime. tm_hour),
            (unsigned long) (tmDateTime. tm_min),
            (unsigned long) (tmDateTime. tm_sec),
            ulMilliSecs
            );

    // create the 'date' directory in staging if not exist
    {
		if (neededForTranscoder)
			assetPathName = MMSStorage::getTranscoderStagingRootRepository(_storage);
		else
			assetPathName = MMSStorage::getStagingRootRepository(_storage);
        assetPathName	/= (workspaceDirectoryName + "_" + directoryNamePrefix + "_" + pDateTime + relativePath);

		// 2023-06-02: questo metodo viene chiamato dall'engine (non dal transcoder),
		//	nel caso di directory 'locale' per il transcoder, non bisogna creare qui la directory
        if (!neededForTranscoder && !fs::exists(assetPathName)) 
        {
            _logger->info(__FILEREF__ + "Create directory"
                + ", assetPathName: " + assetPathName.string()
            );
			fs::create_directories(assetPathName);
			fs::permissions(assetPathName,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
				| fs::perms::group_read | fs::perms::group_exec 
				| fs::perms::others_read | fs::perms::others_exec,
				fs::perm_options::replace);
        }
    }

    {
        assetPathName /= localFileName;

		// 2023-06-02: questo metodo viene chiamato dall'engine (non dal transcoder),
		//	nel caso di directory 'locale' per il transcoder, non bisogna rimuovere nulla
        if (!neededForTranscoder && removeLinuxPathIfExist && fs::exists(assetPathName)) 
        {
            try 
            {
                if (fs::is_directory(assetPathName))
                {
                    _logger->info(__FILEREF__ + "Remove directory"
                        + ", assetPathName: " + assetPathName.string()
                    );
                    fs::remove_all(assetPathName);
                } 
                else if (fs::is_regular_file(assetPathName)) 
                {
                    _logger->info(__FILEREF__ + "Remove file"
                        + ", assetPathName: " + assetPathName.string()
                    );
                    fs::remove(assetPathName);
                } 
                else 
                {
					string errorMessage = string("Unexpected file in staging")                                  
                            + ", assetPathName: " + assetPathName.string();

					_logger->error(__FILEREF__ + errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch (...) 
            {
                //				 * the entry does not exist
                //				 *
                //				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                //					(const char *) errFileIO, __FILE__, __LINE__);
                //
                //				Error err = ToolsErrors (__FILE__, __LINE__,
                //					TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
                //					1, (const char *) bContentPathName);
                //				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                //					(const char *) err, __FILE__, __LINE__);
                //
                //				return err;
            }
        }
    }


    return assetPathName;
}

fs::path MMSStorage::creatingDirsUsingTerritories(
	unsigned long ulCurrentMMSPartitionIndex,
	string relativePath,
	string workspaceDirectoryName,
	bool deliveryRepositoriesToo,
	Workspace::TerritoriesHashMap& phmTerritories)
{
    char pMMSPartitionName [64];


	if (_noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

    sprintf(pMMSPartitionName, "MMS_%04lu", ulCurrentMMSPartitionIndex);

    fs::path mmsAssetPathName = MMSStorage::getMMSRootRepository(_storage)
		/ string(pMMSPartitionName) / workspaceDirectoryName / relativePath.substr(1);

    if (!fs::exists(mmsAssetPathName)) 
    {
        _logger->info(__FILEREF__ + "Create directory"
            + ", mmsAssetPathName: " + mmsAssetPathName.string()
        );
		fs::create_directories(mmsAssetPathName);
		fs::permissions(mmsAssetPathName,
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec 
			| fs::perms::group_read | fs::perms::group_exec 
			| fs::perms::others_read | fs::perms::others_exec,
			fs::perm_options::replace);
    }


    return mmsAssetPathName;
}

void MMSStorage::removePhysicalPath(int64_t physicalPathKey)
{
    try
    {
		if (_noFileSystemAccess)
		{
			string errorMessage = string("no rights to execute this method")
				+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

        _logger->info(__FILEREF__ + "getStorageDetailsByPhysicalPathKey ..."
            + ", physicalPathKey: " + to_string(physicalPathKey)
        );
        
        tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int,shared_ptr<Workspace>,
			string,string, string,string, uint64_t, bool> storageDetails =
            _mmsEngineDBFacade->getStorageDetails(physicalPathKey, true /*fromMaster*/);

		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
        int mmsPartitionNumber;
        shared_ptr<Workspace> workspace;
        string relativePath;
        string fileName;
        string deliveryFileName;
        string title;
        uint64_t sizeInBytes;
		bool externalReadOnlyStorage;

        tie(ignore, deliveryTechnology, mmsPartitionNumber, workspace, relativePath, fileName, 
                deliveryFileName, title, sizeInBytes, externalReadOnlyStorage) = storageDetails;

		if (!externalReadOnlyStorage)
		{
			removePhysicalPathFile(
				-1,
				physicalPathKey,
				deliveryTechnology,
				fileName,
				externalReadOnlyStorage,
				mmsPartitionNumber,
				workspace->_directoryName,
				relativePath,
				sizeInBytes);
		}

        _logger->info(__FILEREF__ + "removePhysicalPathKey ..."
            + ", physicalPathKey: " + to_string(physicalPathKey)
        );

        _mmsEngineDBFacade->removePhysicalPath(physicalPathKey);
    }
    catch(MediaItemKeyNotFound& e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    catch(runtime_error& e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    catch(exception& e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }    
}

void MMSStorage::removeMediaItem(int64_t mediaItemKey)
{
    try
    {
		if (_noFileSystemAccess)
		{
			string errorMessage = string("no rights to execute this method")
				+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

        _logger->info(__FILEREF__ + "getAllStorageDetails ..."
            + ", mediaItemKey: " + to_string(mediaItemKey)
        );

        vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string,
			int64_t, bool>> allStorageDetails;
        _mmsEngineDBFacade->getAllStorageDetails(mediaItemKey, false /*fromMaster*/, allStorageDetails);

        for (tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string,
				int64_t, bool>& storageDetails: allStorageDetails)
        {
			MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
			bool externalReadOnlyStorage;
			uint64_t sizeInBytes;

            tie(deliveryTechnology, mmsPartitionNumber, workspaceDirectoryName, relativePath,
					fileName, sizeInBytes, externalReadOnlyStorage) = storageDetails;

			if (!externalReadOnlyStorage)
			{
				removePhysicalPathFile(
					mediaItemKey,
					-1,
					deliveryTechnology,
					fileName,
					externalReadOnlyStorage,
					mmsPartitionNumber,
					workspaceDirectoryName,
					relativePath,
					sizeInBytes);
			}
        }

        _logger->info(__FILEREF__ + "removeMediaItem ..."
            + ", mediaItemKey: " + to_string(mediaItemKey)
        );
        _mmsEngineDBFacade->removeMediaItem(mediaItemKey);
    }
    catch(runtime_error& e)
    {
        string errorMessage = string("removeMediaItem failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", exception: " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception& e)
    {
        string errorMessage = string("removeMediaItem failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void MMSStorage::removePhysicalPathFile(
	int64_t mediaItemKey,
	int64_t physicalPathKey,
	MMSEngineDBFacade::DeliveryTechnology deliveryTechnology,
	string fileName,
	bool externalReadOnlyStorage,
	int partitionKey,
	string workspaceDirectoryName,
	string relativePath,
	uint64_t sizeInBytes
)
{
    try
    {
		if (_noFileSystemAccess)
		{
			string errorMessage = string("no rights to execute this method")
				+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

        _logger->info(__FILEREF__ + "removePhysicalPathFile"
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
        );

		{
			string m3u8Suffix(".m3u8");

			if (deliveryTechnology == MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming
				|| 
				fileName.size() >= m3u8Suffix.size()	// end with .m3u8
					&& 0 == fileName.compare(fileName.size()-m3u8Suffix.size(), m3u8Suffix.size(),
					m3u8Suffix)
				)
			{
				// in this case we have to removed the directory and not just the m3u8/mpd file
				fileName = "";
			}

			_logger->info(__FILEREF__ + "getMMSAssetPathName ..."
				+ ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage)
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", workspaceDirectoryName: " + workspaceDirectoryName
				+ ", relativePath: " + relativePath
				+ ", fileName: " + fileName
			);
			fs::path mmsAssetPathName = getMMSAssetPathName(
				externalReadOnlyStorage,
				partitionKey,
				workspaceDirectoryName,
				relativePath,
				fileName);

			if (fs::exists(mmsAssetPathName))
			{
				if (fs::is_directory(mmsAssetPathName)) 
				{
					try
					{
						_logger->info(__FILEREF__ + "Remove directory"
							+ ", mmsAssetPathName: " + mmsAssetPathName.string()
						);
						fs::remove_all(mmsAssetPathName);
					}
					catch(runtime_error& e)
					{
						string errorMessage = string("removeDirectory failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName.string()
							+ ", exception: " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}
					catch(exception& e)
					{
						string errorMessage = string("removeDirectory failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName.string()
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}

					uint64_t newCurrentFreeSizeInBytes =
						_mmsEngineDBFacade->updatePartitionBecauseOfDeletion(partitionKey,
								sizeInBytes);
					_logger->info(__FILEREF__ + "updatePartitionBecauseOfDeletion"
						+ ", partitionKey: " + to_string(partitionKey)
						+ ", newCurrentFreeSizeInBytes: " + to_string(newCurrentFreeSizeInBytes)
					);
				}
				else if (fs::is_regular_file(mmsAssetPathName)) 
				{
					try
					{
						_logger->info(__FILEREF__ + "Remove file"
							+ ", mmsAssetPathName: " + mmsAssetPathName.string()
						);
						fs::remove_all(mmsAssetPathName);
					}
					catch(runtime_error& e)
					{
						string errorMessage = string("remove failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName.string()
							+ ", exception: " + e.what()
						;
       
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}
					catch(exception& e)
					{
						string errorMessage = string("remove failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName.string()
						;
       
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}

					uint64_t newCurrentFreeSizeInBytes =
						_mmsEngineDBFacade->updatePartitionBecauseOfDeletion(partitionKey,
								sizeInBytes);
					_logger->info(__FILEREF__ + "updatePartitionBecauseOfDeletion"
						+ ", partitionKey: " + to_string(partitionKey)
						+ ", newCurrentFreeSizeInBytes: " + to_string(newCurrentFreeSizeInBytes)
					);
				}
				else 
				{
					string errorMessage = string("Unexpected directory entry");

					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
	}
	catch(runtime_error& e)
	{
		string errorMessage = string("removePhysicalPathFile failed")
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", exception: " + e.what()
		;

		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch(exception& e)
	{
		string errorMessage = string("removePhysicalPathFile failed")
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", physicalPathKey: " + to_string(physicalPathKey)
		;

		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}


fs::path MMSStorage::moveAssetInMMSRepository(
	int64_t ingestionJobKey,
	fs::path sourceAssetPathName,
	string workspaceDirectoryName,
	string destinationAssetFileName,
	string relativePath,

	unsigned long *pulMMSPartitionIndexUsed, // OUT
    // FileIO::DirectoryEntryType_p pSourceFileType,	// OUT: TOOLS_FILEIO_DIRECTORY or TOOLS_FILEIO_REGULARFILE

	bool deliveryRepositoriesToo,
	Workspace::TerritoriesHashMap& phmTerritories
)
{
	if (_noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

    if ((relativePath.size() > 0 && relativePath.front() != '/')
		|| pulMMSPartitionIndexUsed == (unsigned long *) NULL) 
	{
		string errorMessage = string("Wrong argument")                                                          
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", relativePath: " + relativePath;

		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
    }

	unsigned long long ullFSEntrySizeInBytes = 0;
	{
		if (fs::is_directory(sourceAssetPathName))
		{
			// recursive_directory_iterator, by default, does not follow sym links
			for (fs::directory_entry const& entry: fs::recursive_directory_iterator(sourceAssetPathName))
			{
				if (entry.is_regular_file())
					ullFSEntrySizeInBytes += entry.file_size();
			}
        }
        else if (fs::is_regular_file(sourceAssetPathName))
        {
			ullFSEntrySizeInBytes = fs::file_size(sourceAssetPathName);
        }
	}

    {
		int partitionKey;
		uint64_t newCurrentFreeSizeInBytes;

		try
		{
			#ifdef __POSTGRES__
			pair<int, uint64_t> partitionDetails = _mmsEngineDBFacade
				->getPartitionToBeUsedAndUpdateFreeSpace(ingestionJobKey, ullFSEntrySizeInBytes);
			#else
			pair<int, uint64_t> partitionDetails = _mmsEngineDBFacade
				->getPartitionToBeUsedAndUpdateFreeSpace(ullFSEntrySizeInBytes);
			#endif
			tie(partitionKey, newCurrentFreeSizeInBytes) = partitionDetails;
		}
		catch(NoMoreSpaceInMMSPartition& e)
		{
			string errorMessage = fmt::format(
				"getPartitionToBeUsedAndUpdateFreeSpace failed"
				", ingestionJobKey: {}"
				", ullFSEntrySizeInBytes: {}",
				ingestionJobKey, ullFSEntrySizeInBytes
			);
			_logger->error(__FILEREF__ + errorMessage);

			// 2024-01-21: A volte il campo del DB currentFreeSizeInBytes si "corrompe" ed assume un valore
			// non corretto (molto piu basso). Per essere sicuri che non sia un falso errore di 'no more space', 
			// settiamo il valore corretto di currentFreeSizeInBytes chiamando refreshPartitionsFreeSizes
			// e riproviamo
			refreshPartitionsFreeSizes();

			#ifdef __POSTGRES__
			pair<int, uint64_t> partitionDetails = _mmsEngineDBFacade
				->getPartitionToBeUsedAndUpdateFreeSpace(ingestionJobKey, ullFSEntrySizeInBytes);
			#else
			pair<int, uint64_t> partitionDetails = _mmsEngineDBFacade
				->getPartitionToBeUsedAndUpdateFreeSpace(ullFSEntrySizeInBytes);
			#endif
			tie(partitionKey, newCurrentFreeSizeInBytes) = partitionDetails;
		}

		_logger->info(__FILEREF__ + "getPartitionToBeUsedAndUpdateFreeSpace"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
			+ ", partitionKey: " + to_string(partitionKey)
			+ ", newCurrentFreeSizeInBytes: " + to_string(newCurrentFreeSizeInBytes)
		);

		*pulMMSPartitionIndexUsed = partitionKey;
    }

    // creating directories and build the bMMSAssetPathName
    fs::path mmsAssetPathName;
    {
        // to create the content provider directory and the
        // territories directories (if not already existing)
        mmsAssetPathName = creatingDirsUsingTerritories(*pulMMSPartitionIndexUsed,
            relativePath, workspaceDirectoryName, deliveryRepositoriesToo,
            phmTerritories);

        mmsAssetPathName /= destinationAssetFileName;
    }

	_logger->info(__FILEREF__ + "Selected MMS Partition for the content"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", workspaceDirectoryName: " + workspaceDirectoryName
		+ ", *pulMMSPartitionIndexUsed: " + to_string(*pulMMSPartitionIndexUsed)
		+ ", mmsAssetPathName: " + mmsAssetPathName.string()
		+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
	);

    // move the file in case of .3gp content OR
    // move the directory in case of IPhone content
    {
        if (fs::is_directory(sourceAssetPathName)) 
        {
			// 2020-04-11: I saw sometimes the below moveDirectory fails because the removeDirectory fails
			//	And this is because it fails the deletion of files like .nfs0000000103c87546000004d7
			//	For this reason we split moveDirectory in: copyDirectory and removeDirectory.
			//	In this way we will be able to manage the failure of the remove directory

			/*
            _logger->info(__FILEREF__ + "Move directory"
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
            );

			chrono::system_clock::time_point startPoint = chrono::system_clock::now();
            FileIO::moveDirectory(sourceAssetPathName,
                    mmsAssetPathName,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
			chrono::system_clock::time_point endPoint = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "Move directory statistics"
				+ ", @MMS MOVE statistics@ - elapsed (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
			);
			*/
            _logger->info(__FILEREF__ + "Copy directory"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", from: " + sourceAssetPathName.string()
                + ", to: " + mmsAssetPathName.string()
            );
			chrono::system_clock::time_point startPoint = chrono::system_clock::now();
			fs::copy(sourceAssetPathName, mmsAssetPathName, fs::copy_options::recursive);
			chrono::system_clock::time_point endPoint = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "Copy directory statistics"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", from: " + sourceAssetPathName.string()
                + ", to: " + mmsAssetPathName.string()
				+ ", @MMS COPY statistics@ - elapsed (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
			);

			try
			{
				_logger->info(__FILEREF__ + "Remove directory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName.string()
				);
				fs::remove_all(sourceAssetPathName);
			}
			catch(runtime_error& e)
			{
				// we will not raise an exception, it is a staging directory,
				// it will be removed by cronjob (see the comment above)
				_logger->error(__FILEREF__ + "fs::remove_all failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName.string()
					+ ", e.what(): " + e.what()
				);
			}
        }
        else // fs::is_regilar_file(sourceAssetPathName)) 
        {
            _logger->info(__FILEREF__ + "Move file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", from: " + sourceAssetPathName.string()
                + ", to: " + mmsAssetPathName.string()
                + ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
            );

			/*
			 * 2021-08-29: sometimes the moveFile failed:
			 * [2021-08-28 22:52:08.756] [mmsEngineService] [error] [tid 3114800] [MMSEngineProcessor.cpp:5992] _mmsStorage->moveAssetInMMSRepository failed, _processorIdentifier: 1, ingestionJobKey: 5477449, errorMessage: FileIO::moveFile failed: Class: ToolsErrors, Code: 211, File: /opt/catrasoftware/CatraLibraries/Tools/src/FileIO.cpp, Line: 5872, Msg: The write function failed. Errno: 5
			 * It is not clear the reason of this error, I'll try again
			 * 2021-09-05: I had again this error. Looking at the log, I saw that the size of the file
			 *	logged just before the starting of the move was 5923184964 and the move failed when
			 *	it already written bytes 6208068844. So May be the nfs was still writing the file
			 *	and we started to copy/move.
			 *	I'll increase the delay before to copy again.
			 */
			int64_t moveElapsedInSeconds;
			try
			{
				moveElapsedInSeconds = MMSStorage::move(ingestionJobKey, sourceAssetPathName, mmsAssetPathName, _logger);
			}
			catch(runtime_error& e)
			{
				_logger->error(__FILEREF__ + "Move file failed, wait a bit, retrieve again the size and try again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", from: " + sourceAssetPathName.string()
					+ ", to: " + mmsAssetPathName.string()
					+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
					+ ", _waitingNFSSync_maxMillisecondsToWait: " + to_string(_waitingNFSSync_maxMillisecondsToWait)
					+ ", e.what: " + e.what()
				);

				// scenario of the above comment marked as 2021-09-05
				this_thread::sleep_for(chrono::milliseconds(
					_waitingNFSSync_maxMillisecondsToWait));

				ullFSEntrySizeInBytes = fs::file_size(sourceAssetPathName);

				_logger->info(__FILEREF__ + "Move file again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", from: " + sourceAssetPathName.string()
					+ ", to: " + mmsAssetPathName.string()
					+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
				);

				moveElapsedInSeconds = MMSStorage::move(ingestionJobKey, sourceAssetPathName, mmsAssetPathName, _logger);
			}

            unsigned long ulDestFileSizeInBytes = fs::file_size(mmsAssetPathName);

			_logger->info(__FILEREF__ + "Move file statistics"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", from: " + sourceAssetPathName.string()
                + ", to: " + mmsAssetPathName.string()
                + ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
                + ", ulDestFileSizeInBytes: " + to_string(ulDestFileSizeInBytes)
				+ ", @MMS MOVE statistics@ - elapsed (secs): @"
				+ to_string(moveElapsedInSeconds) + "@"
			);

			if (ullFSEntrySizeInBytes != ulDestFileSizeInBytes)
			{
				string errorMessage = string("Source and destination file have different sizes")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", source: " + sourceAssetPathName.string()
					+ ", dest: " + mmsAssetPathName.string()
					+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
					+ ", ulDestFileSizeInBytes: " + to_string(ulDestFileSizeInBytes)
				;

				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
        }
    }


    return mmsAssetPathName;
}

int64_t MMSStorage::move(
	int64_t ingestionJobKey,
	fs::path source,
	fs::path dest,
	shared_ptr<spdlog::logger> logger)
{
	chrono::system_clock::time_point startPoint;
	chrono::system_clock::time_point endPoint;
	try
	{
		startPoint = chrono::system_clock::now();
		// fs::rename works only if source and destination are on the same file systems
		fs::rename(source, dest);
		endPoint = chrono::system_clock::now();
	}
	catch(fs::filesystem_error& e)
	{
		if (e.code().value() == 18)	// filesystem error: cannot rename: Invalid cross-device link
		{
			try
			{
				// copy and delete
				startPoint = chrono::system_clock::now();
				fs::copy(source, dest, fs::copy_options::recursive);
				fs::remove_all(source);
				endPoint = chrono::system_clock::now();
			}
			catch(fs::filesystem_error& e)
			{
				logger->error(__FILEREF__ + "move (copy and remove) failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", source: " + source.string()
					+ ", dest: " + dest.string()
					+ ", e.what: " + e.what()
					+ ", code value: " + to_string(e.code().value())
					+ ", code message: " + e.code().message()
					+ ", code category: " + e.code().category().name()
				);

				throw e;
			}
		}
		else if (e.code().value() == 17)	// filesystem error: cannot copy: File exists
		{
			logger->info(__FILEREF__ + "No move to be done, file already exists"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + source.string()
				+ ", dest: " + dest.string()
				+ ", e.what: " + e.what()
				+ ", code value: " + to_string(e.code().value())
				+ ", code message: " + e.code().message()
				+ ", code category: " + e.code().category().name()
			);
			endPoint = chrono::system_clock::now();
		}
		else
		{
			logger->error(__FILEREF__ + "Move failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + source.string()
				+ ", dest: " + dest.string()
				+ ", e.what: " + e.what()
				+ ", code value: " + to_string(e.code().value())
				+ ", code message: " + e.code().message()
				+ ", code category: " + e.code().category().name()
			);

			throw e;
		}
	}

	return chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count();
}

void MMSStorage::deleteWorkspace(
		shared_ptr<Workspace> workspace)
{
	if (_noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	{
		fs::path workspaceIngestionDirectory = MMSStorage::getIngestionRootRepository(_storage);
		workspaceIngestionDirectory /= workspace->_directoryName;

        if (fs::exists(workspaceIngestionDirectory))
        {
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", workspaceIngestionDirectory: " + workspaceIngestionDirectory.string()
			);
			fs::remove_all(workspaceIngestionDirectory);
        }
	}

	{
		fs::path liveRootDirectory = MMSStorage::getLiveRootRepository(_storage);
		liveRootDirectory /= workspace->_directoryName;

        if (fs::is_directory(liveRootDirectory))
        {
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", liveRootDirectory: " + liveRootDirectory.string()
			);
			fs::remove_all(liveRootDirectory);
        }
	}

	{
		vector<pair<int, uint64_t>> partitionsInfo;

		_mmsEngineDBFacade->getPartitionsInfo(partitionsInfo);

		for (pair<int, uint64_t> partitionInfo: partitionsInfo)
		{
			int partitionKey;
			uint64_t currentFreeSizeInBytes;

			tie(partitionKey, currentFreeSizeInBytes) = partitionInfo;

			fs::path workspacePathName = getMMSAssetPathName(
				false,	// externalReadOnlyStorage
				partitionKey,
				workspace->_directoryName,
                string(""),		// relativePath
				string("")		// fileName
				);

			if (fs::is_directory(workspacePathName))
			{
				uint64_t directorySizeInBytes = 0;

				// recursive_directory_iterator, by default, does not follow sym links
				for (fs::directory_entry const& entry: fs::recursive_directory_iterator(workspacePathName))
				{
					if (entry.is_regular_file())
						directorySizeInBytes += entry.file_size();
				}

				_logger->info(__FILEREF__ + "Remove directory"
					+ ", workspacePathName: " + workspacePathName.string()
				);
				fs::remove_all(workspacePathName);

				uint64_t newCurrentFreeSizeInBytes =
					_mmsEngineDBFacade->updatePartitionBecauseOfDeletion(partitionKey,
					directorySizeInBytes);
				_logger->info(__FILEREF__ + "updatePartitionBecauseOfDeletion"
					+ ", partitionKey: " + to_string(partitionKey)
					+ ", newCurrentFreeSizeInBytes: "
						+ to_string(newCurrentFreeSizeInBytes)
				);
			}
		}
	}
}

unsigned long MMSStorage::getWorkspaceStorageUsage(
	string workspaceDirectoryName)
{

    unsigned long ulStorageUsageInMB;

    unsigned long ulMMSPartitionIndex;
    unsigned long long ullDirectoryUsageInBytes;
    unsigned long long ullWorkspaceStorageUsageInBytes;


	if (_noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

    ullWorkspaceStorageUsageInBytes = 0;

	vector<pair<int, uint64_t>> partitionsInfo;

	_mmsEngineDBFacade->getPartitionsInfo(partitionsInfo);

	for (pair<int, uint64_t> partitionInfo: partitionsInfo)
    {
		int partitionKey;
		uint64_t currentFreeSizeInBytes;

		tie(partitionKey, currentFreeSizeInBytes) = partitionInfo;

		fs::path workspacePathName = getMMSAssetPathName(
			false,	// externalReadOnlyStorage
			partitionKey,
			workspaceDirectoryName,
			string(""),		// relativePath
			string("")		// fileName
		);

		if (fs::is_directory(workspacePathName))
		{
			try
			{
				ullDirectoryUsageInBytes = 0;
				// recursive_directory_iterator, by default, does not follow sym links
				for (fs::directory_entry const& entry: fs::recursive_directory_iterator(workspacePathName))
				{
					if (entry.is_regular_file())
						ullDirectoryUsageInBytes += entry.file_size();
				}
			}
			catch(runtime_error& e)
			{
				ullDirectoryUsageInBytes		= 0;

				_logger->error(__FILEREF__ + "getDirectorySizeInBytes failed"
					+ ", e.what(): " + e.what()
				);
			}

			ullWorkspaceStorageUsageInBytes += ullDirectoryUsageInBytes;
		}
    }


    ulStorageUsageInMB = (unsigned long) (ullWorkspaceStorageUsageInBytes / (1000 * 1000));


    return ulStorageUsageInMB;
}

void MMSStorage::refreshPartitionsFreeSizes() 
{
	int partitionKey = 0;
	bool mmsAvailablePartitions = true;

	if (_noFileSystemAccess)
	{
		string errorMessage = string("no rights to execute this method")
			+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	while (mmsAvailablePartitions) 
	{
		fs::path partitionPathName;
		{
			char pMMSPartitionName [64];

			sprintf(pMMSPartitionName, "MMS_%04d", partitionKey);

			partitionPathName = MMSStorage::getMMSRootRepository(_storage);
			partitionPathName /= pMMSPartitionName;
		}

		if (!fs::exists(partitionPathName))
		{
			mmsAvailablePartitions = false;

			continue;
		}

		uint64_t currentFreeSizeInBytes;
		{
			uint64_t usedInBytes;
			uint64_t availableInBytes;
			long lPercentUsed;

			chrono::system_clock::time_point startPoint = chrono::system_clock::now();

			fs::space_info si = fs::space(partitionPathName);

			currentFreeSizeInBytes = si.available;

			chrono::system_clock::time_point endPoint = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "refreshPartitionFreeSizes"
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", partitionPathName: " + partitionPathName.string()
				+ ", currentFreeSizeInBytes: " + to_string(currentFreeSizeInBytes)
				+ ", @MMS statistics@ - elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
			);
		}

		int localFreeSpaceToLeaveInMB;
		{
			char pMMSPartitionName [64];
			sprintf(pMMSPartitionName, "%04d", partitionKey);
			string freeSpaceConfField = string("freeSpaceToLeaveInEachPartitionInMB_")
				+ pMMSPartitionName;

			localFreeSpaceToLeaveInMB = JSONUtils::asInt(
				_configuration["storage"], freeSpaceConfField, _freeSpaceToLeaveInEachPartitionInMB);
		}

		_logger->info(__FILEREF__ + "addUpdatePartitionInfo"
			+ ", partitionKey: " + to_string(partitionKey)
			+ ", partitionPathName: " + partitionPathName.string()
			+ ", currentFreeSizeInBytes: " + to_string(currentFreeSizeInBytes)
			+ ", localFreeSpaceToLeaveInMB: " + to_string(localFreeSpaceToLeaveInMB)
		);
		_mmsEngineDBFacade->addUpdatePartitionInfo(partitionKey, partitionPathName,
			currentFreeSizeInBytes, localFreeSpaceToLeaveInMB);

		partitionKey++;
	}

	/*
	{
		string infoMessage = string("refreshPartitionsFreeSizes. MMS Partitions info")
			+ ", _mmsPartitionsInfo.size: " + to_string(_mmsPartitionsInfo.size())
		;
		for (int ulMMSPartitionIndex = 0;
			ulMMSPartitionIndex < _mmsPartitionsInfo.size();
			ulMMSPartitionIndex++) 
		{
			infoMessage +=
				(", _mmsPartitionsInfo [" + to_string(ulMMSPartitionIndex) + "]: "
					+ to_string((_mmsPartitionsInfo[ulMMSPartitionIndex])._currentFreeSizeInBytes))
			;
		}

		_logger->info(__FILEREF__ + infoMessage);
	}
	*/
}

// this method is in this class just because it is called
// by both MMSEngineProcessor and API_Ingestion. I needed a library linked by both
// the components
void MMSStorage::manageTarFileInCaseOfIngestionOfSegments(
	int64_t ingestionJobKey,
	string tarBinaryPathName, string workspaceIngestionRepository,
	string sourcePathName
)
{
	// tarBinaryPathName like /var/catramms/storage/IngestionRepository/users/2/1449874_source.tar.gz
	// workspaceIngestionRepository like /var/catramms/storage/IngestionRepository/users/2
	// sourcePathName: /var/catramms/storage/MMSWorkingAreaRepository/Staging/2_1449859_virtualVOD_2022_08_11_12_41_46_0212/1449859_liveRecorderVirtualVOD.tar.gz

	string executeCommand;
	try
	{
		if (_noFileSystemAccess)
		{
			string errorMessage = string("no rights to execute this method")
				+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		_logger->info(__FILEREF__ + "Received manageTarFileInCaseOfIngestionOfSegments"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", tarBinaryPathName: " + tarBinaryPathName
			+ ", tarBinary size: " + to_string(fs::file_size(tarBinaryPathName))
			+ ", workspaceIngestionRepository: " + workspaceIngestionRepository
			+ ", sourcePathName: " + sourcePathName
		);

		// non possiamo eseguire il tar contenente la directory content direttamente
		// in workspaceIngestionRepository perchè se lo stesso user ingesta due tar contemporaneamente,
		// la directory content viene usata per entrambi i tar creando problemi
		// Aggiungiamo quindi l'ingestionJobKey
		string workIngestionDirectory = fmt::format("{}/{}",
			workspaceIngestionRepository, ingestionJobKey);

		_logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", workIngestionDirectory: " + workIngestionDirectory
		);
		fs::create_directories(workIngestionDirectory);
		fs::permissions(workIngestionDirectory,
			fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
			| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec
			| fs::perms::others_exec,
			fs::perm_options::replace);

		// tar into workspaceIngestion directory
		//	source will be something like <ingestion key>_source
		//	destination will be the original directory (that has to be the same name of the tar file name)
		executeCommand = fmt::format(
			"tar xfz {} --directory {}",
			tarBinaryPathName, workIngestionDirectory);
		_logger->info(__FILEREF__ + "Start tar command "
			+ ", executeCommand: " + executeCommand
		);
		chrono::system_clock::time_point startTar = chrono::system_clock::now();
		int executeCommandStatus = ProcessUtility::execute(executeCommand);
		chrono::system_clock::time_point endTar = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "End tar command "
			+ ", executeCommand: " + executeCommand
			+ ", @MMS statistics@ - tarDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endTar - startTar).count()) + "@"
		);
		if (executeCommandStatus != 0)
		{
			string errorMessage = string("ProcessUtility::execute failed")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", executeCommandStatus: " + to_string(executeCommandStatus) 
				+ ", executeCommand: " + executeCommand 
			;

			_logger->error(__FILEREF__ + errorMessage);
          
			throw runtime_error(errorMessage);
		}

		// sourceFileName is the name of the tar file name that is the same
		//	of the name of the directory inside the tar file
		string sourceFileName;
		{
			string suffix(".tar.gz");
			if (!(sourcePathName.size() >= suffix.size()
				&& 0 == sourcePathName.compare(sourcePathName.size()-suffix.size(), suffix.size(), suffix)))
			{
				string errorMessage = __FILEREF__ + "sourcePathName does not end with " + suffix
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourcePathName: " + sourcePathName
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			size_t startFileNameIndex = sourcePathName.find_last_of("/");
			if (startFileNameIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "sourcePathName bad format"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourcePathName: " + sourcePathName
					+ ", startFileNameIndex: " + to_string(startFileNameIndex)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceFileName = sourcePathName.substr(startFileNameIndex + 1);
			sourceFileName = sourceFileName.substr(0, sourceFileName.size() - suffix.size());
		}

		// remove tar file
		{
			fs::path sourceTarFile = workspaceIngestionRepository;
			sourceTarFile /= (to_string(ingestionJobKey) + "_source" + ".tar.gz");

			_logger->info(__FILEREF__ + "Remove file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceTarFile: " + sourceTarFile.string()
			);

			fs::remove_all(sourceTarFile);
		}

		// rename directory generated from tar: from user_tar_filename to 1247848_source
		// Example from /var/catramms/storage/IngestionRepository/users/1/9670725_liveRecorderVirtualVOD
		//	to /var/catramms/storage/IngestionRepository/users/1/9676038_source
		{
			fs::path sourceDirectory = workIngestionDirectory;
			sourceDirectory /= sourceFileName;

			fs::path destDirectory = workspaceIngestionRepository;
			destDirectory /= (to_string(ingestionJobKey) + "_source");

			_logger->info(__FILEREF__ + "Start copyDirectory..."
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceDirectory: " + sourceDirectory.string()
				+ ", destDirectory: " + destDirectory.string()
			);
			// 2020-05-01: since the remove of the director could fails because of nfs issue,
			//	better do a copy and then a remove.
			//	In this way, in case the remove fails, we can ignore the error.
			//	The directory will be removed later by cron job
			{
				chrono::system_clock::time_point startPoint = chrono::system_clock::now();
				fs::copy(sourceDirectory, destDirectory, fs::copy_options::recursive);
				chrono::system_clock::time_point endPoint = chrono::system_clock::now();
				/*
				int64_t sourceDirectorySize = 0;
				int64_t destDirectorySize = 0;
				{
					// recursive_directory_iterator, by default, does not follow sym links
					for (fs::directory_entry const& entry: fs::recursive_directory_iterator(sourceDirectory))
					{
						if (entry.is_regular_file())
							sourceDirectorySize += entry.file_size();
					}
					for (fs::directory_entry const& entry: fs::recursive_directory_iterator(destDirectory))
					{
						if (entry.is_regular_file())
							destDirectorySize += entry.file_size();
					}
				}
				*/
				_logger->info(__FILEREF__ + "End copyDirectory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceDirectory: " + sourceDirectory.string()
					+ ", destDirectory: " + destDirectory.string()
					// + ", sourceDirectorySize: " + to_string(sourceDirectorySize)
					// + ", destDirectorySize: " + to_string(destDirectorySize)
					+ ", @MMS COPY statistics@ - copyDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
				);
			}

			try
			{
				chrono::system_clock::time_point startPoint = chrono::system_clock::now();
				fs::remove_all(workIngestionDirectory);
				chrono::system_clock::time_point endPoint = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "End removeDirectory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", workIngestionDirectory: " + workIngestionDirectory
					+ ", @MMS REMOVE statistics@ - removeDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
				);
			}
			catch(runtime_error& e)
			{
				string errorMessage = string("removeDirectory failed")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", e.what: " + e.what() 
				;
				_logger->error(__FILEREF__ + errorMessage);
         
				// throw runtime_error(errorMessage);
			}
		}
	}
	catch(runtime_error& e)
	{
		string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", e.what: " + e.what() 
		;
		_logger->error(__FILEREF__ + errorMessage);
         
		throw runtime_error(errorMessage);
	}
}

