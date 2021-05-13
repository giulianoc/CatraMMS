
#include <fstream>
#include "MMSStorage.h"
#include "JSONUtils.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/System.h"
#include "catralibraries/DateTime.h"

MMSStorage::MMSStorage(
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger) 
{

	try
	{
		_logger             = logger;
		_configuration		= configuration;

		_hostName = System::getHostName();

		_storage = configuration["storage"].get("path", "XXX").asString();
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", storage->path: " + _storage
		);
		if (_storage.size() > 0 && _storage.back() != '/')
			_storage.push_back('/');

		_freeSpaceToLeaveInEachPartitionInMB = JSONUtils::asInt(configuration["storage"], "freeSpaceToLeaveInEachPartitionInMB", 100);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", storage->freeSpaceToLeaveInEachPartitionInMB: " + to_string(_freeSpaceToLeaveInEachPartitionInMB)
		);

		// _ingestionRootRepository = _storage + "IngestionRepository/users/";
		// _mmsRootRepository = _storage + "MMSRepository/";
		// _downloadRootRepository = _storage + "DownloadRepository/";
		// _streamingRootRepository = _storage + "StreamingRepository/";

		// _stagingRootRepository = _storage + "MMSWorkingAreaRepository/Staging/";
		// _transcoderStagingRootRepository = _storage + "MMSTranscoderWorkingAreaRepository/Staging/";
		// _deliveryFreeRootRepository = _storage + "MMSRepository-free/";

		// _liveRootRepository = _storage + "MMSRepository/" + MMSStorage::getDirectoryForLiveContents() + "/";

		// string ffmpegArea = _storage + "MMSTranscoderWorkingAreaRepository/ffmpeg/";
    
		// string nginxArea = _storage + "MMSWorkingAreaRepository/nginx/";

		// _profilesRootRepository = _storage + "MMSRepository/EncodingProfiles/";

		MMSStorage::createDirectories(configuration, _logger);

		// Partitions staff
		{
			char pMMSPartitionName [64];


			lock_guard<recursive_mutex> locker(_mtMMSPartitions);

			unsigned long ulMMSPartitionsNumber = 0;
			bool mmsAvailablePartitions = true;

			_ulCurrentMMSPartitionIndex = 0;

			// inizializzare PartitionInfos
			while (mmsAvailablePartitions) 
			{
				string partitionPathName(MMSStorage::getMMSRootRepository(_storage));
				sprintf(pMMSPartitionName, "MMS_%04lu", ulMMSPartitionsNumber++);
				partitionPathName.append(pMMSPartitionName);

				PartitionInfo	partitionInfo;

				partitionInfo._partitionPathName = partitionPathName;

				if (!FileIO::directoryExisting(partitionPathName))
				{
					mmsAvailablePartitions = false;

					continue;
				}

				string partitionInfoPathName = partitionPathName;

				partitionInfoPathName.append("/partitionInfo.json");
				_logger->info(__FILEREF__ + "Looking for the Partition info file"
					+ ", partitionInfoPathName: " + partitionInfoPathName
				);
				if (FileIO::fileExisting(partitionInfoPathName))
				{
					/*
					* In case of a partition where only a subset of it is dedicated to MMS,
					* we cannot use getFileSystemInfo because it will return info about the entire partition.
					* So this conf file will tell us
					*	- the max size of this storage to be used
					*	- the procedure to be used to get the current MMS Usage, in particular getDirectoryUsage
					*		calculate the usage of any directory/files
					* Sample of file:
					{
						"partitionUsageType": "getDirectoryUsage",
						"maxStorageUsageInKB": 1500000000
					}
					*/
					Json::Value partitionInfoJson;

					try
					{
						ifstream partitionInfoFile(partitionInfoPathName.c_str(), std::ifstream::binary);
						partitionInfoFile >> partitionInfoJson;

						// getFileSystemInfo (default and more performant) or getDirectoryUsage
						string field = "partitionUsageType";
						if (!JSONUtils::isMetadataPresent(partitionInfoJson, field))
							partitionInfo._partitionUsageType = "getFileSystemInfo";
						else
						{
							partitionInfo._partitionUsageType	= partitionInfoJson.get(field, "").asString();
							if (partitionInfo._partitionUsageType != "getDirectoryUsage")
								partitionInfo._partitionUsageType = "getFileSystemInfo";
						}

						field = "maxStorageUsageInKB";
						if (JSONUtils::isMetadataPresent(partitionInfoJson, field))
							partitionInfo._maxStorageUsageInKB       = JSONUtils::asInt64(partitionInfoJson, field, -1);
						else
							partitionInfo._maxStorageUsageInKB       = -1;
					}
					catch(...)
					{
						_logger->error(__FILEREF__ + "wrong json partition info format"
							+ ", partitionInfoPathName: " + partitionInfoPathName
						);
					}
				}
				else
				{
					partitionInfo._partitionUsageType		= "getFileSystemInfo";
					partitionInfo._maxStorageUsageInKB		= -1;
				}

				refreshPartitionFreeSizes(partitionInfo);

				_mmsPartitionsInfo.push_back(partitionInfo);
			}

			if (_mmsPartitionsInfo.size() == 0)
			{
				_logger->error(__FILEREF__ + "No partition available");

				throw runtime_error("No MMS partition found");
			}
		}
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception e)
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

	try
	{
		string storage = configuration["storage"].get("path", "").asString();
		logger->info(__FILEREF__ + "Configuration item"
			+ ", storage->path: " + storage
		);
		if (storage.size() > 0 && storage.back() != '/')
			storage.push_back('/');

		// string ingestionRootRepository = MMSStorage::getIngestionRootRepository(storage);
		// string mmsRootRepository = MMSStorage::getMMSRootRepository(storage);
		// _downloadRootRepository = _storage + "DownloadRepository/";
		// _streamingRootRepository = _storage + "StreamingRepository/";

		// string stagingRootRepository = storage + "MMSWorkingAreaRepository/Staging/";
		// string transcoderStagingRootRepository = storage + "MMSTranscoderWorkingAreaRepository/Staging/";
		// _deliveryFreeRootRepository = _storage + "MMSRepository-free/";

		// string liveRootRepository = storage + "MMSRepository/" + MMSStorage::getDirectoryForLiveContents() + "/";

		// string ffmpegArea = storage + "MMSTranscoderWorkingAreaRepository/ffmpeg/";
    
		// string nginxArea = storage + "MMSWorkingAreaRepository/nginx/";

		// _profilesRootRepository = _storage + "MMSRepository/EncodingProfiles/";

		bool noErrorIfExists = true;
		bool recursive = true;
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", ingestionRootRepository: " + MMSStorage::getIngestionRootRepository(storage)
		);
		FileIO::createDirectory(MMSStorage::getIngestionRootRepository(storage),
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", mmsRootRepository: " + MMSStorage::getMMSRootRepository(storage)
		);
		FileIO::createDirectory(MMSStorage::getMMSRootRepository(storage),
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

		// create MMS_0000 in case it does not exist (first running of MMS)
		{
			string MMS_0000Path = MMSStorage::getMMSRootRepository(storage) + "MMS_0000";


			logger->info(__FILEREF__ + "Creating directory (if needed)"
				+ ", MMS_0000 Path: " + MMS_0000Path
			);
			FileIO::createDirectory(MMS_0000Path,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
		}

		/*
		_logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", _downloadRootRepository: " + _downloadRootRepository
		);
		FileIO::createDirectory(_downloadRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

		_logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", _streamingRootRepository: " + _streamingRootRepository
		);
		FileIO::createDirectory(_streamingRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
		*/

		/*
		_logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", _profilesRootRepository: " + _profilesRootRepository
		);
		FileIO::createDirectory(_profilesRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
		*/

		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", stagingRootRepository: " + MMSStorage::getStagingRootRepository(storage)
		);
		FileIO::createDirectory(MMSStorage::getStagingRootRepository(storage),
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", transcoderStagingRootRepository: " + MMSStorage::getTranscoderStagingRootRepository(storage)
		);
		FileIO::createDirectory(MMSStorage::getTranscoderStagingRootRepository(storage),
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

		/*
		_logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", _deliveryFreeRootRepository: " + _deliveryFreeRootRepository
		);
		FileIO::createDirectory(_deliveryFreeRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
		*/
		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", liveRootRepository: " + MMSStorage::getLiveRootRepository(storage) 
		);
		FileIO::createDirectory(MMSStorage::getLiveRootRepository(storage),
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", ffmpegArea: " + getFFMPEGArea(storage)
		);
		FileIO::createDirectory(getFFMPEGArea(storage),
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

		logger->info(__FILEREF__ + "Creating directory (if needed)"
			+ ", nginxArea: " + getNginxArea(storage)
		);
		FileIO::createDirectory(getNginxArea(storage),
            S_IRUSR | S_IWUSR | S_IXUSR 
            | S_IRGRP | S_IWGRP | S_IXGRP
            | S_IROTH | S_IWOTH | S_IXOTH, 
            noErrorIfExists, recursive);
	}
	catch(runtime_error e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
	catch(exception e)
	{
		logger->error(__FILEREF__ + "MMSStorage::MMSStorage failed"
			+ ", e.what(): " + e.what()
		);
	}
}

string MMSStorage::getMMSRootRepository(string storage) {
    return storage + "MMSRepository/";
}

string MMSStorage::getMMSRootRepository() {
    return MMSStorage::getMMSRootRepository(_storage);
}

/*
string MMSStorage::getStreamingRootRepository(void) {
    return _streamingRootRepository;
}

string MMSStorage::getDownloadRootRepository(void) {
    return _downloadRootRepository;
}
*/

string MMSStorage::getIngestionRootRepository(string storage) {
    return storage + "IngestionRepository/users/";
}

tuple<int64_t, string, int, string, string, int64_t, string>
	MMSStorage::getPhysicalPathDetails(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		int64_t mediaItemKey, int64_t encodingProfileKey,
		bool warningIfMissing)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
				string, string, string, string, int64_t, bool>
			storageDetails = mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey,
			warningIfMissing);

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
			fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage) = storageDetails;

		_logger->info(__FILEREF__ + "getMMSAssetPathName ..."
			+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
			+ ", workspaceDirectoryName: " + workspace->_directoryName
			+ ", relativePath: " + relativePath
			+ ", fileName: " + fileName
		);
		string physicalPath = getMMSAssetPathName(
			externalReadOnlyStorage,
			mmsPartitionNumber,
			workspace->_directoryName,
			relativePath,
			fileName);
    
		return make_tuple(physicalPathKey, physicalPath, mmsPartitionNumber,
				relativePath, fileName, sizeInBytes, deliveryFileName);
    }
    catch(MediaItemKeyNotFound e)
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
    catch(runtime_error e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

tuple<string, int, string, string, int64_t, string> MMSStorage::getPhysicalPathDetails(
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, int64_t physicalPathKey)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, int64_t, bool> storageDetails =
			mmsEngineDBFacade->getStorageDetails(physicalPathKey);

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
			+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
			+ ", workspaceDirectoryName: " + workspace->_directoryName
			+ ", relativePath: " + relativePath
			+ ", fileName: " + fileName
		);
		string physicalPath = getMMSAssetPathName(
			externalReadOnlyStorage,
			mmsPartitionNumber,
			workspace->_directoryName,
			relativePath,
			fileName);

		return make_tuple(physicalPath, mmsPartitionNumber, relativePath,
			fileName, sizeInBytes, deliveryFileName);
    }
    catch(MediaItemKeyNotFound e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(runtime_error e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception e)
    {
        string errorMessage = string("getPhysicalPathDetails failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

pair<string, string> MMSStorage::getVODDeliveryURI(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		int64_t physicalPathKey, bool save, shared_ptr<Workspace> requestWorkspace)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string,
			string, string, int64_t, bool> storageDetails =
			mmsEngineDBFacade->getStorageDetails(physicalPathKey);

		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		int mmsPartitionNumber;
		shared_ptr<Workspace> contentWorkspace;
		string relativePath;
		string fileName;
		string deliveryFileName;
		string title;
		bool externalReadOnlyStorage;
		tie(ignore, deliveryTechnology, mmsPartitionNumber, contentWorkspace, relativePath, fileName, 
            deliveryFileName, title, ignore, externalReadOnlyStorage) = storageDetails;

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

		return make_pair(deliveryFileName, deliveryURI);
    }
    catch(MediaItemKeyNotFound e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(runtime_error e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

tuple<int64_t, string, string> MMSStorage::getVODDeliveryURI(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		int64_t mediaItemKey, int64_t encodingProfileKey, bool save,
		shared_ptr<Workspace> requestWorkspace)
{
	try
	{
		bool warningIfMissing = false;
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int,
			shared_ptr<Workspace>,string,string,string,string,int64_t, bool> storageDetails =
			mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey,
			warningIfMissing);

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

		return make_tuple(physicalPathKey, deliveryFileName, deliveryURI);
    }
    catch(MediaItemKeyNotFound e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(runtime_error e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception e)
    {
        string errorMessage = string("getDeliveryURI failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
}

/*
string MMSStorage::getDeliveryFreeAssetPathName(
	string workspaceDirectoryName,
	string liveProxyAssetName,
	string assetExtension
)
{

	string deliveryFreeAssetPathName = _deliveryFreeRootRepository
		+ workspaceDirectoryName + "/" + liveProxyAssetName + "/"
		+ liveProxyAssetName + assetExtension;


    return deliveryFreeAssetPathName;
}
*/

string MMSStorage::getLiveDeliveryAssetPathName(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		string directoryId,
		string liveFileExtension, shared_ptr<Workspace> requestWorkspace)
{
	tuple<string, string, string> liveDeliveryDetails = getLiveDeliveryDetails(
			mmsEngineDBFacade, directoryId,
			liveFileExtension, requestWorkspace);

	string deliveryPath;
	string deliveryPathName;
	string deliveryFileName;

	tie(deliveryPathName, deliveryPath, deliveryFileName) = liveDeliveryDetails;

	string deliveryAssetPathName = MMSStorage::getMMSRootRepository(_storage)
		+ deliveryPathName.substr(1);

	return deliveryAssetPathName;
}

string MMSStorage::getLiveDeliveryAssetPath(
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	string directoryId, shared_ptr<Workspace> requestWorkspace)
{
	string liveFileExtension = "xxx";

	tuple<string, string, string> liveDeliveryDetails = getLiveDeliveryDetails(
		mmsEngineDBFacade, directoryId,
		liveFileExtension, requestWorkspace);

	string deliveryPath;
	string deliveryPathName;
	string deliveryFileName;

	tie(deliveryPathName, deliveryPath, deliveryFileName) = liveDeliveryDetails;

	string deliveryAssetPath = MMSStorage::getMMSRootRepository(_storage)
		+ deliveryPath.substr(1);

	return deliveryAssetPath;
}

tuple<string, string, string> MMSStorage::getLiveDeliveryDetails(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		string directoryId, string liveFileExtension,
		shared_ptr<Workspace> requestWorkspace)
{
	string deliveryPath;
	string deliveryPathName;
	string deliveryFileName;

	try
	{
		// if (liveURLType == "LiveProxy")
		{
			deliveryFileName = directoryId + "." + liveFileExtension;

			deliveryPath = "/" + MMSStorage::getDirectoryForLiveContents()
				+ "/" + requestWorkspace->_directoryName + "/" + directoryId;

			deliveryPathName = deliveryPath + "/" + deliveryFileName;
		}
    }
    catch(runtime_error e)
    {
        string errorMessage = string("getLiveDeliveryDetails failed")
            + ", directoryId: " + directoryId
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }
    catch(exception e)
    {
        string errorMessage = string("getLiveDeliveryDetails failed")
            + ", directoryId: " + directoryId
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        throw e;
    }

	return make_tuple(deliveryPathName, deliveryPath, deliveryFileName);
}

string MMSStorage::getWorkspaceIngestionRepository(shared_ptr<Workspace> workspace)
{
    string workspaceIngestionDirectory = MMSStorage::getIngestionRootRepository(_storage);
    workspaceIngestionDirectory.append(workspace->_directoryName);
    
    if (!FileIO::directoryExisting(workspaceIngestionDirectory)) 
    {
        _logger->info(__FILEREF__ + "Create directory"
            + ", workspaceIngestionDirectory: " + workspaceIngestionDirectory
        );

        bool noErrorIfExists = true;
        bool recursive = true;
        FileIO::createDirectory(workspaceIngestionDirectory,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP |
                S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    return workspaceIngestionDirectory;
}

string MMSStorage::getStagingRootRepository(string storage) {
    return storage + "MMSWorkingAreaRepository/Staging/";
}

string MMSStorage::getTranscoderStagingRootRepository(string storage) {
    return storage + "MMSTranscoderWorkingAreaRepository/Staging/";
}

string MMSStorage::getDirectoryForLiveContents() {
    return "MMSLive";
}

string MMSStorage::getLiveRootRepository(string storage) {
	return storage + "MMSRepository/" + MMSStorage::getDirectoryForLiveContents() + "/";
}

    
string MMSStorage::getFFMPEGArea(string storage) {
	return storage + "MMSTranscoderWorkingAreaRepository/ffmpeg/";
}

string MMSStorage::getNginxArea(string storage) {
	return storage + "MMSWorkingAreaRepository/nginx/";
}

string MMSStorage::getRepository(RepositoryType rtRepositoryType) 
{

    switch (rtRepositoryType) 
    {
        case RepositoryType::MMSREP_REPOSITORYTYPE_MMSCUSTOMER:
        {
            return MMSStorage::getMMSRootRepository(_storage);
        }
		/*
        case RepositoryType::MMSREP_REPOSITORYTYPE_DOWNLOAD:
        {
            return _downloadRootRepository;
        }
        case RepositoryType::MMSREP_REPOSITORYTYPE_STREAMING:
        {
            return _streamingRootRepository;
        }
		*/
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

string MMSStorage::getMMSAssetPathName(
		bool externalReadOnlyStorage,
        unsigned long ulPartitionNumber,
        string workspaceDirectoryName,
        string relativePath, // using '/'
        string fileName)
{
	string assetPathName;

	if (externalReadOnlyStorage)
	{
		assetPathName = MMSStorage::getMMSRootRepository(_storage) + "ExternalStorage_" + workspaceDirectoryName
			+ relativePath + fileName;
	}
	else
	{
		PartitionInfo partitionInfo = _mmsPartitionsInfo[ulPartitionNumber];
		assetPathName =
			partitionInfo._partitionPathName
			+ "/"
			+ workspaceDirectoryName
			+ relativePath
			+ fileName;
	}

    return assetPathName;
}

/*
string MMSStorage::getDownloadLinkPathName(
        unsigned long ulPartitionNumber,
        string workspaceDirectoryName,
        string territoryName,
        string relativePath,
        string fileName,
        bool downloadRepositoryToo)
{

    char pMMSPartitionName [64];
    string linkPathName;

    if (downloadRepositoryToo) 
    {
        sprintf(pMMSPartitionName, "MMS_%04lu/", ulPartitionNumber);

        linkPathName = _downloadRootRepository;
        linkPathName
            .append(pMMSPartitionName)
            .append(workspaceDirectoryName)
            .append("/")
            .append(territoryName)
            .append(relativePath)
            .append(fileName);
    } 
    else
    {
        sprintf(pMMSPartitionName, "/MMS_%04lu/", ulPartitionNumber);

        linkPathName = pMMSPartitionName;
        linkPathName
            .append(workspaceDirectoryName)
            .append("/")
            .append(territoryName)
            .append(relativePath)
            .append(fileName);
    }


    return linkPathName;
}

string MMSStorage::getStreamingLinkPathName(
        unsigned long ulPartitionNumber, // IN
        string workspaceDirectoryName, // IN
        string territoryName, // IN
        string relativePath, // IN
        string fileName) // IN
{
    char pMMSPartitionName [64];
    string linkPathName;


    sprintf(pMMSPartitionName, "MMS_%04lu/", ulPartitionNumber);

    linkPathName = _streamingRootRepository;
    linkPathName
        .append(pMMSPartitionName)
        .append(workspaceDirectoryName)
        .append("/")
        .append(territoryName)
        .append(relativePath)
        .append(fileName);


    return linkPathName;
}
*/

string MMSStorage::getStagingAssetPathName(
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
    string assetPathName;


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
        assetPathName
            .append(workspaceDirectoryName)
            .append("_")    // .append("/")
            .append(directoryNamePrefix)
            .append("_")
            .append(pDateTime)
            .append(relativePath);

        if (!FileIO::directoryExisting(assetPathName)) 
        {
            _logger->info(__FILEREF__ + "Create directory"
                + ", assetPathName: " + assetPathName
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(
                    assetPathName,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }
    }

    {
        assetPathName.append(localFileName);

        if (removeLinuxPathIfExist) 
        {
            FileIO::DirectoryEntryType_t detSourceFileType;

            try 
            {
                detSourceFileType = FileIO::getDirectoryEntryType(assetPathName);

                if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
                {
                    _logger->info(__FILEREF__ + "Remove directory"
                        + ", assetPathName: " + assetPathName
                    );
                    bool removeRecursively = true;
                    FileIO::removeDirectory(assetPathName, removeRecursively);
                } 
                else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
                {
                    _logger->info(__FILEREF__ + "Remove file"
                        + ", assetPathName: " + assetPathName
                    );
                    FileIO::remove(assetPathName);
                } 
                else 
                {
					string errorMessage = string("Unexpected file in staging")                                  
                            + ", assetPathName: " + assetPathName;

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

/*
string MMSStorage::getEncodingProfilePathName(
        long long llEncodingProfileKey,
        string profileFileNameExtension)
{
    string encodingProfilePathName(_profilesRootRepository);

    encodingProfilePathName
        .append(to_string(llEncodingProfileKey))
        .append(profileFileNameExtension);

    return encodingProfilePathName;
}

string MMSStorage::getFFMPEGEncodingProfilePathName(
        MMSEngineDBFacade::ContentType contentType,
        long long llEncodingProfileKey)
{

    if (contentType != MMSEngineDBFacade::ContentType::Video && 
            contentType != MMSEngineDBFacade::ContentType::Audio &&
            contentType != MMSEngineDBFacade::ContentType::Image)
    {
		string errorMessage = string("Wrong argument")                                                          
                + ", contentType: " + to_string(static_cast<int>(contentType));

		_logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }

    string encodingProfilePathName(_profilesRootRepository);

    encodingProfilePathName
        .append(to_string(llEncodingProfileKey));

    if (contentType == MMSEngineDBFacade::ContentType::Video)
    {
        encodingProfilePathName.append(".vep");
    } 
    else if (contentType == MMSEngineDBFacade::ContentType::Audio)
    {
        encodingProfilePathName.append(".aep");
    } 
    else if (contentType == MMSEngineDBFacade::ContentType::Image)
    {
        encodingProfilePathName.append(".iep");
    }


    return encodingProfilePathName;
}
*/

string MMSStorage::creatingDirsUsingTerritories(
        unsigned long ulCurrentMMSPartitionIndex,
        string relativePath,
        string workspaceDirectoryName,
        bool deliveryRepositoriesToo,
        Workspace::TerritoriesHashMap& phmTerritories)
{

    char pMMSPartitionName [64];


    sprintf(pMMSPartitionName, "MMS_%04lu/", ulCurrentMMSPartitionIndex);

    string mmsAssetPathName(MMSStorage::getMMSRootRepository(_storage));
    mmsAssetPathName
        .append(pMMSPartitionName)
        .append(workspaceDirectoryName)
        .append(relativePath);

    if (!FileIO::directoryExisting(mmsAssetPathName)) 
    {
        _logger->info(__FILEREF__ + "Create directory"
            + ", mmsAssetPathName: " + mmsAssetPathName
        );

        bool noErrorIfExists = true;
        bool recursive = true;
        FileIO::createDirectory(mmsAssetPathName,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP |
                S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    if (mmsAssetPathName.size() > 0 && mmsAssetPathName.back() != '/')
        mmsAssetPathName.append("/");

	/*
	 * commented because currently we do not have territories 
    if (deliveryRepositoriesToo) 
    {
        Workspace::TerritoriesHashMap::iterator it;


        for (it = phmTerritories.begin(); it != phmTerritories.end(); ++it) 
        {
            string territoryName = it->second;

            string downloadAssetPathName(_downloadRootRepository);
            downloadAssetPathName
                .append(pMMSPartitionName)
                .append(workspaceDirectoryName)
                .append("/")
                .append(territoryName)
                .append(relativePath);

            string streamingAssetPathName(_streamingRootRepository);
            streamingAssetPathName
                .append(pMMSPartitionName)
                .append(workspaceDirectoryName)
                .append("/")
                .append(territoryName)
                .append(relativePath);

            if (!FileIO::directoryExisting(downloadAssetPathName)) 
            {
                _logger->info(__FILEREF__ + "Create directory"
                    + ", downloadAssetPathName: " + downloadAssetPathName
                );
                
                bool noErrorIfExists = true;
                bool recursive = true;
                FileIO::createDirectory(downloadAssetPathName,
                        S_IRUSR | S_IWUSR | S_IXUSR |
                        S_IRGRP | S_IXGRP |
                        S_IROTH | S_IXOTH, noErrorIfExists, recursive);
            }

            if (!FileIO::directoryExisting(streamingAssetPathName)) 
            {
                _logger->info(__FILEREF__ + "Create directory"
                    + ", streamingAssetPathName: " + streamingAssetPathName
                );

                bool noErrorIfExists = true;
                bool recursive = true;
                FileIO::createDirectory(streamingAssetPathName,
                        S_IRUSR | S_IWUSR | S_IXUSR |
                        S_IRGRP | S_IXGRP |
                        S_IROTH | S_IXOTH, noErrorIfExists, recursive);
            }
        }
    }
	*/


    return mmsAssetPathName;
}

void MMSStorage::removePhysicalPath(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, int64_t physicalPathKey)
{

    try
    {
        _logger->info(__FILEREF__ + "getStorageDetailsByPhysicalPathKey ..."
            + ", physicalPathKey: " + to_string(physicalPathKey)
        );
        
        tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int,shared_ptr<Workspace>,string,string,
			string,string,int64_t, bool> storageDetails =
            mmsEngineDBFacade->getStorageDetails(physicalPathKey);

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

        mmsEngineDBFacade->removePhysicalPath(physicalPathKey);
    }
    catch(MediaItemKeyNotFound e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", e.what(): " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;
        
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }    
}

void MMSStorage::removeMediaItem(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, int64_t mediaItemKey)
{
    try
    {
        _logger->info(__FILEREF__ + "getAllStorageDetails ..."
            + ", mediaItemKey: " + to_string(mediaItemKey)
        );

        vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>>
			allStorageDetails;
        mmsEngineDBFacade->getAllStorageDetails(mediaItemKey, allStorageDetails);

        for (tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>&
				storageDetails: allStorageDetails)
        {
			MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
			bool externalReadOnlyStorage;
			int64_t sizeInBytes;

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
        mmsEngineDBFacade->removeMediaItem(mediaItemKey);
    }
    catch(runtime_error e)
    {
        string errorMessage = string("removeMediaItem failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", exception: " + e.what()
        ;
        
        _logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
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
	int mmsPartitionNumber,
	string workspaceDirectoryName,
	string relativePath,
	int64_t sizeInBytes
		)
{
    try
    {
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
				+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
				+ ", workspaceDirectoryName: " + workspaceDirectoryName
				+ ", relativePath: " + relativePath
				+ ", fileName: " + fileName
			);
			string mmsAssetPathName = getMMSAssetPathName(
				externalReadOnlyStorage,
				mmsPartitionNumber,
				workspaceDirectoryName,
				relativePath,
				fileName);

			FileIO::DirectoryEntryType_t detSourceFileType;
			bool fileExist = true;

			try
			{
				detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);
			}
			catch(FileNotExisting fne)
			{
				string errorMessage = string("file/directory not present")
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", mmsAssetPathName: " + mmsAssetPathName
				;
      
				_logger->warn(__FILEREF__ + errorMessage);

				fileExist = false;
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + e.what());

				throw e;
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "...exception");

				throw e;
			}

			if (fileExist)
			{
				if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
				{
					try
					{
						_logger->info(__FILEREF__ + "Remove directory"
							+ ", mmsAssetPathName: " + mmsAssetPathName
						);
						bool removeRecursively = true;
						FileIO::removeDirectory(mmsAssetPathName, removeRecursively);
					}
					catch(DirectoryNotExisting dne)
					{
						string errorMessage = string("removeDirectory failed. directory not present")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName
						;
       
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error e)
					{
						string errorMessage = string("removeDirectory failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName
							+ ", exception: " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}
					catch(exception e)
					{
						string errorMessage = string("removeDirectory failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}

					{
						lock_guard<recursive_mutex> locker(_mtMMSPartitions);

						PartitionInfo& partitionInfo = _mmsPartitionsInfo.at(mmsPartitionNumber);

						partitionInfo._currentFreeSizeInBytes		+= sizeInBytes;

						_logger->info(__FILEREF__ + "Partition free size info"
							+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
							+ ", _currentFreeSizeInBytes: "
								+ to_string(partitionInfo._currentFreeSizeInBytes)
						);
					}
				} 
				else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
				{
					try
					{
						_logger->info(__FILEREF__ + "Remove file"
							+ ", mmsAssetPathName: " + mmsAssetPathName
						);
						FileIO::remove(mmsAssetPathName);
					}
					catch(FileNotExisting fne)
					{
						string errorMessage = string("removefailed, file not present")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName
						;
       
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error e)
					{
						string errorMessage = string("remove failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName
							+ ", exception: " + e.what()
						;
       
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}
					catch(exception e)
					{
						string errorMessage = string("remove failed")
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", physicalPathKey: " + to_string(physicalPathKey)
							+ ", mmsAssetPathName: " + mmsAssetPathName
						;
       
						_logger->error(__FILEREF__ + errorMessage);

						throw e;
					}

					{
						lock_guard<recursive_mutex> locker(_mtMMSPartitions);

						PartitionInfo& partitionInfo = _mmsPartitionsInfo.at(mmsPartitionNumber);

						partitionInfo._currentFreeSizeInBytes		+= sizeInBytes;

						_logger->info(__FILEREF__ + "Partition free size info"
							+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
							+ ", _currentFreeSizeInBytes: "
								+ to_string(partitionInfo._currentFreeSizeInBytes)
						);
					}
				} 
				else 
				{
					string errorMessage = string("Unexpected directory entry")
                           + ", detSourceFileType: " + to_string(detSourceFileType);

					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("removePhysicalPathFile failed")
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", exception: " + e.what()
		;

		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch(exception e)
	{
		string errorMessage = string("removePhysicalPathFile failed")
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", physicalPathKey: " + to_string(physicalPathKey)
		;

		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

/*
void MMSStorage::moveContentInRepository(
        string filePathName,
        RepositoryType rtRepositoryType,
        string workspaceDirectoryName,
        bool addDateTimeToFileName)
{

    contentInRepository(
        1,
        filePathName,
        rtRepositoryType,
        workspaceDirectoryName,
        addDateTimeToFileName);
}

void MMSStorage::copyFileInRepository(
        string filePathName,
        RepositoryType rtRepositoryType,
        string workspaceDirectoryName,
        bool addDateTimeToFileName)
{

    contentInRepository(
        0,
        filePathName,
        rtRepositoryType,
        workspaceDirectoryName,
        addDateTimeToFileName);
}

void MMSStorage::contentInRepository(
        unsigned long ulIsCopyOrMove,
        string contentPathName,
        RepositoryType rtRepositoryType,
        string workspaceDirectoryName,
        bool addDateTimeToFileName)
{

    tm tmDateTime;
    unsigned long ulMilliSecs;
    FileIO::DirectoryEntryType_t detSourceFileType;


    // pDestRepository includes the '/' at the end
    string metaDataFileInDestRepository(getRepository(rtRepositoryType));
    metaDataFileInDestRepository
        .append(workspaceDirectoryName)
        .append("/");

    DateTime::get_tm_LocalTime(&tmDateTime, &ulMilliSecs);

    if (rtRepositoryType == RepositoryType::MMSREP_REPOSITORYTYPE_STAGING) 
    {
        char pDateTime [64];
        bool directoryExisting;


        sprintf(pDateTime,
                "%04lu_%02lu_%02lu",
                (unsigned long) (tmDateTime. tm_year + 1900),
                (unsigned long) (tmDateTime. tm_mon + 1),
                (unsigned long) (tmDateTime. tm_mday));

        metaDataFileInDestRepository.append(pDateTime);

        if (!FileIO::directoryExisting(metaDataFileInDestRepository)) 
        {
            _logger->info(__FILEREF__ + "Create directory"
                + ", metaDataFileInDestRepository: " + metaDataFileInDestRepository
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(metaDataFileInDestRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }

        metaDataFileInDestRepository.append("/");
    }

    if (addDateTimeToFileName) 
    {
        char pDateTime [64];


        sprintf(pDateTime,
                "%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu_",
                (unsigned long) (tmDateTime. tm_year + 1900),
                (unsigned long) (tmDateTime. tm_mon + 1),
                (unsigned long) (tmDateTime. tm_mday),
                (unsigned long) (tmDateTime. tm_hour),
                (unsigned long) (tmDateTime. tm_min),
                (unsigned long) (tmDateTime. tm_sec),
                ulMilliSecs);

        metaDataFileInDestRepository.append(pDateTime);
    }

    size_t fileNameStart;
    string fileName;
    if ((fileNameStart = contentPathName.find_last_of('/')) == string::npos)
        fileName = contentPathName;
    else
        fileName = contentPathName.substr(fileNameStart + 1);

    metaDataFileInDestRepository.append(fileName);

    // file in case of .3gp content OR
    // directory in case of IPhone content
    detSourceFileType = FileIO::getDirectoryEntryType(contentPathName);

    if (ulIsCopyOrMove == 1) 
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            _logger->info(__FILEREF__ + "Move directory"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::moveDirectory(contentPathName,
                    metaDataFileInDestRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
        } 
        else // if (detSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE
        {
            _logger->info(__FILEREF__ + "Move file"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::moveFile(contentPathName, metaDataFileInDestRepository);
        }
    } 
    else 
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            _logger->info(__FILEREF__ + "Copy directory"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::copyDirectory(contentPathName,
                    metaDataFileInDestRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
        } 
        else 
        {
            _logger->info(__FILEREF__ + "Copy file"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::copyFile(contentPathName,
                    metaDataFileInDestRepository);
        }
    }
}
*/

string MMSStorage::moveAssetInMMSRepository(
        string sourceAssetPathName,
        string workspaceDirectoryName,
        string destinationAssetFileName,
        string relativePath,

        bool partitionIndexToBeCalculated,
        unsigned long *pulMMSPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

        bool deliveryRepositoriesToo,
        Workspace::TerritoriesHashMap& phmTerritories
        )
{
    FileIO::DirectoryEntryType_t detSourceFileType;

    if ((relativePath.size() > 0 && relativePath.front() != '/')
			|| pulMMSPartitionIndexUsed == (unsigned long *) NULL) 
    {
		string errorMessage = string("Wrong argument")                                                          
            + ", relativePath: " + relativePath;

        _logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
    }

    lock_guard<recursive_mutex> locker(_mtMMSPartitions);

    // file in case of .3gp content OR
    // directory in case of IPhone content
    detSourceFileType = FileIO::getDirectoryEntryType(sourceAssetPathName);

    if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
            detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
    {
        _logger->error(__FILEREF__ + "Wrong directory entry type");

        throw runtime_error("Wrong directory entry type");
    }

	unsigned long long ullFSEntrySizeInBytes;
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            ullFSEntrySizeInBytes = FileIO::getDirectorySizeInBytes(sourceAssetPathName);
        } 
        else // if (detSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE)
        {
            unsigned long ulFileSizeInBytes;
            bool inCaseOfLinkHasItToBeRead = false;


            ulFileSizeInBytes = FileIO::getFileSizeInBytes(sourceAssetPathName, inCaseOfLinkHasItToBeRead);

            ullFSEntrySizeInBytes = ulFileSizeInBytes;
        }
	}

    if (partitionIndexToBeCalculated) 
    {
        // find the MMS partition index
        unsigned long ulMMSPartitionIndex;
        for (ulMMSPartitionIndex = 0;
                ulMMSPartitionIndex < _mmsPartitionsInfo.size();
                ulMMSPartitionIndex++) 
        {
            int64_t mmsPartitionsFreeSizeInKB = (int64_t)
                ((_mmsPartitionsInfo[_ulCurrentMMSPartitionIndex])._currentFreeSizeInBytes / 1000);

			int localFreeSpaceToLeaveInEachPartitionInMB;
			{
				char pMMSPartitionName [64];
				sprintf(pMMSPartitionName, "%04lu", _ulCurrentMMSPartitionIndex);
				string freeSpaceConfField = string("freeSpaceToLeaveInEachPartitionInMB_")
					+ pMMSPartitionName;

				if (JSONUtils::isMetadataPresent(_configuration["storage"], freeSpaceConfField))
					localFreeSpaceToLeaveInEachPartitionInMB = JSONUtils::asInt(
							_configuration["storage"], freeSpaceConfField, 100);
				else
					localFreeSpaceToLeaveInEachPartitionInMB = _freeSpaceToLeaveInEachPartitionInMB;
				_logger->info(__FILEREF__ + "FreeSpaceToLeaveInEachPartitionInMB"
					+ ", freeSpaceConfField: " + freeSpaceConfField
					+ ", _freeSpaceToLeaveInEachPartitionInMB: "
						+ to_string(_freeSpaceToLeaveInEachPartitionInMB)
					+ ", localFreeSpaceToLeaveInEachPartitionInMB: "
						+ to_string(localFreeSpaceToLeaveInEachPartitionInMB)
				);
			}

            if (mmsPartitionsFreeSizeInKB <=
                    (localFreeSpaceToLeaveInEachPartitionInMB * 1000)) 
            {
                _logger->info(__FILEREF__ + "Partition space too low"
                    + ", _ulCurrentMMSPartitionIndex: " + to_string(_ulCurrentMMSPartitionIndex)
                    + ", mmsPartitionsFreeSizeInKB: " + to_string(mmsPartitionsFreeSizeInKB)
                    + ", localFreeSpaceToLeaveInEachPartitionInMB * 1000: " + to_string(localFreeSpaceToLeaveInEachPartitionInMB * 1000)
                );

                if (_ulCurrentMMSPartitionIndex + 1 >= _mmsPartitionsInfo.size())
                    _ulCurrentMMSPartitionIndex = 0;
                else
                    _ulCurrentMMSPartitionIndex++;

                continue;
            }

            if ((unsigned long long) (mmsPartitionsFreeSizeInKB -
                    (localFreeSpaceToLeaveInEachPartitionInMB * 1000)) >
                    (ullFSEntrySizeInBytes / 1000)) 
            {
                break;
            }

            if (_ulCurrentMMSPartitionIndex + 1 >= _mmsPartitionsInfo.size())
                _ulCurrentMMSPartitionIndex = 0;
            else
                _ulCurrentMMSPartitionIndex++;
        }

        if (ulMMSPartitionIndex == _mmsPartitionsInfo.size()) 
        {
            string errorMessage = string("No more space in MMS Partitions")
                    + ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
                    ;
            for (ulMMSPartitionIndex = 0;
                ulMMSPartitionIndex < _mmsPartitionsInfo.size();
                ulMMSPartitionIndex++) 
            {
                errorMessage +=
                    (", _mmsPartitionsInfo [" + to_string(ulMMSPartitionIndex) + "]: "
					+ to_string((_mmsPartitionsInfo[ulMMSPartitionIndex])._currentFreeSizeInBytes))
                    ;
            }

            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }

        *pulMMSPartitionIndexUsed = _ulCurrentMMSPartitionIndex;
    }

    // creating directories and build the bMMSAssetPathName
    string mmsAssetPathName;
    {
        // to create the content provider directory and the
        // territories directories (if not already existing)
        mmsAssetPathName = creatingDirsUsingTerritories(*pulMMSPartitionIndexUsed,
            relativePath, workspaceDirectoryName, deliveryRepositoriesToo,
            phmTerritories);

        mmsAssetPathName.append(destinationAssetFileName);
    }

    _logger->info(__FILEREF__ + "Selected MMS Partition for the content"
        + ", workspaceDirectoryName: " + workspaceDirectoryName
        + ", *pulMMSPartitionIndexUsed: " + to_string(*pulMMSPartitionIndexUsed)
        + ", mmsAssetPathName: " + mmsAssetPathName
        + ", _mmsPartitionsInfo[_ulCurrentMMSPartitionIndex]: "
			+ to_string((_mmsPartitionsInfo[_ulCurrentMMSPartitionIndex])._currentFreeSizeInBytes)
    );

    // move the file in case of .3gp content OR
    // move the directory in case of IPhone content
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
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
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
            );
			chrono::system_clock::time_point startPoint = chrono::system_clock::now();
            FileIO::copyDirectory(sourceAssetPathName,
                    mmsAssetPathName,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
			chrono::system_clock::time_point endPoint = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "Copy directory statistics"
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
				+ ", @MMS COPY statistics@ - elapsed (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
			);

			try
			{
				_logger->info(__FILEREF__ + "Remove directory"
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);
				bool bRemoveRecursively = true;
				FileIO::removeDirectory(sourceAssetPathName, bRemoveRecursively);
			}
			catch(runtime_error e)
			{
				// we will not raise an exception, it is a staging directory,
				// it will be removed by cronjob (see the comment above)
				_logger->error(__FILEREF__ + "FileIO::removeDirectory failed"
						+ ", sourceAssetPathName: " + sourceAssetPathName
						+ ", e.what(): " + e.what()
				);
			}
        }
        else // if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_REGULARFILE)
        {
            _logger->info(__FILEREF__ + "Move file"
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
            );

			chrono::system_clock::time_point startPoint = chrono::system_clock::now();
            FileIO::moveFile(sourceAssetPathName, mmsAssetPathName);
			chrono::system_clock::time_point endPoint = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "Move file statistics"
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
				+ ", @MMS MOVE statistics@ - elapsed (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
			);
        }
    }

    // update _pullMMSPartitionsFreeSizeInMB ONLY if bIsPartitionIndexToBeCalculated
	// 2019-10-19: why I should update PartitionInfo ONLY if bIsPartitionIndexToBeCalculated?
	//	I do it always
    // if (partitionIndexToBeCalculated) 
    {
		PartitionInfo& partitionInfo = _mmsPartitionsInfo.at(_ulCurrentMMSPartitionIndex);

		partitionInfo._currentFreeSizeInBytes			-= ullFSEntrySizeInBytes;

		_logger->info(__FILEREF__ + "Partition free size info"
			+ ", mmsPartitionNumber: " + to_string(_ulCurrentMMSPartitionIndex)
			+ ", _currentFreeSizeInBytes: " + to_string(partitionInfo._currentFreeSizeInBytes)
		);
    }


    return mmsAssetPathName;
}

void MMSStorage::deleteWorkspace(
		shared_ptr<Workspace> workspace)
{
	{
		string workspaceIngestionDirectory = MMSStorage::getIngestionRootRepository(_storage);
		workspaceIngestionDirectory.append(workspace->_directoryName);

        if (FileIO::directoryExisting(workspaceIngestionDirectory))
        {
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", workspaceIngestionDirectory: " + workspaceIngestionDirectory
			);
			bool removeRecursively = true;
			FileIO::removeDirectory(workspaceIngestionDirectory, removeRecursively);
        }
	}

	{
		lock_guard<recursive_mutex> locker(_mtMMSPartitions);

		for (unsigned long ulMMSPartitionIndex = 0;
            ulMMSPartitionIndex < _mmsPartitionsInfo.size();
            ulMMSPartitionIndex++) 
		{
			string workspacePathName = getMMSAssetPathName(
				false,	// externalReadOnlyStorage
				ulMMSPartitionIndex,
				workspace->_directoryName,
                string(""),		// relativePath
				string("")		// fileName
				);

			if (FileIO::directoryExisting(workspacePathName))
			{
				int64_t directorySizeInBytes = FileIO::getDirectorySizeInBytes(workspacePathName);

				_logger->info(__FILEREF__ + "Remove directory"
					+ ", workspacePathName: " + workspacePathName
				);
				bool removeRecursively = true;
				FileIO::removeDirectory(workspacePathName, removeRecursively);

				{
					PartitionInfo& partitionInfo = _mmsPartitionsInfo.at(ulMMSPartitionIndex);

					partitionInfo._currentFreeSizeInBytes			+= directorySizeInBytes;

					_logger->info(__FILEREF__ + "Partition free size info"
						+ ", mmsPartitionNumber: " + to_string(ulMMSPartitionIndex)
						+ ", _currentFreeSizeInBytes: "
							+ to_string(partitionInfo._currentFreeSizeInBytes)
					);
				}
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


    lock_guard<recursive_mutex> locker(_mtMMSPartitions);

    ullWorkspaceStorageUsageInBytes = 0;

    for (ulMMSPartitionIndex = 0;
            ulMMSPartitionIndex < _mmsPartitionsInfo.size();
            ulMMSPartitionIndex++) 
    {
		string workspacePathName = getMMSAssetPathName(
			false,	// externalReadOnlyStorage
			ulMMSPartitionIndex,
			workspaceDirectoryName,
			string(""),		// relativePath
			string("")		// fileName
		);

		if (FileIO::directoryExisting(workspacePathName))
		{
			try
			{
				ullDirectoryUsageInBytes = FileIO::getDirectorySizeInBytes(workspacePathName);
			}
			catch(runtime_error e)
			{
				ullDirectoryUsageInBytes		= 0;

				_logger->error(__FILEREF__ + "FileIO::getDirectorySizeInBytes failed"
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

	lock_guard<recursive_mutex> locker(_mtMMSPartitions);

	for (unsigned long ulMMSPartitionIndex = 0;
		ulMMSPartitionIndex < _mmsPartitionsInfo.size();
		ulMMSPartitionIndex++) 
	{
		try
		{
			refreshPartitionFreeSizes(_mmsPartitionsInfo.at(ulMMSPartitionIndex)); 
		}
		catch(runtime_error e)
		{
			_logger->error(__FILEREF__ + "refreshPartitionFreeSizes failed"
				+ ", e.what(): " + e.what()
			);
		}
		catch(exception e)
		{
			_logger->error(__FILEREF__ + "refreshPartitionFreeSizes failed"
				+ ", e.what(): " + e.what()
			);
		}
	}
}

void MMSStorage::refreshPartitionFreeSizes(PartitionInfo& partitionInfo) 
{

    int64_t usedInBytes;
    int64_t availableInBytes;

	// lock has to be already done
    // lock_guard<recursive_mutex> locker(_mtMMSPartitions);

	_logger->info(__FILEREF__ + "refreshPartitionFreeSizes (before)"
			+ ", _partitionPathName: " + partitionInfo._partitionPathName
			+ ", _partitionUsageType: " + partitionInfo._partitionUsageType
			+ ", _maxStorageUsageInKB: " + to_string(partitionInfo._maxStorageUsageInKB)
			+ ", _currentFreeSizeInBytes: " + to_string(partitionInfo._currentFreeSizeInBytes)
			+ ", _lastUpdateFreeSize: " + to_string(chrono::system_clock::to_time_t(partitionInfo._lastUpdateFreeSize))
	);

	chrono::system_clock::time_point startPoint = chrono::system_clock::now();

	if (partitionInfo._partitionUsageType == "getDirectoryUsage"
		&& partitionInfo._maxStorageUsageInKB != -1)
	{
		// ullUsedInKB
		{
			chrono::system_clock::time_point startPoint_getDirectoryUsage = chrono::system_clock::now();

			try
			{
				usedInBytes = FileIO::getDirectorySizeInBytes(partitionInfo._partitionPathName);
			}
			catch(runtime_error e)
			{
				usedInBytes		= 0;

				_logger->error(__FILEREF__ + "FileIO::getDirectorySizeInBytes failed"
					+ ", e.what(): " + e.what()
				);
			}
			catch(exception e)
			{
				usedInBytes		= 0;

				_logger->error(__FILEREF__ + "FileIO::getDirectorySizeInBytes failed"
				);
			}

			chrono::system_clock::time_point endPoint_getDirectoryUsage = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "getDirectoryUsage statistics"
				+ ", usedInBytes: " + to_string(usedInBytes)
				+ ", @MMS statistics@ - elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endPoint_getDirectoryUsage - startPoint_getDirectoryUsage).count()) + "@"
			);
		}

		// ullAvailableInBytes;
		{
			if (partitionInfo._maxStorageUsageInKB * 1000 > usedInBytes)
				availableInBytes = (partitionInfo._maxStorageUsageInKB * 1000) - usedInBytes;
			else
				availableInBytes = 0;
		}
	}
	else
	{
		long lPercentUsed;

		FileIO::getFileSystemInfo(partitionInfo._partitionPathName,
			&usedInBytes, &availableInBytes, &lPercentUsed);

		// ullUsedInBytes
		if (partitionInfo._partitionUsageType == "getDirectoryUsage")
		{
			chrono::system_clock::time_point startPoint_getDirectoryUsage = chrono::system_clock::now();

			try
			{
				usedInBytes = FileIO::getDirectorySizeInBytes(partitionInfo._partitionPathName);
			}
			catch(runtime_error e)
			{
				usedInBytes		= 0;

				_logger->error(__FILEREF__ + "FileIO::getDirectorySizeInBytes failed"
					+ ", e.what(): " + e.what()
				);
			}
			catch(exception e)
			{
				usedInBytes		= 0;

				_logger->error(__FILEREF__ + "FileIO::getDirectorySizeInBytes failed"
				);
			}

			chrono::system_clock::time_point endPoint_getDirectoryUsage = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "getDirectoryUsage statistics"
				+ ", usedInBytes: " + to_string(usedInBytes)
				+ ", @MMS statistics@ - elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endPoint_getDirectoryUsage - startPoint_getDirectoryUsage).count()) + "@"
			);
		}

		// ullAvailableInBytes;
		if (partitionInfo._maxStorageUsageInKB != -1)
		{
			if (partitionInfo._maxStorageUsageInKB * 1000 > usedInBytes)
				availableInBytes = (partitionInfo._maxStorageUsageInKB * 1000) - usedInBytes;
			else
				availableInBytes = 0;
		}
	}

	partitionInfo._currentFreeSizeInBytes	= availableInBytes;
	partitionInfo._lastUpdateFreeSize		= chrono::system_clock::now();

	chrono::system_clock::time_point endPoint = chrono::system_clock::now();                              

	_logger->info(__FILEREF__ + "refreshPartitionFreeSizes (after)"
			+ ", _partitionPathName: " + partitionInfo._partitionPathName
			+ ", _partitionUsageType: " + partitionInfo._partitionUsageType
			+ ", _maxStorageUsageInKB: " + to_string(partitionInfo._maxStorageUsageInKB)
			+ ", _currentFreeSizeInBytes: " + to_string(partitionInfo._currentFreeSizeInBytes)
			+ ", _lastUpdateFreeSize: " + to_string(chrono::system_clock::to_time_t(partitionInfo._lastUpdateFreeSize))
			+ ", @MMS statistics@ - elapsed (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
	);
}

