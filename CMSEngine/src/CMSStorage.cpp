
#include "CMSStorage.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/System.h"
#include "catralibraries/DateTime.h"

CMSStorage::CMSStorage(
        string storage, 
        unsigned long freeSpaceToLeaveInEachPartitionInMB, 
        shared_ptr<spdlog::logger> logger) 
{

    _logger             = logger;

    _hostName = System::getHostName();

    // MB
    _freeSpaceToLeaveInEachPartitionInMB = freeSpaceToLeaveInEachPartitionInMB;

    _storage = storage;

    _ftpRootRepository = _storage + "FTPRepository/users/";
    _cmsRootRepository = _storage + "CMSRepository/";
    _downloadRootRepository = _storage + "DownloadRepository/";
    _streamingRootRepository = _storage + "StreamingRepository/";

    _errorRootRepository = _storage + "CMSWorkingAreaRepository/Errors/";
    _doneRootRepository = _storage + "CMSWorkingAreaRepository/Done/";
    _stagingRootRepository = _storage + "CMSWorkingAreaRepository/Staging/";

    _profilesRootRepository = _storage + "CMSRepository/EncodingProfiles/";

    bool noErrorIfExists = true;
    bool recursive = true;
    _logger->info(string("Creating directory (if needed)")
        + ", _ftpRootRepository: " + _ftpRootRepository
    );
    FileIO::createDirectory(_ftpRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(string("Creating directory (if needed)")
        + ", _cmsRootRepository: " + _cmsRootRepository
    );
    FileIO::createDirectory(_cmsRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    // create CMS_0000 in case it does not exist (first running of CMS)
    {
        string CMS_0000Path = _cmsRootRepository + "CMS_0000";


        _logger->info(string("Creating directory (if needed)")
            + ", CMS_0000Path: " + CMS_0000Path
        );
        FileIO::createDirectory(CMS_0000Path,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    _logger->info(string("Creating directory (if needed)")
        + ", _downloadRootRepository: " + _downloadRootRepository
    );
    FileIO::createDirectory(_downloadRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(string("Creating directory (if needed)")
        + ", _streamingRootRepository: " + _streamingRootRepository
    );
    FileIO::createDirectory(_streamingRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(string("Creating directory (if needed)")
        + ", _errorRootRepository: " + _errorRootRepository
    );
    FileIO::createDirectory(_errorRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(string("Creating directory (if needed)")
        + ", _doneRootRepository: " + _doneRootRepository
    );
    FileIO::createDirectory(_doneRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(string("Creating directory (if needed)")
        + ", _profilesRootRepository: " + _profilesRootRepository
    );
    FileIO::createDirectory(_profilesRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(string("Creating directory (if needed)")
        + ", _stagingRootRepository: " + _stagingRootRepository
    );
    FileIO::createDirectory(_stagingRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    // Partitions staff
    {
        char pCMSPartitionName [64];
        unsigned long long ullUsedInKB;
        unsigned long long ullAvailableInKB;
        long lPercentUsed;


        lock_guard<recursive_mutex> locker(_mtCMSPartitions);

        unsigned long ulCMSPartitionsNumber = 0;
        bool cmsAvailablePartitions = true;

        _ulCurrentCMSPartitionIndex = 0;

        // inizializzare FreeSize
        while (cmsAvailablePartitions) 
        {
            string pathNameToGetFileSystemInfo(_cmsRootRepository);

            sprintf(pCMSPartitionName, "CMS_%04lu", ulCMSPartitionsNumber);

            pathNameToGetFileSystemInfo.append(pCMSPartitionName);

            try 
            {
                FileIO::getFileSystemInfo(pathNameToGetFileSystemInfo,
                        &ullUsedInKB, &ullAvailableInKB, &lPercentUsed);
            }            
            catch (...) 
            {
                break;
            }

            _cmsPartitionsFreeSizeInMB.push_back(ullAvailableInKB / 1024);

            ulCMSPartitionsNumber++;
        }

        if (ulCMSPartitionsNumber == 0) {
            throw runtime_error("No CMS partition found");
        }

        refreshPartitionsFreeSizes();
    }
}

CMSStorage::~CMSStorage(void) {
}

string CMSStorage::getCMSRootRepository(void) {
    return _cmsRootRepository;
}

string CMSStorage::getStreamingRootRepository(void) {
    return _streamingRootRepository;
}

string CMSStorage::getDownloadRootRepository(void) {
    return _downloadRootRepository;
}

string CMSStorage::getFTPRootRepository(void) {
    return _ftpRootRepository;
}

string CMSStorage::getStagingRootRepository(void) {
    return _stagingRootRepository;
}

string CMSStorage::getErrorRootRepository(void) {
    return _errorRootRepository;
}

string CMSStorage::getDoneRootRepository(void) {
    return _doneRootRepository;
}

void CMSStorage::refreshPartitionsFreeSizes(void) 
{
    char pCMSPartitionName [64];
    unsigned long long ullUsedInKB;
    unsigned long long ullAvailableInKB;
    long lPercentUsed;


    lock_guard<recursive_mutex> locker(_mtCMSPartitions);

    for (unsigned long ulCMSPartitionIndex = 0;
            ulCMSPartitionIndex < _cmsPartitionsFreeSizeInMB.size();
            ulCMSPartitionIndex++) 
    {
        string pathNameToGetFileSystemInfo(_cmsRootRepository);

        sprintf(pCMSPartitionName, "CMS_%04lu", ulCMSPartitionIndex);

        pathNameToGetFileSystemInfo.append(pCMSPartitionName);

        FileIO::getFileSystemInfo(pathNameToGetFileSystemInfo,
                &ullUsedInKB, &ullAvailableInKB, &lPercentUsed);

        _cmsPartitionsFreeSizeInMB[ulCMSPartitionIndex] =
                ullAvailableInKB / 1024;

        _logger->info(string("Available space")
            + ", pathNameToGetFileSystemInfo: " + pathNameToGetFileSystemInfo
            + ", _cmsPartitionsFreeSizeInMB[ulCMSPartitionIndex]: " + to_string(_cmsPartitionsFreeSizeInMB[ulCMSPartitionIndex])
        );
    }
}

string CMSStorage::creatingDirsUsingTerritories(
        unsigned long ulCurrentCMSPartitionIndex,
        string relativePath,
        string customerDirectoryName,
        bool deliveryRepositoriesToo,
        Customer::TerritoriesHashMap& phmTerritories)
 {

    char pCMSPartitionName [64];


    sprintf(pCMSPartitionName, "CMS_%04lu/", ulCurrentCMSPartitionIndex);

    string cmsAssetPathName(_cmsRootRepository);
    cmsAssetPathName
        .append(pCMSPartitionName)
        .append(customerDirectoryName)
        .append(relativePath);

    if (!FileIO::directoryExisting(cmsAssetPathName)) 
    {
        _logger->info(string("Create directory")
            + ", cmsAssetPathName: " + cmsAssetPathName
        );

        bool noErrorIfExists = true;
        bool recursive = true;
        FileIO::createDirectory(cmsAssetPathName,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP |
                S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    if (cmsAssetPathName.back() != '/')
        cmsAssetPathName.append("/");

    if (deliveryRepositoriesToo) 
    {
        Customer::TerritoriesHashMap::iterator it;


        for (it = phmTerritories.begin(); it != phmTerritories.end(); ++it) 
        {
            string territoryName = it->second;

            string downloadAssetPathName(_downloadRootRepository);
            downloadAssetPathName
                .append(pCMSPartitionName)
                .append(customerDirectoryName)
                .append("/")
                .append(territoryName)
                .append(relativePath);

            string streamingAssetPathName(_streamingRootRepository);
            streamingAssetPathName
                .append(pCMSPartitionName)
                .append(customerDirectoryName)
                .append("/")
                .append(territoryName)
                .append(relativePath);

            if (!FileIO::directoryExisting(downloadAssetPathName)) 
            {
                _logger->info(string("Create directory")
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
                _logger->info(string("Create directory")
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


    return cmsAssetPathName;
}

void CMSStorage::moveContentInRepository(
        string filePathName,
        RepositoryType rtRepositoryType,
        string customerDirectoryName,
        bool addDateTimeToFileName)
 {

    contentInRepository(
        1,
        filePathName,
        rtRepositoryType,
        customerDirectoryName,
        addDateTimeToFileName);
}

void CMSStorage::copyFileInRepository(
        string filePathName,
        RepositoryType rtRepositoryType,
        string customerDirectoryName,
        bool addDateTimeToFileName)
 {

    contentInRepository(
        0,
        filePathName,
        rtRepositoryType,
        customerDirectoryName,
        addDateTimeToFileName);
}

string CMSStorage::getRepository(RepositoryType rtRepositoryType) 
{

    switch (rtRepositoryType) 
    {
        case CMSREP_REPOSITORYTYPE_CMSCUSTOMER:
        {
            return _cmsRootRepository;
        }
        case CMSREP_REPOSITORYTYPE_DOWNLOAD:
        {
            return _downloadRootRepository;
        }
        case CMSREP_REPOSITORYTYPE_STREAMING:
        {
            return _streamingRootRepository;
        }
        case CMSREP_REPOSITORYTYPE_STAGING:
        {
            return _stagingRootRepository;
        }
        case CMSREP_REPOSITORYTYPE_DONE:
        {
            return _doneRootRepository;
        }
        case CMSREP_REPOSITORYTYPE_ERRORS:
        {
            return _errorRootRepository;
        }
        case CMSREP_REPOSITORYTYPE_FTP:
        {
            return _ftpRootRepository;
        }
        default:
        {
            throw invalid_argument(string("Wrong argument")
                    + ", rtRepositoryType: " + to_string(rtRepositoryType)
                    );
        }
    }
}

void CMSStorage::contentInRepository(
        unsigned long ulIsCopyOrMove,
        string contentPathName,
        RepositoryType rtRepositoryType,
        string customerDirectoryName,
        bool addDateTimeToFileName)
 {

    tm tmDateTime;
    unsigned long ulMilliSecs;
    FileIO::DirectoryEntryType_t detSourceFileType;


    // pDestRepository includes the '/' at the end
    string metaDataFileInDestRepository(getRepository(rtRepositoryType));
    metaDataFileInDestRepository
        .append(customerDirectoryName)
        .append("/");

    DateTime::get_tm_LocalTime(&tmDateTime, &ulMilliSecs);

    if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DONE ||
            rtRepositoryType == CMSREP_REPOSITORYTYPE_STAGING ||
            rtRepositoryType == CMSREP_REPOSITORYTYPE_ERRORS) 
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
            _logger->info(string("Create directory")
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

        if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DONE) 
        {
            sprintf(pDateTime, "%02lu",
                    (unsigned long) (tmDateTime. tm_hour));

            metaDataFileInDestRepository.append(pDateTime);

            if (!FileIO::directoryExisting(metaDataFileInDestRepository)) 
            {
                _logger->info(string("Create directory")
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
            _logger->info(string("Move directory")
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
            _logger->info(string("Move file")
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
            _logger->info(string("Copy directory")
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
            _logger->info(string("Copy file")
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::copyFile(contentPathName,
                    metaDataFileInDestRepository);
        }
    }
}

string CMSStorage::moveAssetInCMSRepository(
        string sourceAssetPathName,
        string customerDirectoryName,
        string destinationFileName,
        string relativePath,

        bool partitionIndexToBeCalculated,
        unsigned long *pulCMSPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

        bool deliveryRepositoriesToo,
        Customer::TerritoriesHashMap& phmTerritories
        )
 {
    FileIO::DirectoryEntryType_t detSourceFileType;


    if (relativePath.front() != '/' || pulCMSPartitionIndexUsed == (unsigned long *) NULL) 
    {
            throw invalid_argument(string("Wrong argument")
                    + ", relativePath: " + relativePath
                    );
    }

    lock_guard<recursive_mutex> locker(_mtCMSPartitions);

    // file in case of .3gp content OR
    // directory in case of IPhone content
    detSourceFileType = FileIO::getDirectoryEntryType(sourceAssetPathName);

    if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
            detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
    {
        throw runtime_error("Wrong directory entry type");
    }

    if (partitionIndexToBeCalculated) 
    {
        unsigned long long ullFSEntrySizeInBytes;


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

        // find the CMS partition index
        unsigned long ulCMSPartitionIndex;
        for (ulCMSPartitionIndex = 0;
                ulCMSPartitionIndex < _cmsPartitionsFreeSizeInMB.size();
                ulCMSPartitionIndex++) 
        {
            unsigned long long cmsPartitionsFreeSizeInKB = (unsigned long long)
                ((_cmsPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex]) * 1024);

            if (cmsPartitionsFreeSizeInKB <=
                    (_freeSpaceToLeaveInEachPartitionInMB * 1024)) 
            {
                _logger->info(string("Partition space too low")
                    + ", _ulCurrentCMSPartitionIndex: " + to_string(_ulCurrentCMSPartitionIndex)
                    + ", cmsPartitionsFreeSizeInKB: " + to_string(cmsPartitionsFreeSizeInKB)
                    + ", _freeSpaceToLeaveInEachPartitionInMB * 1024: " + to_string(_freeSpaceToLeaveInEachPartitionInMB * 1024)
                );

                if (_ulCurrentCMSPartitionIndex + 1 >= _cmsPartitionsFreeSizeInMB.size())
                    _ulCurrentCMSPartitionIndex = 0;
                else
                    _ulCurrentCMSPartitionIndex++;

                continue;
            }

            if ((unsigned long long) (cmsPartitionsFreeSizeInKB -
                    (_freeSpaceToLeaveInEachPartitionInMB * 1024)) >
                    (ullFSEntrySizeInBytes / 1024)) 
            {
                break;
            }

            if (_ulCurrentCMSPartitionIndex + 1 >= _cmsPartitionsFreeSizeInMB.size())
                _ulCurrentCMSPartitionIndex = 0;
            else
                _ulCurrentCMSPartitionIndex++;
        }

        if (ulCMSPartitionIndex == _cmsPartitionsFreeSizeInMB.size()) 
        {
            throw runtime_error(string("No more space in CMS Partitions")
                    + ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
                    );
        }

        *pulCMSPartitionIndexUsed = _ulCurrentCMSPartitionIndex;
    }

    // creating directories and build the bCMSAssetPathName
    string cmsAssetPathName;
    {
        // to create the content provider directory and the
        // territories directories (if not already existing)
        cmsAssetPathName = creatingDirsUsingTerritories(*pulCMSPartitionIndexUsed,
            relativePath, customerDirectoryName, deliveryRepositoriesToo,
            phmTerritories);

        cmsAssetPathName.append(destinationFileName);
    }

    _logger->info(string("Selected CMS Partition for the content")
        + ", customerDirectoryName: " + customerDirectoryName
        + ", *pulCMSPartitionIndexUsed: " + to_string(*pulCMSPartitionIndexUsed)
        + ", cmsAssetPathName: " + cmsAssetPathName
        + ", _cmsPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex]: " + to_string(_cmsPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex])
    );

    // move the file in case of .3gp content OR
    // move the directory in case of IPhone content
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            _logger->info(string("Move directory")
                + ", from: " + sourceAssetPathName
                + ", to: " + cmsAssetPathName
            );

            FileIO::moveDirectory(sourceAssetPathName,
                    cmsAssetPathName,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
        } 
        else // if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_REGULARFILE)
        {
            _logger->info(string("Move file")
                + ", from: " + sourceAssetPathName
                + ", to: " + cmsAssetPathName
            );

            FileIO::moveFile(sourceAssetPathName,
                    cmsAssetPathName);
        }
    }

    // update _pullCMSPartitionsFreeSizeInMB ONLY if bIsPartitionIndexToBeCalculated
    if (partitionIndexToBeCalculated) 
    {
        unsigned long long ullUsedInKB;
        unsigned long long ullAvailableInKB;
        long lPercentUsed;


        FileIO::getFileSystemInfo(cmsAssetPathName,
                &ullUsedInKB, &ullAvailableInKB, &lPercentUsed);

        _cmsPartitionsFreeSizeInMB [_ulCurrentCMSPartitionIndex] =
            ullAvailableInKB / 1024;

        _logger->info(string("Available space")
            + ", cmsAssetPathName: " + cmsAssetPathName
            + ", _cmsPartitionsFreeSizeInMB[_ulCurrentCMSPartitionIndex]: " + to_string(_cmsPartitionsFreeSizeInMB[_ulCurrentCMSPartitionIndex])
        );
    }


    return cmsAssetPathName;
}

string CMSStorage::getCMSAssetPathName(
        unsigned long ulPartitionNumber,
        string customerDirectoryName,
        string relativePath, // using '/'
        string fileName)
 {
    char pCMSPartitionName [64];


    sprintf(pCMSPartitionName, "CMS_%04lu/", ulPartitionNumber);

    string assetPathName(_cmsRootRepository);
    assetPathName
        .append(pCMSPartitionName)
        .append(customerDirectoryName)
        .append(relativePath)
        .append(fileName);


    return assetPathName;
}

string CMSStorage::getDownloadLinkPathName(
        unsigned long ulPartitionNumber,
        string customerDirectoryName,
        string territoryName,
        string relativePath,
        string fileName,
        bool downloadRepositoryToo)
 {

    char pCMSPartitionName [64];
    string linkPathName;

    if (downloadRepositoryToo) 
    {
        sprintf(pCMSPartitionName, "CMS_%04lu/", ulPartitionNumber);

        linkPathName = _downloadRootRepository;
        linkPathName
            .append(pCMSPartitionName)
            .append(customerDirectoryName)
            .append("/")
            .append(territoryName)
            .append(relativePath)
            .append(fileName);
    } 
    else
    {
        sprintf(pCMSPartitionName, "/CMS_%04lu/", ulPartitionNumber);

        linkPathName = pCMSPartitionName;
        linkPathName
            .append(customerDirectoryName)
            .append("/")
            .append(territoryName)
            .append(relativePath)
            .append(fileName);
    }


    return linkPathName;
}

string CMSStorage::getStreamingLinkPathName(
        unsigned long ulPartitionNumber, // IN
        string customerDirectoryName, // IN
        string territoryName, // IN
        string relativePath, // IN
        string fileName) // IN
 {
    char pCMSPartitionName [64];
    string linkPathName;


    sprintf(pCMSPartitionName, "CMS_%04lu/", ulPartitionNumber);

    linkPathName = _streamingRootRepository;
    linkPathName
        .append(pCMSPartitionName)
        .append(customerDirectoryName)
        .append("/")
        .append(territoryName)
        .append(relativePath)
        .append(fileName);


    return linkPathName;
}

string CMSStorage::getStagingAssetPathName(
        string customerDirectoryName,
        string relativePath,
        string fileName, // may be empty ("")
        long long llMediaItemKey,
        long long llPhysicalPathKey,
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
            "%04lu_%02lu_%02lu",
            (unsigned long) (tmDateTime. tm_year + 1900),
            (unsigned long) (tmDateTime. tm_mon + 1),
            (unsigned long) (tmDateTime. tm_mday));

    // create the 'date' directory in staging if not exist
    {
        assetPathName = _stagingRootRepository;
        assetPathName
            .append(customerDirectoryName)
            .append("/")
            .append(pDateTime)
            .append(relativePath);

        if (!FileIO::directoryExisting(assetPathName)) 
        {
            _logger->info(string("Create directory")
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
                    _logger->info(string("Remove directory")
                        + ", assetPathName: " + assetPathName
                    );

                    bool removeRecursively = true;
                    FileIO::removeDirectory(assetPathName, removeRecursively);
                } 
                else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
                {
                    _logger->info(string("Remove file")
                        + ", assetPathName: " + assetPathName
                    );

                    FileIO::remove(assetPathName);
                } 
                else 
                {
                    throw runtime_error(string("Unexpected file in staging")
                            + ", assetPathName: " + assetPathName
                            );
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

string CMSStorage::getEncodingProfilePathName(
        long long llEncodingProfileKey,
        string profileFileNameExtension)
 {
    string encodingProfilePathName(_profilesRootRepository);

    encodingProfilePathName
        .append(to_string(llEncodingProfileKey))
        .append(profileFileNameExtension);

    return encodingProfilePathName;
}

string CMSStorage::getFFMPEGEncodingProfilePathName(
        unsigned long ulContentType,
        long long llEncodingProfileKey)
 {

    if (ulContentType != 0 && ulContentType != 1 && ulContentType != 2 &&
            ulContentType != 4) // video/audio/image/ringtone
    {
        throw invalid_argument(string("Wrong argument")
                + ", ulContentType: " + to_string(ulContentType)
                );
    }

    string encodingProfilePathName(_profilesRootRepository);

    encodingProfilePathName
        .append(to_string(llEncodingProfileKey));

    if (ulContentType == 0) // video
    {
        encodingProfilePathName.append(".vep");
    } 
    else if (ulContentType == 1 || ulContentType == 4) // audio / ringtone
    {
        encodingProfilePathName.append(".aep");
    } 
    else if (ulContentType == 2) // image
    {
        encodingProfilePathName.append(".iep");
    }


    return encodingProfilePathName;
}

unsigned long CMSStorage::getCustomerStorageUsage(
        string customerDirectoryName)
 {

    unsigned long ulStorageUsageInMB;

    unsigned long ulCMSPartitionIndex;
    unsigned long long ullDirectoryUsageInBytes;
    unsigned long long ullCustomerStorageUsageInBytes;


    lock_guard<recursive_mutex> locker(_mtCMSPartitions);

    ullCustomerStorageUsageInBytes = 0;

    for (ulCMSPartitionIndex = 0;
            ulCMSPartitionIndex < _cmsPartitionsFreeSizeInMB.size();
            ulCMSPartitionIndex++) 
    {
        string contentProviderPathName = getCMSAssetPathName(
                ulCMSPartitionIndex, customerDirectoryName,
                string(""), string(""));

        try 
        {
            ullDirectoryUsageInBytes = FileIO::getDirectoryUsage(contentProviderPathName);
        } 
        catch (DirectoryNotExisting d) 
        {
            continue;
        } 
        catch (...) 
        {
            throw runtime_error(string("FileIO:: getDirectoryUsage failed")
                    + ", contentProviderPathName: " + contentProviderPathName
                    );
        }

        ullCustomerStorageUsageInBytes += ullDirectoryUsageInBytes;
    }


    ulStorageUsageInMB = (unsigned long)
            (ullCustomerStorageUsageInBytes / (1024 * 1024));

    return ulStorageUsageInMB;
}


/*
Error CMSStorage:: sanityCheck_ContentsOnFileSystem (
        RepositoryType rtRepositoryType)

{

        FileIO:: Directory_t			dDeliveryDirectoryL1;
        Error_t							errOpenDir;
        Error_t							errReadDirectory;
        FileIO:: DirectoryEntryType_t	detDirectoryEntryType;
        SanityCheckContentInfo_t		psciSanityCheckContentsInfo [
                CMSREP_CMSREPOSITORY_MAXSANITYCHECKCONTENTSINFONUMBER];
        long							lSanityCheckContentsInfoCurrentIndex;
        time_t							tSanityCheckStart;
        unsigned long					ulDirectoryLevelIndexInsideCustomer;
        unsigned long					ulFileIndex;
        unsigned long					ulCurrentFileNumberProcessedInThisSchedule;
        unsigned long				ulCurrentFilesRemovedNumberInThisSchedule;
        // OthersFiles means: unexpected, WorkingArea
        unsigned long				ulCurrentDirectoriesRemovedNumberInThisSchedule;
        Boolean_t					bHasCustomerToBeResumed;
        Error_t						errSanityCheck;


        {
                Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
                        CMSREP_CMSREPOSITORY_STARTSANITYCHECKONREPOSITORY,
                        1, (const char *) (*(_pbRepositories [rtRepositoryType])));
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        tSanityCheckStart								= time (NULL);

        bHasCustomerToBeResumed							= true;

        lSanityCheckContentsInfoCurrentIndex			= -1;

        ulFileIndex										= 0;
        ulCurrentFileNumberProcessedInThisSchedule		= 0;
        ulCurrentFilesRemovedNumberInThisSchedule	= 0;
        ulCurrentDirectoriesRemovedNumberInThisSchedule	= 0;

        if ((errOpenDir = FileIO:: openDirectory (
                (const char *) (*(_pbRepositories [rtRepositoryType])),
                &dDeliveryDirectoryL1)) != errNoError)
        {
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) errOpenDir, __FILE__, __LINE__);

                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_OPENDIRECTORY_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                        __FILE__, __LINE__);

                return err;
        }

        if (rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER ||
                rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
                rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
        {
                // the customers inside the CMS repository are inside one more
                // level because this level is for the CMSREP_XXXX directories

                FileIO:: Directory_t			dDeliveryDirectoryL2;
                Buffer_t						bPartitionDirectory;
                Buffer_t						bCustomersDirectory;
                Boolean_t						bIsCMS_Directory;
                Boolean_t						bHasPartitionToBeResumed;


                if (bCustomersDirectory. init () != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_INIT_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);
                        }

                        return err;
                }

                if (bPartitionDirectory. init () != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_INIT_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);
                        }

                        return err;
                }

                bHasPartitionToBeResumed				= true;

                while ((errReadDirectory = FileIO:: readDirectory (
                        &dDeliveryDirectoryL1, &bPartitionDirectory,
                        &detDirectoryEntryType)) == errNoError)
                {
                        if (bHasPartitionToBeResumed &&
                                strcmp (_svlpcLastProcessedContent [rtRepositoryType].
                                        _pPartition, "") &&
                                strcmp (_svlpcLastProcessedContent [rtRepositoryType].
                                        _pPartition, (const char *) bPartitionDirectory))
                                continue;

                        bHasPartitionToBeResumed			= false;

                        if ((unsigned long) bPartitionDirectory >=
                                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH)
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_BUFFERNOTENOUGHBIG,
                                        2,
                                        (unsigned long)
                                                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                                        (unsigned long) bPartitionDirectory);
                                _ptSystemTracer -> trace (
                                        Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }
                        strcpy (_svlpcLastProcessedContent [rtRepositoryType].
                                _pPartition, (const char *) bPartitionDirectory);

                        if ((detDirectoryEntryType == FileIO:: TOOLS_FILEIO_DIRECTORY ||
                                detDirectoryEntryType == FileIO:: TOOLS_FILEIO_LINKFILE) &&
                                strncmp ((const char *) bPartitionDirectory, "CMS_", 4) == 0)
                        {
                                bIsCMS_Directory			= true;
                        }
                        else
                        {
                                bIsCMS_Directory			= false;
                        }

                        if ((rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pDownloadReservedDirectoryName)) ||
                                (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pDownloadFreeDirectoryName)) ||
                                (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pDownloadiPhoneLiveDirectoryName)) ||
                                (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pDownloadSilverlightLiveDirectoryName)) ||
                                (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pDownloadAdobeLiveDirectoryName)) ||
                                (rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pStreamingFreeDirectoryName)) ||
                                (rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pStreamingMetaDirectoryName)) ||
                                (rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
                                        !strcmp ((const char *) bPartitionDirectory,
                                        _pStreamingRecordingDirectoryName)))
                                continue;

                        if (bCustomersDirectory. setBuffer (
                                (const char *) (*(_pbRepositories [rtRepositoryType]))) !=
                                errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_BUFFER_SETBUFFER_FAILED);
                                _ptSystemTracer -> trace (
                                        Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }

                        if (bCustomersDirectory. append (
                                (const char *) bPartitionDirectory) != errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_BUFFER_APPEND_FAILED);
                                _ptSystemTracer -> trace (
                                        Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }

                        if (bCustomersDirectory. append ("/") != errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_BUFFER_APPEND_FAILED);
                                _ptSystemTracer -> trace (
                                        Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }

                        if (!strcmp ((const char *) bCustomersDirectory,
                                        (const char *) _bDoneRootRepository) ||
                                !strcmp ((const char *) bCustomersDirectory,
                                        (const char *) _bErrorRootRepository) ||
                                !strcmp ((const char *) bCustomersDirectory,
                                        (const char *) _bStagingRootRepository) ||
                                !strcmp ((const char *) bCustomersDirectory,
                                        (const char *) _bProfilesRootRepository))
                                continue;

                        if (!bIsCMS_Directory)
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
                                        2, (const char *) (*(_pbRepositories [rtRepositoryType])),
                                        (const char *) bCustomersDirectory);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                // To Be Done: file to be removed
                                // if (_bUnexpectedFilesToBeRemoved)

                                continue;
                        }

                        if ((errOpenDir = FileIO:: openDirectory (
                                (const char *) bCustomersDirectory,
                                &dDeliveryDirectoryL2)) != errNoError)
                        {
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) errOpenDir, __FILE__, __LINE__);

                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_FILEIO_OPENDIRECTORY_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }

                        ulDirectoryLevelIndexInsideCustomer		= 0;

                        if ((errSanityCheck = sanityCheck_CustomersDirectory (
                                rtRepositoryType,
                                (const char *) bCustomersDirectory,
                                &dDeliveryDirectoryL2,
                                psciSanityCheckContentsInfo,
                                &lSanityCheckContentsInfoCurrentIndex,
                                &ulFileIndex,
                                &ulCurrentFileNumberProcessedInThisSchedule,
                                &ulCurrentFilesRemovedNumberInThisSchedule,
                                &ulCurrentDirectoriesRemovedNumberInThisSchedule,
                                &ulDirectoryLevelIndexInsideCustomer,
                                &bHasCustomerToBeResumed)) != errNoError)
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_SANITYCHECK_CUSTOMERSDIRECTORY_FAILED);

                                if ((unsigned long) errSanityCheck ==
                                        CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
                                {
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                                (const char *) err, __FILE__, __LINE__);
                                }
                                else
                                {
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

//				if (FileIO:: closeDirectory (&dDeliveryDirectoryL2) !=
//					errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
//					errNoError)
//				{
//					Error err = ToolsErrors (__FILE__, __LINE__,
//						TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);
//				}
//
//				return err;
                        }

                        if (FileIO:: closeDirectory (&dDeliveryDirectoryL2) !=
                                errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }
                }

                if (bHasPartitionToBeResumed)
                {
                        // if bHasPartitionToBeResumed is still true, it means the
                        // Partition was not found and we will reset it

                        strcpy (_svlpcLastProcessedContent [rtRepositoryType].
                                _pPartition, "");
                }
        }
        else
        {
                ulDirectoryLevelIndexInsideCustomer		= 0;

                if ((errSanityCheck = sanityCheck_CustomersDirectory (rtRepositoryType,
                        (const char *) (*(_pbRepositories [rtRepositoryType])),
                        &dDeliveryDirectoryL1,
                        psciSanityCheckContentsInfo,
                        &lSanityCheckContentsInfoCurrentIndex,
                        &ulFileIndex,
                        &ulCurrentFileNumberProcessedInThisSchedule,
                        &ulCurrentFilesRemovedNumberInThisSchedule,
                        &ulCurrentDirectoriesRemovedNumberInThisSchedule,
                        &ulDirectoryLevelIndexInsideCustomer,
                        &bHasCustomerToBeResumed)) != errNoError)
                {
                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                CMSREP_CMSREPOSITORY_SANITYCHECK_CUSTOMERSDIRECTORY_FAILED);
                        if ((unsigned long) errSanityCheck ==
                                CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
                        {
                                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                        (const char *) err, __FILE__, __LINE__);
                        }
                        else
                        {
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);
                        }

//			if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) != errNoError)
//			{
//				Error err = ToolsErrors (__FILE__, __LINE__,
//					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//					(const char *) err, __FILE__, __LINE__);
//			}
//
//			return err;
                }
        }

        if (bHasCustomerToBeResumed)
        {
                // if bHasCustomerToBeResumed is still true, it means the
                // CustomerDirectoryName was not found and we will reset it

                strcpy (_svlpcLastProcessedContent [rtRepositoryType].
                        _pCustomerDirectoryName, "");

                strcpy (_svlpcLastProcessedContent [rtRepositoryType]. _pPartition, "");

                _svlpcLastProcessedContent [rtRepositoryType].
                        _ulFilesNumberAlreadyProcessed					= 0;
        }

        if (FileIO:: closeDirectory (&dDeliveryDirectoryL1) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        if (lSanityCheckContentsInfoCurrentIndex >= 0)
        {
                if (sanityCheck_runOnContentsInfo (
                        psciSanityCheckContentsInfo,
                        lSanityCheckContentsInfoCurrentIndex,
                        rtRepositoryType,
                        &ulCurrentFilesRemovedNumberInThisSchedule,
                        &ulCurrentDirectoriesRemovedNumberInThisSchedule) != errNoError)
                {
                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                CMSREP_CMSREPOSITORY_SANITYCHECK_RUNONCONTENTSINFO_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        // return err;
                }

                lSanityCheckContentsInfoCurrentIndex		= -1;
        }

        {
                Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
                        CMSREP_CMSREPOSITORY_ENDSANITYCHECKONREPOSITORY,
                        7, (const char *) (*(_pbRepositories [rtRepositoryType])),
                        (unsigned long) (time (NULL) - tSanityCheckStart),
                        _svlpcLastProcessedContent [rtRepositoryType]. _pPartition,
                        _svlpcLastProcessedContent [rtRepositoryType]. _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [rtRepositoryType].
                                _ulFilesNumberAlreadyProcessed,
                        ulCurrentFilesRemovedNumberInThisSchedule,
                        ulCurrentDirectoriesRemovedNumberInThisSchedule);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

//	_svlpcLastProcessedContent [rtRepositoryType].
//		_ulFilesNumberAlreadyProcessed					+= 
//		ulCurrentFileNumberProcessedInThisSchedule;
//	
//	if (ulCurrentFileNumberProcessedInThisSchedule <
//		_ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType])
//	{
//		// all the files were processed
//
//		_svlpcLastProcessedContent [rtRepositoryType].
//			_ulFilesNumberAlreadyProcessed					= 0;
//	}


        return errNoError;
}


Error CMSStorage:: sanityCheck_CustomersDirectory (
        RepositoryType rtRepositoryType,
        const char *pCustomersDirectory,
        FileIO:: Directory_p pdDeliveryDirectory,
        SanityCheckContentInfo_p psciSanityCheckContentsInfo,
        long *plSanityCheckContentsInfoCurrentIndex,
        unsigned long *pulFileIndex,
        unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
        unsigned long *pulCurrentFilesRemovedNumberInThisSchedule,
        unsigned long *pulCurrentDirectoriesRemovedNumberInThisSchedule,
        unsigned long *pulDirectoryLevelIndexInsideCustomer,
        Boolean_p pbHasCustomerToBeResumed)

{

        Error_t							errReadDirectory;
        FileIO:: DirectoryEntryType_t	detDirectoryEntryType;
        Buffer_t						bCustomerDirectoryName;
        Buffer_t						bCustomerDirectory;
        Error_t							errSanityCheck;


        {
                Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
                        CMSREP_CMSREPOSITORY_SANITYCHECKONCUSTOMERSDIRECTORY,
                        1, pCustomersDirectory);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (bCustomerDirectoryName. init () != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                        __FILE__, __LINE__);

                return err;
        }

        if (bCustomerDirectory. init () != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                        __FILE__, __LINE__);

                return err;
        }

        while ((errReadDirectory = FileIO:: readDirectory (pdDeliveryDirectory,
                &bCustomerDirectoryName, &detDirectoryEntryType)) == errNoError)
        {
                if (*pbHasCustomerToBeResumed &&
                        strcmp (_svlpcLastProcessedContent [rtRepositoryType].
                                _pCustomerDirectoryName, "") &&
                        strcmp (_svlpcLastProcessedContent [rtRepositoryType].
                                _pCustomerDirectoryName, (const char *) bCustomerDirectoryName))
                {
                        continue;
                }

                if ((rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pDownloadReservedDirectoryName)) ||
                        (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pDownloadFreeDirectoryName)) ||
                        (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pDownloadiPhoneLiveDirectoryName)) ||
                        (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pDownloadSilverlightLiveDirectoryName)) ||
                        (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pDownloadAdobeLiveDirectoryName)) ||
                        (rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pStreamingFreeDirectoryName)) ||
                        (rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pStreamingMetaDirectoryName)) ||
                        (rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING &&
                                !strcmp ((const char *) bCustomerDirectoryName,
                                _pStreamingRecordingDirectoryName)))
                        continue;

                #ifdef WIN32
                #else
                        if (!strcmp ((const char *) bCustomerDirectoryName, "lost+found"))
                                continue;
                #endif

                if (!strcmp ((const char *) bCustomerDirectoryName, ".snapshot"))
                        continue;

                if (*pbHasCustomerToBeResumed == false)
                {
                        _svlpcLastProcessedContent [rtRepositoryType].
                                _ulFilesNumberAlreadyProcessed					= 0;
                }

 *pbHasCustomerToBeResumed			= false;

                if ((unsigned long) bCustomerDirectoryName >=
                        CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH)
                {
                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                CMSREP_CMSREPOSITORY_BUFFERNOTENOUGHBIG,
                                2,
                                (unsigned long)
                                        CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                                (unsigned long) bCustomerDirectoryName);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                        return err;
                }
                strcpy (_svlpcLastProcessedContent [rtRepositoryType].
                        _pCustomerDirectoryName, (const char *) bCustomerDirectoryName);

                if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_REGULARFILE ||
                        detDirectoryEntryType == FileIO:: TOOLS_FILEIO_LINKFILE ||
                        detDirectoryEntryType == FileIO:: TOOLS_FILEIO_UNKNOWN)
                {
                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
                                2, pCustomersDirectory, (const char *) bCustomerDirectoryName);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        // To Be Done: file to be removed if (_bUnexpectedFilesToBeRemoved)

                        continue;
                }

                // we found a Customer directory
                #ifdef WIN32
                        if (bCustomerDirectory. setBuffer (pCustomersDirectory) !=
                                        errNoError ||
                                bCustomerDirectory. append (bCustomerDirectoryName. str()) !=
                                        errNoError ||
                                bCustomerDirectory. append ("\\") != errNoError)
                #else
                        if (bCustomerDirectory. setBuffer (pCustomersDirectory) !=
                                        errNoError ||
                                bCustomerDirectory. append (bCustomerDirectoryName. str()) !=
                                        errNoError ||
                                bCustomerDirectory. append ("/") != errNoError)
                #endif
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_INSERTAT_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

 *pulDirectoryLevelIndexInsideCustomer			+= 1;

                if ((errSanityCheck = sanityCheck_ContentsDirectory (
                        (const char *) bCustomerDirectoryName,
                        (const char *) bCustomerDirectory,
                        ((unsigned long) bCustomerDirectory) - 1,
                        rtRepositoryType,
                        psciSanityCheckContentsInfo,
                        plSanityCheckContentsInfoCurrentIndex,
                        pulFileIndex,
                        pulCurrentFileNumberProcessedInThisSchedule,
                        pulCurrentFilesRemovedNumberInThisSchedule,
                        pulCurrentDirectoriesRemovedNumberInThisSchedule,
                        pulDirectoryLevelIndexInsideCustomer)) != errNoError)
                {
                        if ((unsigned long) errSanityCheck ==
                                CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSDIRECTORY_FAILED);
                                // _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                // 	(const char *) err, __FILE__, __LINE__);

 *pulDirectoryLevelIndexInsideCustomer			-= 1;

                                return errSanityCheck;
                        }
                        else
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSDIRECTORY_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

 *pulDirectoryLevelIndexInsideCustomer			-= 1;

                                return err;
                        }
                }

 *pulDirectoryLevelIndexInsideCustomer			-= 1;
        }

        // the CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED was not received,
        // so the last customer saved is reset to start
        // from the beginning next time
        strcpy (_svlpcLastProcessedContent [rtRepositoryType]. _pCustomerDirectoryName, "");
        strcpy (_svlpcLastProcessedContent [rtRepositoryType]. _pPartition, "");
        _svlpcLastProcessedContent [rtRepositoryType].
                _ulFilesNumberAlreadyProcessed					= 0;


        return errNoError;
}


Error CMSStorage:: sanityCheck_ContentsDirectory (
        const char *pCustomerDirectoryName, const char *pContentsDirectory,
        unsigned long ulRelativePathIndex,
        RepositoryType rtRepositoryType,
        SanityCheckContentInfo_p psciSanityCheckContentsInfo,
        long *plSanityCheckContentsInfoCurrentIndex,
        unsigned long *pulFileIndex,
        unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
        unsigned long *pulCurrentFilesRemovedNumberInThisSchedule,
        unsigned long *pulCurrentDirectoriesRemovedNumberInThisSchedule,
        unsigned long *pulDirectoryLevelIndexInsideCustomer)
        // pContentsDirectory finishes with / or \\

{

        FileIO:: Directory_t			dContentsDirectory;
        Error_t							errOpenDir;
        Error_t							errReadDirectory;
        FileIO:: DirectoryEntryType_t	detDirectoryEntryType;
        Buffer_t						bContentPathName;
        Error_t							errFileIO;
        Error_t							errSanityCheck;



        // pulDirectoryLevelIndexInsideCustomer:
        // In the CMSRepository:
        // 	1 means customer name
        // 	...
        // 	4 means the .3gp or a directory in case of IPhone

        {
                Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
                        CMSREP_CMSREPOSITORY_SANITYCHECKONDIRECTORY,
                        6, pCustomerDirectoryName, pContentsDirectory,
                        ulRelativePathIndex, *pulDirectoryLevelIndexInsideCustomer,
 *pulFileIndex, _svlpcLastProcessedContent [rtRepositoryType].
                        _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

//	if (*pulCurrentFileNumberProcessedInThisSchedule >=
//		_ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType])
//		return errNoError;

        if (bContentPathName. init () != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                        __FILE__, __LINE__);

                return err;
        }

        if ((errOpenDir = FileIO:: openDirectory (
                pContentsDirectory, &dContentsDirectory)) != errNoError)
        {
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) errOpenDir, __FILE__, __LINE__);

                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_OPENDIRECTORY_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
                        __FILE__, __LINE__);

                return err;
        }

        while ((errReadDirectory = FileIO:: readDirectory (&dContentsDirectory,
                &bContentPathName, &detDirectoryEntryType)) == errNoError)
        {
                (*pulFileIndex)			+= 1;

                if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_UNKNOWN)
                {
                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
                                2, pContentsDirectory, (const char *) bContentPathName);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        if (_bUnexpectedFilesToBeRemoved)
                        {
                                if (bContentPathName. insertAt (0,
                                        pContentsDirectory) != errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_BUFFER_INSERTAT_FAILED);
                                        _ptSystemTracer -> trace (
                                                Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (
                                                &dContentsDirectory) != errNoError)
                                        {
                                                Error err = ToolsErrors (
                                                        __FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (
                                                        Tracer:: TRACER_LERRR,
                                                        (const char *) err,
                                                        __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                (*pulCurrentFilesRemovedNumberInThisSchedule)		+= 1;

                                {
                                        Message msg = CMSRepositoryMessages (
                                                __FILE__, __LINE__,
                                                CMSREP_CMSREPOSITORY_REMOVEFILE,
                                                2, pCustomerDirectoryName, bContentPathName. str());
                                        _ptSystemTracer -> trace (
                                                Tracer:: TRACER_LINFO,
                                                (const char *) msg, __FILE__, __LINE__);
                                }

                                if ((errFileIO = FileIO:: remove (
                                        (const char *) bContentPathName)) !=
                                        errNoError)
                                {
                                        _ptSystemTracer -> trace (
                                                Tracer:: TRACER_LERRR,
                                                (const char *) errFileIO,
                                                __FILE__, __LINE__);

                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_REMOVE_FAILED,
                                                1,
                                                (const char *) bContentPathName);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (
                                                &dContentsDirectory) != errNoError)
                                        {
                                                Error err = ToolsErrors (
                                                        __FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (
                                                        Tracer:: TRACER_LERRR,
                                                        (const char *) err,
                                                        __FILE__, __LINE__);
                                        }

                                        return err;
                                }
                        }

                        continue;

//			if (FileIO:: closeDirectory (&dContentsDirectory) != errNoError)
//			{
//				Error err = ToolsErrors (__FILE__, __LINE__,
//					TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//					(const char *) err, __FILE__, __LINE__);
//			}
//
//			return err;
                }
                else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_STAGING ||
                        rtRepositoryType == CMSREP_REPOSITORYTYPE_ERRORS ||
                        rtRepositoryType == CMSREP_REPOSITORYTYPE_DONE)
                {
                        // in this case, under <staging/error/done>/<customer name>
                        // we should have directory like having the format <YYYY_MM_DD>
                        // We will not go inside and we will remove the entire directory
                        // if too old

                        time_t							tLastModificationTime;
                        FileIO:: DirectoryEntryType_t	detSourceFileType;



                        if (bContentPathName. insertAt (0,
                                pContentsDirectory) != errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_BUFFER_INSERTAT_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }

                        if ((errFileIO = FileIO:: getDirectoryEntryType (
                                bContentPathName. str(), &detSourceFileType)) !=
                                errNoError)
                        {
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) errFileIO, __FILE__, __LINE__);

                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
                                        1, (const char *) bContentPathName);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }

                        if (detSourceFileType != FileIO:: TOOLS_FILEIO_DIRECTORY &&
                                detSourceFileType != FileIO:: TOOLS_FILEIO_REGULARFILE)
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_WRONGDIRECTORYENTRYTYPE,
                                        2, (long) detSourceFileType, bContentPathName. str());
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);
                                }

                                return err;
                        }

                        if (detSourceFileType == FileIO:: TOOLS_FILEIO_DIRECTORY)
                        {
                                char						pRetentionDate [
                                        CMSREP_CMSREPOSITORY_MAXDATETIMELENGTH];
                                const char					*pDirectoryName;
                                tm							tmDateTime;
                                unsigned long				ulMilliSecs;
                                unsigned long				ulRetentionYear;
                                unsigned long				ulRetentionMonth;
                                unsigned long				ulRetentionDay;
                                unsigned long				ulRetentionHour;
                                unsigned long				ulRetentionMinutes;
                                unsigned long				ulRetentionSeconds;
                                Boolean_t					bRetentionDaylightSavingTime;


                                #ifdef WIN32
                                        if ((pDirectoryName = strrchr (bContentPathName. str(),
                                                '\\')) == (const char *) NULL)
                                #else
                                        if ((pDirectoryName = strrchr (bContentPathName. str(),
                                                '/')) == (const char *) NULL)
                                #endif
                                {
                                        pDirectoryName		= bContentPathName. str();
                                }
                                else
                                {
                                        pDirectoryName++;
                                }

                                if (!isdigit(pDirectoryName [0]) ||
                                        !isdigit(pDirectoryName [1]) ||
                                        !isdigit(pDirectoryName [2]) ||
                                        !isdigit(pDirectoryName [3]) ||
                                        pDirectoryName [4] != '_' ||
                                        !isdigit(pDirectoryName [5]) ||
                                        !isdigit(pDirectoryName [6]) ||
                                        pDirectoryName [7] != '_' ||
                                        !isdigit(pDirectoryName [8]) ||
                                        !isdigit(pDirectoryName [9]) ||
                                        pDirectoryName [10] != '\0')
                                {
                                        // we expect a directory having a format YYYY_MM_DD

                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
                                                2, pContentsDirectory, pDirectoryName);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (_bUnexpectedFilesToBeRemoved)
                                        {
                                                (*pulCurrentDirectoriesRemovedNumberInThisSchedule)	+=
                                                        1;

                                                {
                                                        Message msg = CMSRepositoryMessages (
                                                                __FILE__, __LINE__,
                                                                CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
                                                                2, pContentsDirectory, pDirectoryName);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                                                (const char *) msg, __FILE__, __LINE__);
                                                }

                                                if ((errFileIO = FileIO:: removeDirectory (
                                                        (const char *) bContentPathName, true)) !=
                                                        errNoError)
                                                {
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) errFileIO, __FILE__, __LINE__);

                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_REMOVEDIRECTORY_FAILED,
                                                                1, bContentPathName. str ());
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);
                                                }
                                        }

                                        continue;
                                }

                                if (DateTime:: get_tm_LocalTime (&tmDateTime, &ulMilliSecs) !=
                                        errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_DATETIME_GET_TM_LOCALTIME_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                if (DateTime:: addSeconds (
                                        tmDateTime. tm_year + 1900,
                                        tmDateTime. tm_mon + 1,
                                        tmDateTime. tm_mday,
                                        tmDateTime. tm_hour,
                                        tmDateTime. tm_min,
                                        tmDateTime. tm_sec,
                                        tmDateTime. tm_isdst,
                                        _ulRetentionPeriodInSecondsForTemporaryFiles * -1,
                                        &ulRetentionYear,
                                        &ulRetentionMonth,
                                        &ulRetentionDay,
                                        &ulRetentionHour,
                                        &ulRetentionMinutes,
                                        &ulRetentionSeconds,
                                        &bRetentionDaylightSavingTime
                                        ) != errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_DATETIME_GET_TM_LOCALTIME_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                sprintf (pRetentionDate, "%04lu_%02lu_%02lu",
                                        ulRetentionYear, ulRetentionMonth, ulRetentionDay);

                                if (strcmp (pDirectoryName, pRetentionDate) < 0)
                                {
                                        // remove directory
                                        if (_bUnexpectedFilesToBeRemoved)
                                        {
                                                (*pulCurrentDirectoriesRemovedNumberInThisSchedule)	+=
                                                        1;

                                                {
                                                        Message msg = CMSRepositoryMessages (
                                                                __FILE__, __LINE__,
                                                                CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
                                                                2, pCustomerDirectoryName,
                                                                bContentPathName. str());
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                                                (const char *) msg, __FILE__, __LINE__);
                                                }

                                                if ((errFileIO = FileIO:: removeDirectory (
                                                        bContentPathName. str(), true)) !=
                                                        errNoError)
                                                {
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) errFileIO, __FILE__, __LINE__);

                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_REMOVEDIRECTORY_FAILED,
                                                                1, bContentPathName. str ());
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);

                                                        if (FileIO:: closeDirectory (
                                                                &dContentsDirectory) != errNoError)
                                                        {
                                                                Error err = ToolsErrors (
                                                                        __FILE__, __LINE__,
                                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                                _ptSystemTracer -> trace (
                                                                        Tracer:: TRACER_LERRR,
                                                                        (const char *) err,
                                                                        __FILE__, __LINE__);
                                                        }

                                                        return err;
                                                }
                                        }
                                }
                        }
                        else // if (detSourceFileType ==
                                // FileIO:: TOOLS_FILEIO_REGULARFILE)
                        {
                                if ((errFileIO = FileIO:: getFileTime (
                                        (const char *) bContentPathName,
                                        &tLastModificationTime)) != errNoError)
                                {
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) errFileIO, __FILE__, __LINE__);

                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_FILEIO_GETFILETIME_FAILED,
                                                1, (const char *) bContentPathName);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                if (((unsigned long) (time (NULL) -
                                        tLastModificationTime)) >=
                                        _ulRetentionPeriodInSecondsForTemporaryFiles)
                                {
//					Error err = CMSRepositoryErrors (__FILE__, __LINE__,
//					CMSREP_CMSENGINEPROCESSOR_SANITYCHECKFILETOOOLDTOBEREMOVED,
//						1, (const char *) bContentPathName);
//					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//						(const char *) err, __FILE__, __LINE__);

                                        if (_bUnexpectedFilesToBeRemoved)
                                        {
                                                (*pulCurrentFilesRemovedNumberInThisSchedule)	+= 1;

                                                {
                                                        Message msg = CMSRepositoryMessages (
                                                                __FILE__, __LINE__,
                                                                CMSREP_CMSREPOSITORY_REMOVEFILE,
                                                                2, pCustomerDirectoryName,
                                                                bContentPathName. str());
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                                                (const char *) msg, __FILE__, __LINE__);
                                                }

                                                if ((errFileIO = FileIO:: remove (
                                                        bContentPathName. str())) != errNoError)
                                                {
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) errFileIO, __FILE__, __LINE__);

                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_REMOVE_FAILED,
                                                                1, (const char *) bContentPathName);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);

                                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                                errNoError)
                                                        {
                                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                        (const char *) err, __FILE__, __LINE__);
                                                        }

                                                        return err;
                                                }
                                        }
                                }
                        }
                }
                else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_FTP)
                {
                        {
                                Boolean_t				bIsIngestionLogFile;


                                if (!strcmp ((const char *) bContentPathName, "Ingestion.log"))
                                {
                                        bIsIngestionLogFile			= true;
                                }
                                else
                                {
                                        bIsIngestionLogFile			= false;
                                }

                                if (bContentPathName. insertAt (0,
                                        pContentsDirectory) != errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_BUFFER_INSERTAT_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                if (!bIsIngestionLogFile)
                                {
                                        time_t							tLastModificationTime;
                                        FileIO:: DirectoryEntryType_t	detSourceFileType;


                                        if ((errFileIO = FileIO:: getDirectoryEntryType (
                                                (const char *) bContentPathName, &detSourceFileType)) !=
                                                errNoError)
                                        {
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) errFileIO, __FILE__, __LINE__);

                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
                                                        1, (const char *) bContentPathName);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                        errNoError)
                                                {
                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);
                                                }

                                                return err;
                                        }

                                        if ((errFileIO = FileIO:: getFileTime (
                                                (const char *) bContentPathName,
                                                &tLastModificationTime)) != errNoError)
                                        {
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) errFileIO,
                                                        __FILE__, __LINE__);

                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_GETFILETIME_FAILED,
                                                        1,
                                                        (const char *) bContentPathName);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                        errNoError)
                                                {
                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);
                                                }

                                                return err;
                                        }

                                        if (((unsigned long) (time (NULL) -
                                                tLastModificationTime)) >=
                                                _ulRetentionPeriodInSecondsForTemporaryFiles)
                                        {
//						Error err = CMSRepositoryErrors (__FILE__, __LINE__,
//						CMSREP_CMSENGINEPROCESSOR_SANITYCHECKFILETOOOLDTOBEREMOVED,
//							1, (const char *) bContentPathName);
//						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//							(const char *) err, __FILE__, __LINE__);

                                                if (_bUnexpectedFilesToBeRemoved)
                                                {
                                                        if (detSourceFileType ==
                                                                FileIO:: TOOLS_FILEIO_DIRECTORY)
                                                        {
                                                        (*pulCurrentDirectoriesRemovedNumberInThisSchedule)
                                                                        += 1;

                                                                {
                                                                        Message msg = CMSRepositoryMessages (
                                                                                __FILE__, __LINE__,
                                                                                CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
                                                                                2,
                                                                                pCustomerDirectoryName,
                                                                                (const char *) bContentPathName);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LINFO,
                                                                                (const char *) msg, __FILE__, __LINE__);
                                                                }

                                                                if ((errFileIO = FileIO:: removeDirectory (
                                                                        (const char *) bContentPathName, true)) !=
                                                                        errNoError)
                                                                {
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) errFileIO,
                                                                                __FILE__, __LINE__);

                                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                                TOOLS_FILEIO_REMOVEDIRECTORY_FAILED,
                                                                                1,
                                                                                (const char *) bContentPathName);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) err, __FILE__, __LINE__);

                                                                        if (FileIO:: closeDirectory (
                                                                                &dContentsDirectory) != errNoError)
                                                                        {
                                                                                Error err = ToolsErrors (
                                                                                        __FILE__, __LINE__,
                                                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                                                _ptSystemTracer -> trace (
                                                                                        Tracer:: TRACER_LERRR,
                                                                                        (const char *) err,
                                                                                        __FILE__, __LINE__);
                                                                        }

                                                                        return err;
                                                                }
                                                        }
                                                        else // if (detSourceFileType ==
                                                                // FileIO:: TOOLS_FILEIO_REGULARFILE)
                                                        {
                                                        (*pulCurrentFilesRemovedNumberInThisSchedule)
                                                                        += 1;

                                                                {
                                                                        Message msg = CMSRepositoryMessages (
                                                                                __FILE__, __LINE__,
                                                                                CMSREP_CMSREPOSITORY_REMOVEFILE,
                                                                                2,
                                                                                pCustomerDirectoryName,
                                                                                (const char *) bContentPathName);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LINFO,
                                                                                (const char *) msg, __FILE__, __LINE__);
                                                                }

                                                                if ((errFileIO = FileIO:: remove (
                                                                        (const char *) bContentPathName)) !=
                                                                        errNoError)
                                                                {
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) errFileIO,
                                                                                __FILE__, __LINE__);

                                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                                TOOLS_FILEIO_REMOVE_FAILED,
                                                                                1,
                                                                                (const char *) bContentPathName);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) err, __FILE__, __LINE__);

                                                                        if (FileIO:: closeDirectory (
                                                                                &dContentsDirectory) != errNoError)
                                                                        {
                                                                                Error err = ToolsErrors (
                                                                                        __FILE__, __LINE__,
                                                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                                                _ptSystemTracer -> trace (
                                                                                        Tracer:: TRACER_LERRR,
                                                                                        (const char *) err,
                                                                                        __FILE__, __LINE__);
                                                                        }

                                                                        return err;
                                                                }
                                                        }
                                                }
                                        }
                                }
                                else
                                {
                                        // It is the Ingestion.log file.
                                        // It will be removed only if too big
                                        unsigned long				ulFileSizeInBytes;


                                        if ((errFileIO = FileIO:: getFileSizeInBytes (
                                                (const char *) bContentPathName,
                                                &ulFileSizeInBytes, false)) != errNoError)
                                        {
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) errFileIO,
                                                        __FILE__, __LINE__);

                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_GETFILESIZEINBYTES_FAILED,
                                                        1,
                                                        (const char *) bContentPathName);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                        errNoError)
                                                {
                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);
                                                }

                                                return err;
                                        }

                                        // the below check is in KB.
                                        // Remove if it is too big (10MB)
                                        if (ulFileSizeInBytes / 1024 >= 10 * 1024)
                                        {
                                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                CMSREP_CMSREPOSITORY_SANITYCHECKFILETOOBIGTOBEREMOVED,
                                                        2, (unsigned long) (ulFileSizeInBytes / 1024),
                                                        (const char *) bContentPathName);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                if (_bUnexpectedFilesToBeRemoved)
                                                {
                                                        (*pulCurrentFilesRemovedNumberInThisSchedule)
                                                                += 1;

                                                        {
                                                                Message msg = CMSRepositoryMessages (
                                                                        __FILE__, __LINE__,
                                                                        CMSREP_CMSREPOSITORY_REMOVEFILE,
                                                                        2,
                                                                        pCustomerDirectoryName,
                                                                        (const char *) bContentPathName);
                                                                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                                                        (const char *) msg, __FILE__, __LINE__);
                                                        }

                                                        if ((errFileIO = FileIO:: remove (
                                                                (const char *) bContentPathName)) !=
                                                                errNoError)
                                                        {
                                                                _ptSystemTracer -> trace (
                                                                        Tracer:: TRACER_LERRR,
                                                                        (const char *) errFileIO,
                                                                        __FILE__, __LINE__);

                                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                                        TOOLS_FILEIO_REMOVE_FAILED,
                                                                        1,
                                                                        (const char *) bContentPathName);
                                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                        (const char *) err, __FILE__, __LINE__);

                                                                if (FileIO:: closeDirectory (
                                                                        &dContentsDirectory) != errNoError)
                                                                {
                                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) err, __FILE__, __LINE__);
                                                                }

                                                                return err;
                                                        }
                                                }
                                        }
                                }
                        }
                }
                else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
                        rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING ||
                        rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
                {
                        if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_DIRECTORY)
                        {
                                if (*pulDirectoryLevelIndexInsideCustomer == 4 &&
                                        rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
                                {
                                        // In the scenario where
                                        // *pulDirectoryLevelIndexInsideCustomer == 4 &&
                                        //	rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER
                                        //	we should have the IPhone and that has not to be going
                                        //	through by the sanity check

                                        continue;
                                }

                                if (bContentPathName. insertAt (0,
                                        pContentsDirectory) != errNoError)
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_BUFFER_INSERTAT_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                #ifdef WIN32
                                        if (bContentPathName. append ("\\") != errNoError)
                                #else
                                        if (bContentPathName. append ("/") != errNoError)
                                #endif
                                {
                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                TOOLS_BUFFER_APPEND_FAILED);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                if (!strcmp ((const char *) bContentPathName,
                                        (const char *) _bProfilesRootRepository))
                                {
                                        // profiles directory

                                        continue;
                                }

 *pulDirectoryLevelIndexInsideCustomer			+= 1;

                                if ((errSanityCheck = sanityCheck_ContentsDirectory (
                                        pCustomerDirectoryName,
                                        (const char *) bContentPathName,
                                        ulRelativePathIndex,
                                        rtRepositoryType,
                                        psciSanityCheckContentsInfo,
                                        plSanityCheckContentsInfoCurrentIndex,
                                        pulFileIndex,
                                        pulCurrentFileNumberProcessedInThisSchedule,
                                        pulCurrentFilesRemovedNumberInThisSchedule,
                                        pulCurrentDirectoriesRemovedNumberInThisSchedule,
                                        pulDirectoryLevelIndexInsideCustomer)) != errNoError)
                                {
                                        if ((unsigned long) errSanityCheck ==
                                                CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED)
                                        {
 *pulDirectoryLevelIndexInsideCustomer			-= 1;

                                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                        errNoError)
                                                {
                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);
                                                }

                                                return errSanityCheck;
                                        }
                                        else
                                        {
                                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                // we will continue the sanity check in order that
                                                // the other directories can be verified and
                                                // we will stop it

//						*pulDirectoryLevelIndexInsideCustomer			-= 1;
//
//						if (FileIO:: closeDirectory (&dContentsDirectory) !=
//							errNoError)
//						{
//							Error err = ToolsErrors (__FILE__, __LINE__,
//								TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//								(const char *) err, __FILE__, __LINE__);
//						}
//
//						return err;
                                        }
                                }

 *pulDirectoryLevelIndexInsideCustomer			-= 1;
                        }
                        else // TOOLS_FILEIO_REGULARFILE || TOOLS_FILEIO_LINKFILE)
                        {
                                if (*pulFileIndex <
                                        _svlpcLastProcessedContent [rtRepositoryType].
                                        _ulFilesNumberAlreadyProcessed)
                                {
                                        // file already processed

                                        continue;
                                }

                                if (*pulCurrentFileNumberProcessedInThisSchedule >=
                                        _ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType])
                                {
                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED,
                                                2, 
                                                (const char *) (*(_pbRepositories [rtRepositoryType])),
                                                _ulMaxFilesToBeProcessedPerSchedule [rtRepositoryType]);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);
                                        }

                                        return err;
                                }

                                (*pulCurrentFileNumberProcessedInThisSchedule)		+= 1;

                                (_svlpcLastProcessedContent [rtRepositoryType].
                                        _ulFilesNumberAlreadyProcessed)					+= 1;

                                {
                                        const char				*pRelativePath;
                                        char					pTerritoryName [
                                                CMSREP_CMSREPOSITORY_MAXTERRITORYNAME];


                                        // retrieve the TerritoryName and RelativePath
                                        if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
                                                rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
                                        {
                                                #ifdef WIN32
                                                        pRelativePath	= strchr (
                                                                pContentsDirectory + ulRelativePathIndex + 1,
                                                                '\\');
                                                #else
                                                        pRelativePath	= strchr (
                                                                pContentsDirectory + ulRelativePathIndex + 1,
                                                                '/');
                                                #endif

                                                if (pRelativePath == (const char *) NULL)
                                                {
                                                        // we expect a directory (the territory) but
                                                        // we found a file
                                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                        CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
                                                                2, pContentsDirectory,
                                                                (const char *) bContentPathName);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);

                                                        if (_bUnexpectedFilesToBeRemoved)
                                                        {
                                                                if (bContentPathName. insertAt (0,
                                                                        pContentsDirectory) != errNoError)
                                                                {
                                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                                TOOLS_BUFFER_INSERTAT_FAILED);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) err, __FILE__, __LINE__);

                                                                        if (FileIO:: closeDirectory (
                                                                                &dContentsDirectory) != errNoError)
                                                                        {
                                                                                Error err = ToolsErrors (
                                                                                        __FILE__, __LINE__,
                                                                                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                                                _ptSystemTracer -> trace (
                                                                                        Tracer:: TRACER_LERRR,
                                                                                        (const char *) err,
                                                                                        __FILE__, __LINE__);
                                                                        }

                                                                        return err;
                                                                }

                                                        (*pulCurrentFilesRemovedNumberInThisSchedule)
                                                                        += 1;

                                                                {
                                                                        Message msg = CMSRepositoryMessages (
                                                                                __FILE__, __LINE__,
                                                                                CMSREP_CMSREPOSITORY_REMOVEFILE,
                                                                                2,
                                                                                pCustomerDirectoryName,
                                                                                (const char *) bContentPathName);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LINFO,
                                                                                (const char *) msg, __FILE__, __LINE__);
                                                                }

                                                                // this is just a link because we are
                                                                // in the Download or Streaming repository
                                                                // or a file in case of playlist
                                                                if ((errFileIO = FileIO:: remove (
                                                                        (const char *) bContentPathName)) !=
                                                                        errNoError)
                                                                {
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) errFileIO,
                                                                                __FILE__, __LINE__);

                                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                                TOOLS_FILEIO_REMOVE_FAILED,
                                                                                1,
                                                                                (const char *) bContentPathName);
                                                                        _ptSystemTracer -> trace (
                                                                                Tracer:: TRACER_LERRR,
                                                                                (const char *) err, __FILE__, __LINE__);

//									if (FileIO:: closeDirectory (
//										&dContentsDirectory) != errNoError)
//									{
//										Error err = ToolsErrors (
//											__FILE__, __LINE__,
//											TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//										_ptSystemTracer -> trace (
//											Tracer:: TRACER_LERRR,
//											(const char *) err,
//											__FILE__, __LINE__);
//									}
//
//									return err;
                                                                }
                                                        }

                                                        continue;

//							if (FileIO:: closeDirectory (&dContentsDirectory) !=
//								errNoError)
//							{
//								Error err = ToolsErrors (__FILE__, __LINE__,
//									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//									(const char *) err, __FILE__, __LINE__);
//							}
//
//							return err;
                                                }

                                                strncpy (pTerritoryName,
                                                        pContentsDirectory + ulRelativePathIndex + 1,
                                                        pRelativePath -
                                                        (pContentsDirectory + ulRelativePathIndex + 1));
                                                pTerritoryName [pRelativePath -
                                                        (pContentsDirectory + ulRelativePathIndex + 1)]	=
                                                                '\0';
                                        }
                                        else
                                        {
                                                pRelativePath	=
                                                        pContentsDirectory + ulRelativePathIndex;

                                                strcpy (pTerritoryName, "null");
                                        }

                                        if ((*plSanityCheckContentsInfoCurrentIndex) + 1 >=
                                                CMSREP_CMSREPOSITORY_MAXSANITYCHECKCONTENTSINFONUMBER)
                                        {
                                                {
                                                        Message msg = CMSRepositoryMessages (
                                                                __FILE__, __LINE__,
                                                                CMSREP_CMSREPOSITORY_SANITYCHECKSTATUS,
                                                                3,
                                                                (const char *) (*(
                                                                        _pbRepositories [rtRepositoryType])),
                                                                (*pulFileIndex),
                                                                (*pulCurrentFileNumberProcessedInThisSchedule));
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                                                (const char *) msg, __FILE__, __LINE__);
                                                }

                                                if (sanityCheck_runOnContentsInfo (
                                                        psciSanityCheckContentsInfo,
 *plSanityCheckContentsInfoCurrentIndex,
                                                        rtRepositoryType,
                                                        pulCurrentFilesRemovedNumberInThisSchedule,
                                                        pulCurrentDirectoriesRemovedNumberInThisSchedule) !=
                                                        errNoError)
                                                {
                                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_SANITYCHECK_RUNONCONTENTSINFO_FAILED);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);

//							if (FileIO:: closeDirectory (&dContentsDirectory) !=
//								errNoError)
//							{
//								Error err = ToolsErrors (__FILE__, __LINE__,
//									TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
//								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
//									(const char *) err, __FILE__, __LINE__);
//							}
//
//							return err;
                                                }

 *plSanityCheckContentsInfoCurrentIndex		= 0;
                                        }
                                        else
                                        {
                                                (*plSanityCheckContentsInfoCurrentIndex)		+= 1;
                                        }
 
                                        (psciSanityCheckContentsInfo [
 *plSanityCheckContentsInfoCurrentIndex]).
                                                        _ulContentFound		= 2;
                                        (psciSanityCheckContentsInfo [
 *plSanityCheckContentsInfoCurrentIndex]).
                                                        _ulPublishingStatus	= 2;

                                        if ((psciSanityCheckContentsInfo [
 *plSanityCheckContentsInfoCurrentIndex]).
                                                _bContentsDirectory. setBuffer (pContentsDirectory) !=
                                                errNoError ||

                                                (psciSanityCheckContentsInfo [
 *plSanityCheckContentsInfoCurrentIndex]).
                                                _bCustomerDirectoryName. setBuffer (pCustomerDirectoryName) !=
                                                errNoError ||

                                                (psciSanityCheckContentsInfo [
 *plSanityCheckContentsInfoCurrentIndex]).
                                                        _bTerritoryName. setBuffer (pTerritoryName) !=
                                                        errNoError ||

                                                (psciSanityCheckContentsInfo [
 *plSanityCheckContentsInfoCurrentIndex]).
                                                        _bRelativePath. setBuffer (pRelativePath) !=
                                                        errNoError ||

                                                (psciSanityCheckContentsInfo [
 *plSanityCheckContentsInfoCurrentIndex]). _bFileName.
                                                setBuffer ((const char *) bContentPathName) !=
                                                errNoError)
                                        {
                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                        TOOLS_BUFFER_SETBUFFER_FAILED);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                if (FileIO:: closeDirectory (&dContentsDirectory) !=
                                                        errNoError)
                                                {
                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);
                                                }

                                                return err;
                                        }
                                }
                        }
                }
        }

        if (FileIO:: closeDirectory (&dContentsDirectory) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_CLOSEDIRECTORY_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }


        return errNoError;
}
 */

/*
Error CMSStorage:: sanityCheck_runOnContentsInfo (
        SanityCheckContentInfo_p psciSanityCheckContentsInfo,
        unsigned long ulSanityCheckContentsInfoCurrentIndex,
        RepositoryType rtRepositoryType,
        unsigned long *pulCurrentFilesRemovedNumberInThisSchedule,
        unsigned long *pulCurrentDirectoriesRemovedNumberInThisSchedule)

{

        Buffer_t					bURIForHttpPost;
        Buffer_t					bURLParametersForHttpPost;
        Buffer_t					bHttpPostBodyRequest;
        Error_t						errGetAvailableModule;
        HttpPostThread_t			hgPostSanityCheckContentInfo;
        Error_t						errRun;
        Buffer_t					bHttpPostBodyResponse;
        char						pWebServerIPAddress [
                SCK_MAXIPADDRESSLENGTH];
        unsigned long				ulWebServerPort;
        unsigned long				ulSanityCheckContentInfoIndex;



        if (bHttpPostBodyResponse. init () != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        if (bURIForHttpPost. init (
                "/CMSEngine/getSanityCheckContentInfo") != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        if (bURLParametersForHttpPost. init ("") != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        if (bHttpPostBodyRequest. init (
                "<?xml version=\"1.0\" encoding=\"utf-8\"?> <GetSanityCheckContentInfo> ") !=
                errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        for (ulSanityCheckContentInfoIndex = 0;
                ulSanityCheckContentInfoIndex <= ulSanityCheckContentsInfoCurrentIndex;
                ulSanityCheckContentInfoIndex++)
        {
                if (bHttpPostBodyRequest. append (
                        "<ContentToBeChecked> ") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "<Identifier><![CDATA[") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        ulSanityCheckContentInfoIndex) !=
                        errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "]]></Identifier>") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "<CMSRepository><![CDATA[") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append ((long) rtRepositoryType) !=
                        errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "]]></CMSRepository>") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "<CustomerDirectoryName><![CDATA[") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        (const char *)
                                ((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
                                _bCustomerDirectoryName)) !=
                        errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "]]></CustomerDirectoryName>") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "<TerritoryName><![CDATA[") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        (const char *)
                                ((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
                                _bTerritoryName)) !=
                        errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "]]></TerritoryName>") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "<RelativePath><![CDATA[") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        (const char *)
                                ((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
                                _bRelativePath)) !=
                        errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "]]></RelativePath>") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "<FileName><![CDATA[") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        (const char *)
                                ((psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex]).
                                _bFileName)) !=
                        errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "]]></FileName>") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }

                if (bHttpPostBodyRequest. append (
                        "</ContentToBeChecked> ") != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_BUFFER_APPEND_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        return err;
                }
        }

        if (bHttpPostBodyRequest. append (
                "</GetSanityCheckContentInfo> ") !=
                errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_BUFFER_APPEND_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        if ((errGetAvailableModule =
                _plbWebServerLoadBalancer -> getAvailableModule (
                "WebServers", pWebServerIPAddress,
                SCK_MAXIPADDRESSLENGTH,
                &ulWebServerPort)) != errNoError)
        {
                Error err = LoadBalancerErrors (
                        __FILE__, __LINE__,
                        LB_LOADBALANCER_GETAVAILABLEMODULE_FAILED,
                        1, "WebServers");
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        {
                Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
                        CMSREP_HTTPPOSTTHREAD,
                        8,
                        "getSanityCheckContentInfo",
                        pWebServerIPAddress,
                        ulWebServerPort,
                        _pWebServerLocalIPAddress,
                        (const char *) bURIForHttpPost,
                        (const char *) bURLParametersForHttpPost,
                        (const char *) bHttpPostBodyRequest,
                        _ulWebServerTimeoutToWaitAnswerInSeconds);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (hgPostSanityCheckContentInfo. init (
                pWebServerIPAddress,
                ulWebServerPort,
                (const char *) bURIForHttpPost,
                (const char *) bHttpPostBodyRequest,
                (const char *) bURLParametersForHttpPost,
                (const char *) NULL,	// Cookie
                "CMS Engine",
                _ulWebServerTimeoutToWaitAnswerInSeconds,
                0,
                _ulWebServerTimeoutToWaitAnswerInSeconds,
                0,
                _pWebServerLocalIPAddress) != errNoError)
        {
                Error err = WebToolsErrors (__FILE__, __LINE__,
                        WEBTOOLS_HTTPPOSTTHREAD_INIT_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        if ((errRun = hgPostSanityCheckContentInfo. run (
                (Buffer_p) NULL, &bHttpPostBodyResponse,
                (Buffer_p) NULL)) != errNoError)
        {
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) errRun, __FILE__, __LINE__);

                Error err = PThreadErrors (__FILE__, __LINE__,
                        THREADLIB_PTHREAD_RUN_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                if (hgPostSanityCheckContentInfo. finish () !=
                        errNoError)
                {
                        Error err = WebToolsErrors (__FILE__, __LINE__,
                                WEBTOOLS_HTTPPOSTTHREAD_FINISH_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);
                }

                return err;
        }

        if (strstr ((const char *) bHttpPostBodyResponse,
                "<Status><![CDATA[SUCCESS") == (char *) NULL)
        {
                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                        CMSREP_SERVLETFAILED,
                        4, "getSanityCheckContentInfo", pWebServerIPAddress,
                        "",
                        (const char *) bHttpPostBodyResponse);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                if (hgPostSanityCheckContentInfo. finish () !=
                        errNoError)
                {
                        Error err = WebToolsErrors (__FILE__, __LINE__,
                                WEBTOOLS_HTTPPOSTTHREAD_FINISH_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);
                }

                return err;
        }

        {
                bHttpPostBodyResponse. substitute (CMSREP_CMSREPOSITORY_NEWLINE, " ");

                Message msg = CMSRepositoryMessages (__FILE__, __LINE__,
                        CMSREP_HTTPRESPONSE,
                        5,
                        "<not available>",
                        "getSanityCheckContentInfo",
                        "",
                        (const char *) bHttpPostBodyResponse,
                        (unsigned long) hgPostSanityCheckContentInfo);
                _ptSystemTracer -> trace (Tracer:: TRACER_LDBG6, (const char *) msg,
                        __FILE__, __LINE__);
        }

        if (hgPostSanityCheckContentInfo. finish () !=
                errNoError)
        {
                Error err = WebToolsErrors (__FILE__, __LINE__,
                        WEBTOOLS_HTTPPOSTTHREAD_FINISH_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        // parse the XML response and sanity check on the Content
        {
                unsigned long						ulResponseContentsInfoNumber;
                Boolean_t							bContentToBeRemoved;

                // parse the XML response
                {
                        xmlDocPtr			pxdXMLDocument;
                        xmlNodePtr			pxnXMLCMSNode;
                        xmlNodePtr			pxnXMLContentInfoNode;
                        xmlChar				*pxcValue;
                        unsigned long		ulLocalContentInfoIdentifier;


                        if ((pxdXMLDocument = xmlParseMemory (
                                (const char *) bHttpPostBodyResponse,
                                (unsigned long) bHttpPostBodyResponse)) ==
                                (xmlDocPtr) NULL)
                        {
                                // parse error
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_LIBXML2_XMLPARSEMEMORY_FAILED,
                                        2,
                                        "getSanityCheckContentInfo",
                                        (const char *) bHttpPostBodyResponse);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                return err;
                        }

                        // CMS
                        if ((pxnXMLCMSNode = xmlDocGetRootElement (pxdXMLDocument)) ==
                                (xmlNodePtr) NULL)
                        {
                                // empty document
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_LIBXML2_XMLDOCROOTELEMENT_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                xmlFreeDoc (pxdXMLDocument);

                                return err;
                        }

                        while (pxnXMLCMSNode != (xmlNodePtr) NULL &&
                                xmlStrcmp (pxnXMLCMSNode -> name,
                                (const xmlChar *) "CMS"))
                                pxnXMLCMSNode				= pxnXMLCMSNode -> next;

                        if (pxnXMLCMSNode == (xmlNodePtr) NULL ||
                                (pxnXMLCMSNode = pxnXMLCMSNode -> xmlChildrenNode) ==
                                (xmlNodePtr) NULL)
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_XMLWRONG,
                                        2,
                                        "getSanityCheckContentInfo",
                                        (const char *) bHttpPostBodyResponse);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                xmlFreeDoc (pxdXMLDocument);

                                return err;
                        }

                        ulResponseContentsInfoNumber		= 0;

                        while (pxnXMLCMSNode != (xmlNodePtr) NULL)
                        {
                                if (!xmlStrcmp (pxnXMLCMSNode -> name, (const xmlChar *) 
                                        "ContentInfo"))
                                {
                                        SanityCheckContentInfo_t	sciLocalSanityCheckContentInfo;


                                        ulLocalContentInfoIdentifier			= 999999;

                                        if (ulResponseContentsInfoNumber >=
                                                ulSanityCheckContentsInfoCurrentIndex + 1)
                                        {
                                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                        CMSREP_XMLWRONG,
                                                        2,
                                                        "getSanityCheckContentInfo",
                                                        (const char *) bHttpPostBodyResponse);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                xmlFreeDoc (pxdXMLDocument);

                                                return err;
                                        }

                                        if ((pxnXMLContentInfoNode =
                                                pxnXMLCMSNode -> xmlChildrenNode) ==
                                                (xmlNodePtr) NULL)
                                        {
                                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                        CMSREP_XMLWRONG,
                                                        2,
                                                        "getSanityCheckContentInfo",
                                                        (const char *) bHttpPostBodyResponse);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                xmlFreeDoc (pxdXMLDocument);

                                                return err;
                                        }

                                        while (pxnXMLContentInfoNode != (xmlNodePtr) NULL)
                                        {
                                                if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
                                                        (const xmlChar *) "Identifier"))
                                                {
                                                        if ((pxcValue = xmlNodeListGetString (
                                                                pxdXMLDocument,
                                                                pxnXMLContentInfoNode -> xmlChildrenNode,
                                                                1)) != (xmlChar *) NULL)
                                                        {
                                                                // xmlNodeListGetString NULL means
                                                                // an empty element

                                                                // it will be 0 o 1
                                                                ulLocalContentInfoIdentifier	= strtoul (
                                                                        (const char *) pxcValue, (char **) NULL,
                                                                        10);

                                                                xmlFree (pxcValue);
                                                        }
                                                }
                                                else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
                                                        (const xmlChar *) "ContentFound"))
                                                {
                                                        if ((pxcValue = xmlNodeListGetString (
                                                                pxdXMLDocument,
                                                                pxnXMLContentInfoNode -> xmlChildrenNode,
                                                                1)) != (xmlChar *) NULL)
                                                        {
                                                                // xmlNodeListGetString NULL means
                                                                // an empty element

                                                                // it will be 0 o 1
                                                                sciLocalSanityCheckContentInfo.
                                                                        _ulContentFound			= strtoul (
                                                                        (const char *) pxcValue, (char **) NULL,
                                                                        10);

                                                                xmlFree (pxcValue);
                                                        }
                                                }
                                                else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
                                                        (const xmlChar *) "PublishingStatus"))
                                                {
                                                        if ((pxcValue = xmlNodeListGetString (
                                                                pxdXMLDocument,
                                                                pxnXMLContentInfoNode -> xmlChildrenNode,
                                                                1)) != (xmlChar *) NULL)
                                                        {
                                                                // xmlNodeListGetString NULL means
                                                                // an empty element

                                                                // it will be 0 o 1
                                                                sciLocalSanityCheckContentInfo.
                                                                        _ulPublishingStatus		= strtoul (
                                                                        (const char *) pxcValue, (char **) NULL,
                                                                        10);

                                                                xmlFree (pxcValue);
                                                        }
                                                }
                                                else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
                                                        (const xmlChar *) "text"))
                                                {
                                                }
                                                else if (!xmlStrcmp (pxnXMLContentInfoNode -> name,
                                                        (const xmlChar *) "comment"))
                                                {
                                                }
                                                else
                                                {
                                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                                CMSREP_XMLPARAMETERUNKNOWN,
                                                                2, "getSanityCheckContentInfo",
                                                                (const char *) (pxnXMLContentInfoNode -> name));
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);

                                                        xmlFreeDoc (pxdXMLDocument);

                                                        return err;
                                                }

                                                if ((pxnXMLContentInfoNode =
                                                        pxnXMLContentInfoNode -> next) == (xmlNodePtr) NULL)
                                                {
                                                }
                                        }

                                        if (ulLocalContentInfoIdentifier == 999999 ||
                                                ulLocalContentInfoIdentifier >=
                                                ulSanityCheckContentsInfoCurrentIndex + 1)
                                        {
                                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                        CMSREP_XMLWRONG,
                                                        2,
                                                        "getSanityCheckContentInfo",
                                                        (const char *) bHttpPostBodyResponse);
                                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                        (const char *) err, __FILE__, __LINE__);

                                                xmlFreeDoc (pxdXMLDocument);

                                                return err;
                                        }

                                        (psciSanityCheckContentsInfo [
                                                ulLocalContentInfoIdentifier]). _ulContentFound		=
                                                sciLocalSanityCheckContentInfo. _ulContentFound;
                                        (psciSanityCheckContentsInfo [
                                                ulLocalContentInfoIdentifier]). _ulPublishingStatus	=
                                                sciLocalSanityCheckContentInfo. _ulPublishingStatus;

                                        ulResponseContentsInfoNumber++;
                                }
                                else if (!xmlStrcmp (pxnXMLCMSNode -> name,
                                        (const xmlChar *) "Status"))
                                {
                                        // no check on the Status because it is already
                                        // verified before after the run method
                                }
                                else if (!xmlStrcmp (pxnXMLCMSNode -> name,
                                        (const xmlChar *) "text"))
                                {
                                }
                                else if (!xmlStrcmp (pxnXMLCMSNode -> name,
                                        (const xmlChar *) "comment"))
                                {
                                }
                                else
                                {
                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                CMSREP_XMLPARAMETERUNKNOWN,
                                                2, "getSanityCheckContentInfo",
                                                (const char *) (pxnXMLCMSNode -> name));
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        xmlFreeDoc (pxdXMLDocument);

                                        return err;
                                }

                                if ((pxnXMLCMSNode = pxnXMLCMSNode -> next) ==
                                        (xmlNodePtr) NULL)
                                {
                                }
                        }

                        xmlFreeDoc (pxdXMLDocument);

                        if (ulResponseContentsInfoNumber !=
                                ulSanityCheckContentsInfoCurrentIndex + 1)
                        {
                                Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_XMLWRONG,
                                        2,
                                        "getSanityCheckContentInfo",
                                        (const char *) bHttpPostBodyResponse);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                return err;
                        }
                }

                // sanity check on the Content
                {
                        SanityCheckContentInfo_t			sciLocalSanityCheckContentInfo;
                        Error_t								errFileIO;


                        for (ulSanityCheckContentInfoIndex = 0;
                                ulSanityCheckContentInfoIndex <=
                                ulSanityCheckContentsInfoCurrentIndex;
                                ulSanityCheckContentInfoIndex++)
                        {
                                sciLocalSanityCheckContentInfo		=
                                        psciSanityCheckContentsInfo [ulSanityCheckContentInfoIndex];

                                if (
                                        (sciLocalSanityCheckContentInfo. _ulContentFound != 0 &&
                                        sciLocalSanityCheckContentInfo. _ulContentFound != 1) ||
                                        (sciLocalSanityCheckContentInfo. _ulPublishingStatus != 0 &&
                                        sciLocalSanityCheckContentInfo. _ulPublishingStatus != 1)
                                        )
                                {
                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                                CMSREP_SERVLETFAILED,
                                                4, "getSanityCheckContentInfo", pWebServerIPAddress,
                                                (const char *)
                                                        (sciLocalSanityCheckContentInfo. _bFileName),
                                                (const char *) bHttpPostBodyResponse);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                (const char *) err, __FILE__, __LINE__);

                                        return err;
                                }

                                if (rtRepositoryType == CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
                                {
                                        if (sciLocalSanityCheckContentInfo. _ulContentFound == 0)
                                                bContentToBeRemoved			= true;
                                        else
                                                bContentToBeRemoved			= false;
                                }
                                else if (rtRepositoryType == CMSREP_REPOSITORYTYPE_DOWNLOAD ||
                                        rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
                                {
                                        if (sciLocalSanityCheckContentInfo. _ulContentFound == 0 ||
                                                sciLocalSanityCheckContentInfo. _ulPublishingStatus == 0)
                                        {
                                                bContentToBeRemoved			= true;
                                        }
                                        else
                                        {
                                                bContentToBeRemoved			= false;
                                        }
                                }
                                else
                                {
                                        bContentToBeRemoved			= false;
                                }

                                if (bContentToBeRemoved)
                                {
                                        Error err = CMSRepositoryErrors (__FILE__, __LINE__,
                                        CMSREP_CMSREPOSITORY_SANITYCHECKFILESYSTEMDBNOTCONSISTENT,
                                                8, 
                                                (const char *) (*(_pbRepositories [rtRepositoryType])),
                                                (const char *) (sciLocalSanityCheckContentInfo.
                                                        _bCustomerDirectoryName),
                                                !strcmp ((const char *)
                                                        (sciLocalSanityCheckContentInfo.
                                                        _bTerritoryName),
                                                        "null") ? "" : (const char *)
                                                        (sciLocalSanityCheckContentInfo.
                                                        _bTerritoryName),
                                                (const char *)
                                                        (sciLocalSanityCheckContentInfo.
                                                        _bRelativePath),
                                                (const char *) (sciLocalSanityCheckContentInfo.
                                                        _bFileName),
                                                sciLocalSanityCheckContentInfo. _ulContentFound,
                                                sciLocalSanityCheckContentInfo. _ulPublishingStatus,
                                                _bUnexpectedFilesToBeRemoved);
                                        _ptSystemTracer -> trace (Tracer:: TRACER_LWRNG,
                                                (const char *) err, __FILE__, __LINE__);

                                        if (_bUnexpectedFilesToBeRemoved)
                                        {
                                                if ((sciLocalSanityCheckContentInfo. _bFileName).
                                                        insertAt (0, (const char *)
                                                        (sciLocalSanityCheckContentInfo.
                                                        _bContentsDirectory)) != errNoError)
                                                {
                                                        Error err = ToolsErrors (__FILE__, __LINE__,
                                                                TOOLS_BUFFER_INSERTAT_FAILED);
                                                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                                                (const char *) err, __FILE__, __LINE__);

                                                        return err;
                                                }

                                                if (rtRepositoryType ==
                                                        CMSREP_REPOSITORYTYPE_DOWNLOAD ||
                                                        rtRepositoryType == CMSREP_REPOSITORYTYPE_STREAMING)
                                                {
                                                        (*pulCurrentFilesRemovedNumberInThisSchedule)
                                                                += 1;

                                                        {
                                                                Message msg = CMSRepositoryMessages (
                                                                        __FILE__, __LINE__,
                                                                        CMSREP_CMSREPOSITORY_REMOVEFILE,
                                                                        2,
                                                                        (const char *)
                                                                                (sciLocalSanityCheckContentInfo.
                                                                                _bCustomerDirectoryName),
                                                                        (const char *)
                                                                                (sciLocalSanityCheckContentInfo.
                                                                                _bFileName));
                                                                _ptSystemTracer -> trace (
                                                                        Tracer:: TRACER_LINFO,
                                                                        (const char *) msg, __FILE__, __LINE__);
                                                        }

                                                        // this is just a link or a file in case of playlist
                                                        if ((errFileIO = FileIO:: remove (
                                                                        (const char *)
                                                                                (sciLocalSanityCheckContentInfo.
                                                                                _bFileName))) !=
                                                                errNoError)
                                                        {
                                                                _ptSystemTracer -> trace (
                                                                        Tracer:: TRACER_LERRR,
                                                                        (const char *) errFileIO,
                                                                        __FILE__, __LINE__);

                                                                Error err = ToolsErrors (__FILE__, __LINE__,
                                                                        TOOLS_FILEIO_REMOVE_FAILED,
                                                                        1,
                                                                        (const char *)
                                                                                (sciLocalSanityCheckContentInfo.
                                                                                _bFileName));
                                                                _ptSystemTracer -> trace (
                                                                        Tracer:: TRACER_LERRR,
                                                                        (const char *) err, __FILE__, __LINE__);

                                                                return err;
                                                        }
                                                }
                                                else if (rtRepositoryType ==
                                                        CMSREP_REPOSITORYTYPE_CMSCUSTOMER)
                                                {
                                                        // it could be a directory in case of IPhone
                                                        // streaming content

                                                        (*pulCurrentFilesRemovedNumberInThisSchedule)
                                                                += 1;

                                                        if (moveContentInRepository (
                                                                (const char *)
                                                                        (sciLocalSanityCheckContentInfo.
                                                                        _bFileName),
                                                                CMSStorage:: CMSREP_REPOSITORYTYPE_STAGING,
                                                                (const char *)
                                                                        (sciLocalSanityCheckContentInfo.
                                                                        _bCustomerDirectoryName), true) != errNoError)
                                                        {
                                                                Error err = CMSRepositoryErrors (
                                                                        __FILE__, __LINE__,
                                                CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED);
                                                                _ptSystemTracer -> trace (
                                                                        Tracer:: TRACER_LERRR,
                                                                        (const char *) err, __FILE__, __LINE__);

                                                                return err;
                                                        }
                                                }
                                        }
                                }
                        }
                }
        }


        return errNoError;
}
 */

/*
Error CMSStorage:: saveSanityCheckLastProcessedContent (
        const char *pFilePathName)

{

        int					iFileDescriptor;
        long long			llBytesWritten;




        if (pFilePathName == (const char *) NULL)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_ACTIVATION_WRONG);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        #ifdef WIN32
                if (FileIO:: open (pFilePathName,
                        O_WRONLY | O_TRUNC | O_CREAT,
                        _S_IREAD | _S_IWRITE, &iFileDescriptor) != errNoError)
        #else
                if (FileIO:: open (pFilePathName,
                        O_WRONLY | O_TRUNC | O_CREAT,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
                        &iFileDescriptor) != errNoError)
        #endif
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_OPEN_FAILED, 1, pFilePathName);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        // Repository: CMSREP_REPOSITORYTYPE_CMSCUSTOMER
        {
                {
                        Message msg = CMSRepositoryMessages (
                                __FILE__, __LINE__, 
                                CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
                                4,
                                (long) CMSREP_REPOSITORYTYPE_CMSCUSTOMER,
                                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                        _pPartition,
                                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                        _pCustomerDirectoryName,
                                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                        _ulFilesNumberAlreadyProcessed);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                                (const char *) msg, __FILE__, __LINE__);
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *)
                                (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                _pPartition),
                        CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *)
                                (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                _pCustomerDirectoryName),
                        CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                {
                        char				pUnsignedLongBuffer [128];


                        memset (pUnsignedLongBuffer, '\0', 128);

                        if (sprintf (pUnsignedLongBuffer, "%lu",
                                (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                _ulFilesNumberAlreadyProcessed)) < 0)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_SPRINTF_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                FileIO:: close (iFileDescriptor);

                                return err;
                        }

                        if (FileIO:: writeChars (iFileDescriptor,
                                (char *) pUnsignedLongBuffer,
                                128,
                                &llBytesWritten) != errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_FILEIO_WRITECHARS_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                FileIO:: close (iFileDescriptor);

                                return err;
                        }

                        if (FileIO:: writeChars (iFileDescriptor,
                                (char *) "\n",
                                1,
                                &llBytesWritten) != errNoError)
                        {
                                Error err = ToolsErrors (__FILE__, __LINE__,
                                        TOOLS_FILEIO_WRITECHARS_FAILED);
                                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                        (const char *) err, __FILE__, __LINE__);

                                FileIO:: close (iFileDescriptor);

                                return err;
                        }
                }
        }

        // Repository: CMSREP_REPOSITORYTYPE_DOWNLOAD
        {
        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_DOWNLOAD,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _pPartition),
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _pCustomerDirectoryName),
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (sprintf (pUnsignedLongBuffer, "%lu",
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _ulFilesNumberAlreadyProcessed)) < 0)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_SPRINTF_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) pUnsignedLongBuffer,
                        128,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }
        }

        // Repository: CMSREP_REPOSITORYTYPE_STREAMING
        {
        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_STREAMING,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _pPartition),
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _pCustomerDirectoryName),
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (sprintf (pUnsignedLongBuffer, "%lu",
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _ulFilesNumberAlreadyProcessed)) < 0)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_SPRINTF_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) pUnsignedLongBuffer,
                        128,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }
        }

        // Repository: CMSREP_REPOSITORYTYPE_STAGING
        {
        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_STAGING,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _pPartition),
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _pCustomerDirectoryName),
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (sprintf (pUnsignedLongBuffer, "%lu",
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _ulFilesNumberAlreadyProcessed)) < 0)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_SPRINTF_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) pUnsignedLongBuffer,
                        128,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }
        }

        // Repository: CMSREP_REPOSITORYTYPE_DONE
        {
        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_DONE,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _pPartition),
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _pCustomerDirectoryName),
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (sprintf (pUnsignedLongBuffer, "%lu",
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _ulFilesNumberAlreadyProcessed)) < 0)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_SPRINTF_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) pUnsignedLongBuffer,
                        128,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }
        }

        // Repository: CMSREP_REPOSITORYTYPE_ERRORS
        {
        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_ERRORS,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _pPartition),
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _pCustomerDirectoryName),
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (sprintf (pUnsignedLongBuffer, "%lu",
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _ulFilesNumberAlreadyProcessed)) < 0)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_SPRINTF_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) pUnsignedLongBuffer,
                        128,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }
        }

        // Repository: CMSREP_REPOSITORYTYPE_FTP
        {
        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_FTP,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _pPartition),
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *)
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _pCustomerDirectoryName),
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: writeChars (iFileDescriptor,
                (char *) "\n",
                1,
                &llBytesWritten) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_WRITECHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (sprintf (pUnsignedLongBuffer, "%lu",
                        (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _ulFilesNumberAlreadyProcessed)) < 0)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_SPRINTF_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) pUnsignedLongBuffer,
                        128,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                if (FileIO:: writeChars (iFileDescriptor,
                        (char *) "\n",
                        1,
                        &llBytesWritten) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_WRITECHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }
        }

        if (FileIO:: close (iFileDescriptor) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_CLOSE_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER]	=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD]		=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING]	=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING]		=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE]			=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS]		=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS];
//	_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP]			=
//		pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP];


        return errNoError;
}


Error CMSStorage:: readSanityCheckLastProcessedContent (
        const char *pFilePathName)

{

        int					iFileDescriptor;
        long long			llByteRead;
        char				pEndLine [2];


        if (pFilePathName == (const char *) NULL)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_ACTIVATION_WRONG);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }

        if (FileIO:: open (pFilePathName, O_RDONLY, &iFileDescriptor) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_OPEN_FAILED, 1, pFilePathName);
                _ptSystemTracer -> trace (Tracer:: TRACER_LWRNG,
                        (const char *) err, __FILE__, __LINE__);

                // Repository: CMSREP_REPOSITORYTYPE_CMSCUSTOMER
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                        _pPartition, "");
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                        _pCustomerDirectoryName, "");
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                        _ulFilesNumberAlreadyProcessed		= 0;

                // Repository: CMSREP_REPOSITORYTYPE_DOWNLOAD
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _pPartition, "");
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _pCustomerDirectoryName, "");
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _ulFilesNumberAlreadyProcessed		= 0;

                // Repository: CMSREP_REPOSITORYTYPE_STREAMING
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _pPartition, "");
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _pCustomerDirectoryName, "");
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _ulFilesNumberAlreadyProcessed		= 0;

                // Repository: CMSREP_REPOSITORYTYPE_STAGING
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _pPartition, "");
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _pCustomerDirectoryName, "");
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _ulFilesNumberAlreadyProcessed		= 0;

                // Repository: CMSREP_REPOSITORYTYPE_DONE
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _pPartition, "");
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _pCustomerDirectoryName, "");
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _ulFilesNumberAlreadyProcessed		= 0;

                // Repository: CMSREP_REPOSITORYTYPE_ERRORS
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _pPartition, "");
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _pCustomerDirectoryName, "");
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _ulFilesNumberAlreadyProcessed		= 0;

                // Repository: CMSREP_REPOSITORYTYPE_FTP
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _pPartition, "");
                strcpy (_svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _pCustomerDirectoryName, "");
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _ulFilesNumberAlreadyProcessed		= 0;


                return errNoError;
        }

        // Repository: CMSREP_REPOSITORYTYPE_CMSCUSTOMER
        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                        _pPartition,
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                        _pCustomerDirectoryName,
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (FileIO:: readChars (iFileDescriptor,
                        pUnsignedLongBuffer,
                        128,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                        _ulFilesNumberAlreadyProcessed		=
                        strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

                if (FileIO:: readChars (iFileDescriptor,
                        pEndLine,
                        1,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }

        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_CMSCUSTOMER,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_CMSCUSTOMER].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        // Repository: CMSREP_REPOSITORYTYPE_DOWNLOAD
        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _pPartition,
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _pCustomerDirectoryName,
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (FileIO:: readChars (iFileDescriptor,
                        pUnsignedLongBuffer,
                        128,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                        _ulFilesNumberAlreadyProcessed		=
                        strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

                if (FileIO:: readChars (iFileDescriptor,
                        pEndLine,
                        1,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }

        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_DOWNLOAD,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DOWNLOAD].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }


        // Repository: CMSREP_REPOSITORYTYPE_STREAMING
        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _pPartition,
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _pCustomerDirectoryName,
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (FileIO:: readChars (iFileDescriptor,
                        pUnsignedLongBuffer,
                        128,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                        _ulFilesNumberAlreadyProcessed		=
                        strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

                if (FileIO:: readChars (iFileDescriptor,
                        pEndLine,
                        1,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }

        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_STREAMING,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STREAMING].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        // Repository: CMSREP_REPOSITORYTYPE_STAGING
        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _pPartition,
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _pCustomerDirectoryName,
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (FileIO:: readChars (iFileDescriptor,
                        pUnsignedLongBuffer,
                        128,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                        _ulFilesNumberAlreadyProcessed		=
                        strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

                if (FileIO:: readChars (iFileDescriptor,
                        pEndLine,
                        1,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }

        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_STAGING,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_STAGING].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        // Repository: CMSREP_REPOSITORYTYPE_DONE
        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _pPartition,
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _pCustomerDirectoryName,
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (FileIO:: readChars (iFileDescriptor,
                        pUnsignedLongBuffer,
                        128,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                        _ulFilesNumberAlreadyProcessed		=
                        strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

                if (FileIO:: readChars (iFileDescriptor,
                        pEndLine,
                        1,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }

        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_DONE,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_DONE].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        // Repository: CMSREP_REPOSITORYTYPE_ERRORS
        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _pPartition,
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _pCustomerDirectoryName,
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (FileIO:: readChars (iFileDescriptor,
                        pUnsignedLongBuffer,
                        128,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                        _ulFilesNumberAlreadyProcessed		=
                        strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

                if (FileIO:: readChars (iFileDescriptor,
                        pEndLine,
                        1,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }

        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_ERRORS,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_ERRORS].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        // Repository: CMSREP_REPOSITORYTYPE_FTP
        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _pPartition,
                CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _pCustomerDirectoryName,
                CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        if (FileIO:: readChars (iFileDescriptor,
                pEndLine,
                1,
                &llByteRead) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_READCHARS_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                FileIO:: close (iFileDescriptor);

                return err;
        }

        {
                char				pUnsignedLongBuffer [128];


                memset (pUnsignedLongBuffer, '\0', 128);

                if (FileIO:: readChars (iFileDescriptor,
                        pUnsignedLongBuffer,
                        128,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }

                _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                        _ulFilesNumberAlreadyProcessed		=
                        strtoul (pUnsignedLongBuffer, (char **) NULL, 10);

                if (FileIO:: readChars (iFileDescriptor,
                        pEndLine,
                        1,
                        &llByteRead) != errNoError)
                {
                        Error err = ToolsErrors (__FILE__, __LINE__,
                                TOOLS_FILEIO_READCHARS_FAILED);
                        _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                                (const char *) err, __FILE__, __LINE__);

                        FileIO:: close (iFileDescriptor);

                        return err;
                }
        }

        {
                Message msg = CMSRepositoryMessages (
                        __FILE__, __LINE__, 
                        CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
                        4,
                        (long) CMSREP_REPOSITORYTYPE_FTP,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                                _pPartition,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                                _pCustomerDirectoryName,
                        _svlpcLastProcessedContent [CMSREP_REPOSITORYTYPE_FTP].
                                _ulFilesNumberAlreadyProcessed);
                _ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
                        (const char *) msg, __FILE__, __LINE__);
        }

        if (FileIO:: close (iFileDescriptor) != errNoError)
        {
                Error err = ToolsErrors (__FILE__, __LINE__,
                        TOOLS_FILEIO_CLOSE_FAILED);
                _ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                        (const char *) err, __FILE__, __LINE__);

                return err;
        }


//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER]		=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_CMSCUSTOMER];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD]			=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DOWNLOAD];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING]		=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STREAMING];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING]			=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_STAGING];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE]				=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_DONE];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS]			=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_ERRORS];
//	pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP]				=
//		_pulFilesNumberAlreadyProcessed [CMSREP_REPOSITORYTYPE_FTP];


        return errNoError;
}
 */

