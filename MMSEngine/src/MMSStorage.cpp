
#include <fstream>
#include "MMSStorage.h"
#include "JSONUtils.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/System.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/ProcessUtility.h"

MMSStorage::MMSStorage(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger) 
{

	try
	{
		_mmsEngineDBFacade	= mmsEngineDBFacade;
		_logger             = logger;
		_configuration		= configuration;

		_hostName = System::getHostName();

		_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(configuration["storage"],
			"waitingNFSSync_maxMillisecondsToWait", 60000);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", storage->_waitingNFSSync_maxMillisecondsToWait: " + to_string(_waitingNFSSync_maxMillisecondsToWait)
		);

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

		MMSStorage::createDirectories(configuration, _logger);

		refreshPartitionsFreeSizes() ;
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
	MMSStorage::getPhysicalPathDetails(
		int64_t mediaItemKey, int64_t encodingProfileKey,
		bool warningIfMissing)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
				string, string, string, string, int64_t, bool>
			storageDetails = _mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey,
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
			fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage)
			= storageDetails;

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
	int64_t physicalPathKey)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
			string, string, string, string, int64_t, bool> storageDetails =
			_mmsEngineDBFacade->getStorageDetails(physicalPathKey);

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

tuple<string, int, string, string> MMSStorage::getVODDeliveryURI(
	int64_t physicalPathKey, bool save, shared_ptr<Workspace> requestWorkspace)
{
    try
    {
		tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
			string, string, string, string, int64_t, bool> storageDetails =
			_mmsEngineDBFacade->getStorageDetails(physicalPathKey);

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

		return make_tuple(title, mmsPartitionNumber, physicalPathKey,
			deliveryFileName, deliveryURI);
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
		string directoryId,
		string liveFileExtension, shared_ptr<Workspace> requestWorkspace)
{
	tuple<string, string, string> liveDeliveryDetails = getLiveDeliveryDetails(
			directoryId,
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
	string directoryId, shared_ptr<Workspace> requestWorkspace)
{
	string liveFileExtension = "xxx";

	tuple<string, string, string> liveDeliveryDetails = getLiveDeliveryDetails(
		directoryId,
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
        int partitionKey,
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
		string partitionPathName = _mmsEngineDBFacade->getPartitionPathName(partitionKey);
		assetPathName =
			partitionPathName
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

void MMSStorage::removePhysicalPath(int64_t physicalPathKey)
{

    try
    {
        _logger->info(__FILEREF__ + "getStorageDetailsByPhysicalPathKey ..."
            + ", physicalPathKey: " + to_string(physicalPathKey)
        );
        
        tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int,shared_ptr<Workspace>,
			string,string, string,string,int64_t, bool> storageDetails =
            _mmsEngineDBFacade->getStorageDetails(physicalPathKey);

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

        _mmsEngineDBFacade->removePhysicalPath(physicalPathKey);
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

void MMSStorage::removeMediaItem(int64_t mediaItemKey)
{
    try
    {
        _logger->info(__FILEREF__ + "getAllStorageDetails ..."
            + ", mediaItemKey: " + to_string(mediaItemKey)
        );

        vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string,
			int64_t, bool>> allStorageDetails;
        _mmsEngineDBFacade->getAllStorageDetails(mediaItemKey, allStorageDetails);

        for (tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string,
				int64_t, bool>& storageDetails: allStorageDetails)
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
        _mmsEngineDBFacade->removeMediaItem(mediaItemKey);
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
	int partitionKey,
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
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", workspaceDirectoryName: " + workspaceDirectoryName
				+ ", relativePath: " + relativePath
				+ ", fileName: " + fileName
			);
			string mmsAssetPathName = getMMSAssetPathName(
				externalReadOnlyStorage,
				partitionKey,
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

					uint64_t newCurrentFreeSizeInBytes =
						_mmsEngineDBFacade->updatePartitionBecauseOfDeletion(partitionKey,
								sizeInBytes);
					_logger->info(__FILEREF__ + "updatePartitionBecauseOfDeletion"
						+ ", partitionKey: " + to_string(partitionKey)
						+ ", newCurrentFreeSizeInBytes: " + to_string(newCurrentFreeSizeInBytes)
					);
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
	int64_t ingestionJobKey,
	string sourceAssetPathName,
	string workspaceDirectoryName,
	string destinationAssetFileName,
	string relativePath,

	unsigned long *pulMMSPartitionIndexUsed, // OUT
    FileIO::DirectoryEntryType_p pSourceFileType,	// OUT: TOOLS_FILEIO_DIRECTORY or TOOLS_FILEIO_REGULARFILE

	bool deliveryRepositoriesToo,
	Workspace::TerritoriesHashMap& phmTerritories
)
{

    if ((relativePath.size() > 0 && relativePath.front() != '/')
			|| pulMMSPartitionIndexUsed == (unsigned long *) NULL) 
    {
		string errorMessage = string("Wrong argument")                                                          
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", relativePath: " + relativePath;

        _logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
    }

    // file in case of .3gp content OR
    // directory in case of IPhone content
    *pSourceFileType = FileIO::getDirectoryEntryType(sourceAssetPathName);

    if (*pSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
            *pSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
    {
        _logger->error(__FILEREF__ + "Wrong directory entry type");

        throw runtime_error("Wrong directory entry type");
    }

	unsigned long long ullFSEntrySizeInBytes;
    {
        if (*pSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            ullFSEntrySizeInBytes = FileIO::getDirectorySizeInBytes(sourceAssetPathName);
        } 
        else // if (*pSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE)
        {
            unsigned long ulFileSizeInBytes;
            bool inCaseOfLinkHasItToBeRead = false;


            ulFileSizeInBytes = FileIO::getFileSizeInBytes(sourceAssetPathName, inCaseOfLinkHasItToBeRead);

            ullFSEntrySizeInBytes = ulFileSizeInBytes;
        }
	}

    {
		int partitionKey;
		uint64_t newCurrentFreeSizeInBytes;

		pair<int, uint64_t> partitionDetails = _mmsEngineDBFacade
			->getPartitionToBeUsedAndUpdateFreeSpace(ullFSEntrySizeInBytes);
		tie(partitionKey, newCurrentFreeSizeInBytes) = partitionDetails;

		_logger->info(__FILEREF__ + "getPartitionToBeUsedAndUpdateFreeSpace"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
			+ ", partitionKey: " + to_string(partitionKey)
			+ ", newCurrentFreeSizeInBytes: " + to_string(newCurrentFreeSizeInBytes)
		);

		*pulMMSPartitionIndexUsed = partitionKey;
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
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", workspaceDirectoryName: " + workspaceDirectoryName
        + ", *pulMMSPartitionIndexUsed: " + to_string(*pulMMSPartitionIndexUsed)
        + ", mmsAssetPathName: " + mmsAssetPathName
        + ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
    );

    // move the file in case of .3gp content OR
    // move the directory in case of IPhone content
    {
        if (*pSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
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
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
				+ ", @MMS COPY statistics@ - elapsed (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
			);

			try
			{
				_logger->info(__FILEREF__ + "Remove directory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
					+ ", e.what(): " + e.what()
				);
			}
        }
        else // if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_REGULARFILE)
        {
            _logger->info(__FILEREF__ + "Move file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
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
			chrono::system_clock::time_point startPoint;
			chrono::system_clock::time_point endPoint;
			try
			{
				startPoint = chrono::system_clock::now();
				FileIO::moveFile(sourceAssetPathName, mmsAssetPathName);
				endPoint = chrono::system_clock::now();
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "Move file failed, wait a bit, retrieve again the size and try again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", from: " + sourceAssetPathName
					+ ", to: " + mmsAssetPathName
					+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
					+ ", _waitingNFSSync_maxMillisecondsToWait: " + to_string(_waitingNFSSync_maxMillisecondsToWait)
					+ ", e.what: " + e.what()
				);

				// scenario of the above comment marked as 2021-09-05
				this_thread::sleep_for(chrono::milliseconds(
					_waitingNFSSync_maxMillisecondsToWait));

				{
					unsigned long ulFileSizeInBytes;
					bool inCaseOfLinkHasItToBeRead = false;

					ulFileSizeInBytes = FileIO::getFileSizeInBytes(
						sourceAssetPathName, inCaseOfLinkHasItToBeRead);

					ullFSEntrySizeInBytes = ulFileSizeInBytes;
				}

				_logger->info(__FILEREF__ + "Move file again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", from: " + sourceAssetPathName
					+ ", to: " + mmsAssetPathName
					+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
				);

				startPoint = chrono::system_clock::now();
				FileIO::moveFile(sourceAssetPathName, mmsAssetPathName);
				endPoint = chrono::system_clock::now();
			}

            unsigned long ulDestFileSizeInBytes;
			{
				bool inCaseOfLinkHasItToBeRead = false;
				ulDestFileSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName, inCaseOfLinkHasItToBeRead);
			}

			_logger->info(__FILEREF__ + "Move file statistics"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
                + ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
                + ", ulDestFileSizeInBytes: " + to_string(ulDestFileSizeInBytes)
				+ ", @MMS MOVE statistics@ - elapsed (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
			);

			if (ullFSEntrySizeInBytes != ulDestFileSizeInBytes)
			{
				string errorMessage = string("Source and destination file have different sizes")
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", source: " + sourceAssetPathName
					+ ", dest: " + mmsAssetPathName
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
		string liveRootDirectory = MMSStorage::getLiveRootRepository(_storage);
		liveRootDirectory.append(workspace->_directoryName);

        if (FileIO::directoryExisting(liveRootDirectory))
        {
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", liveRootDirectory: " + liveRootDirectory
			);
			bool removeRecursively = true;
			FileIO::removeDirectory(liveRootDirectory, removeRecursively);
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

			string workspacePathName = getMMSAssetPathName(
				false,	// externalReadOnlyStorage
				partitionKey,
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


    ullWorkspaceStorageUsageInBytes = 0;

	vector<pair<int, uint64_t>> partitionsInfo;

	_mmsEngineDBFacade->getPartitionsInfo(partitionsInfo);

	for (pair<int, uint64_t> partitionInfo: partitionsInfo)
    {
		int partitionKey;
		uint64_t currentFreeSizeInBytes;

		tie(partitionKey, currentFreeSizeInBytes) = partitionInfo;

		string workspacePathName = getMMSAssetPathName(
			false,	// externalReadOnlyStorage
			partitionKey,
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
	int partitionKey = 0;
	bool mmsAvailablePartitions = true;

	while (mmsAvailablePartitions) 
	{
		string partitionPathName;
		{
			char pMMSPartitionName [64];

			sprintf(pMMSPartitionName, "MMS_%04d", partitionKey);

			partitionPathName = MMSStorage::getMMSRootRepository(_storage);
			partitionPathName.append(pMMSPartitionName);
		}

		if (!FileIO::directoryExisting(partitionPathName))
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

			FileIO::getFileSystemInfo(partitionPathName,
				&usedInBytes, &availableInBytes, &lPercentUsed);

			currentFreeSizeInBytes = availableInBytes;

			chrono::system_clock::time_point endPoint = chrono::system_clock::now();                              
			_logger->info(__FILEREF__ + "refreshPartitionFreeSizes"
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", partitionPathName: " + partitionPathName
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

			if (JSONUtils::isMetadataPresent(_configuration["storage"], freeSpaceConfField))
				localFreeSpaceToLeaveInMB = JSONUtils::asInt(
					_configuration["storage"], freeSpaceConfField, 100);
			else
				localFreeSpaceToLeaveInMB = _freeSpaceToLeaveInEachPartitionInMB;
		}

		_logger->info(__FILEREF__ + "addUpdatePartitionInfo"
			+ ", partitionKey: " + to_string(partitionKey)
			+ ", partitionPathName: " + partitionPathName
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
		_logger->info(__FILEREF__ + "Received manageTarFileInCaseOfIngestionOfSegments"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", tarBinaryPathName: " + tarBinaryPathName
			+ ", workspaceIngestionRepository: " + workspaceIngestionRepository
			+ ", sourcePathName: " + sourcePathName
		);

		// tar into workspaceIngestion directory
		//	source will be something like <ingestion key>_source
		//	destination will be the original directory (that has to be the same name of the tar file name)
		executeCommand =
			"tar xfz " + tarBinaryPathName
			+ " --directory " + workspaceIngestionRepository;
		_logger->info(__FILEREF__ + "Start tar command "
			+ ", executeCommand: " + executeCommand
		);
		chrono::system_clock::time_point startTar = chrono::system_clock::now();
		int executeCommandStatus = ProcessUtility::execute(executeCommand);
		chrono::system_clock::time_point endTar = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "End tar command "
			+ ", executeCommand: " + executeCommand
			+ ", @MMS statistics@ - tarDuration (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count()) + "@"
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
			string sourceTarFile = workspaceIngestionRepository + "/"
				+ to_string(ingestionJobKey)
				+ "_source"
				+ ".tar.gz";

			_logger->info(__FILEREF__ + "Remove file"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceTarFile: " + sourceTarFile
			);

			FileIO::remove(sourceTarFile);
		}

		// rename directory generated from tar: from user_tar_filename to 1247848_source
		// Example from /var/catramms/storage/IngestionRepository/users/1/9670725_liveRecorderVirtualVOD
		//	to /var/catramms/storage/IngestionRepository/users/1/9676038_source
		{
			string sourceDirectory = workspaceIngestionRepository + "/" + sourceFileName;
			string destDirectory = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
			_logger->info(__FILEREF__ + "Start moveDirectory..."
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", sourceDirectory: " + sourceDirectory
				+ ", destDirectory: " + destDirectory
			);
			// 2020-05-01: since the remove of the director could fails because of nfs issue,
			//	better do a copy and then a remove.
			//	In this way, in case the remove fails, we can ignore the error.
			//	The directory will be removed later by cron job
			{
				chrono::system_clock::time_point startPoint = chrono::system_clock::now();
				FileIO::copyDirectory(sourceDirectory, destDirectory,
					S_IRUSR | S_IWUSR | S_IXUSR |                                                                         
					S_IRGRP | S_IXGRP |                                                                                   
					S_IROTH | S_IXOTH);
				chrono::system_clock::time_point endPoint = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "End copyDirectory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceDirectory: " + sourceDirectory
					+ ", destDirectory: " + destDirectory
					+ ", @MMS COPY statistics@ - copyDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
				);
			}

			try
			{
				chrono::system_clock::time_point startPoint = chrono::system_clock::now();
				bool removeRecursively = true;
				FileIO::removeDirectory(sourceDirectory, removeRecursively);
				chrono::system_clock::time_point endPoint = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "End removeDirectory"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", sourceDirectory: " + sourceDirectory
					+ ", @MMS REMOVE statistics@ - removeDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()) + "@"
				);
			}
			catch(runtime_error e)
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
	catch(runtime_error e)
	{
		string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments failed")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", e.what: " + e.what() 
		;
		_logger->error(__FILEREF__ + errorMessage);
         
		throw runtime_error(errorMessage);
	}
}

