
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

string CMSStorage::getCustomerFTPRepository(shared_ptr<Customer> customer)
{
    string customerFTPDirectory = getFTPRootRepository();
    customerFTPDirectory.append(customer->_name);
    
    if (!FileIO::directoryExisting(customerFTPDirectory)) 
    {
        _logger->info(string("Create directory")
            + ", customerFTPDirectory: " + customerFTPDirectory
        );

        bool noErrorIfExists = true;
        bool recursive = true;
        FileIO::createDirectory(customerFTPDirectory,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP |
                S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    {
        string customerErrorFTPDirectory = customerFTPDirectory;
        customerErrorFTPDirectory
                .append("/")
                .append("ERROR");

        if (!FileIO::directoryExisting(customerErrorFTPDirectory)) 
        {
            _logger->info(string("Create directory")
                + ", customerErrorFTPDirectory: " + customerErrorFTPDirectory
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(customerErrorFTPDirectory,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }
    }

    {
        string customerSuccessFTPDirectory = customerFTPDirectory;
        customerSuccessFTPDirectory
                .append("/")
                .append("SUCCESS");

        if (!FileIO::directoryExisting(customerSuccessFTPDirectory)) 
        {
            _logger->info(string("Create directory")
                + ", customerSuccessFTPDirectory: " + customerSuccessFTPDirectory
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(customerSuccessFTPDirectory,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }
    }

    {
        string customerWorkingFTPDirectory = customerFTPDirectory;
        customerWorkingFTPDirectory
                .append("/")
                .append("WORKING");

        if (!FileIO::directoryExisting(customerWorkingFTPDirectory)) 
        {
            _logger->info(string("Create directory")
                + ", customerWorkingFTPDirectory: " + customerWorkingFTPDirectory
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(customerWorkingFTPDirectory,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }
    }

    return customerFTPDirectory;
}

string CMSStorage::moveFTPRepositoryEntryToWorkingArea(
        shared_ptr<Customer> customer,
        string entryFileName)
{
    string ftpDirectoryEntryPathName = getCustomerFTPRepository(customer);
    ftpDirectoryEntryPathName
            .append("/")
            .append(entryFileName);

    string ftpDirectoryWorkingEntryPathName = getCustomerFTPRepository(customer);
    ftpDirectoryWorkingEntryPathName
        .append("/")
        .append("WORKING")
        .append("/")
        .append(entryFileName);
    
    _logger->info(string("Move file")
        + ", from: " + ftpDirectoryEntryPathName
        + ", to: " + ftpDirectoryWorkingEntryPathName
    );

    FileIO::moveFile(ftpDirectoryEntryPathName, ftpDirectoryWorkingEntryPathName);
            
    return ftpDirectoryWorkingEntryPathName;
}

string CMSStorage::moveFTPRepositoryWorkingEntryToErrorArea(
        shared_ptr<Customer> customer,
        string entryFileName)
{
    string ftpDirectoryWorkingEntryPathName = getCustomerFTPRepository(customer);
    ftpDirectoryWorkingEntryPathName
        .append("/")
        .append("WORKING")
        .append("/")
        .append(entryFileName);

    string ftpDirectoryErrorEntryPathName = getCustomerFTPRepository(customer);
    ftpDirectoryErrorEntryPathName
        .append("/")
        .append("ERROR")
        .append("/")
        .append(entryFileName);

    
    _logger->info(string("Move file")
        + ", from: " + ftpDirectoryWorkingEntryPathName
        + ", to: " + ftpDirectoryErrorEntryPathName
    );

    FileIO::moveFile(ftpDirectoryWorkingEntryPathName, ftpDirectoryErrorEntryPathName);
            
    return ftpDirectoryErrorEntryPathName;
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
        case RepositoryType::CMSREP_REPOSITORYTYPE_CMSCUSTOMER:
        {
            return _cmsRootRepository;
        }
        case RepositoryType::CMSREP_REPOSITORYTYPE_DOWNLOAD:
        {
            return _downloadRootRepository;
        }
        case RepositoryType::CMSREP_REPOSITORYTYPE_STREAMING:
        {
            return _streamingRootRepository;
        }
        case RepositoryType::CMSREP_REPOSITORYTYPE_STAGING:
        {
            return _stagingRootRepository;
        }
        case RepositoryType::CMSREP_REPOSITORYTYPE_DONE:
        {
            return _doneRootRepository;
        }
        case RepositoryType::CMSREP_REPOSITORYTYPE_ERRORS:
        {
            return _errorRootRepository;
        }
        case RepositoryType::CMSREP_REPOSITORYTYPE_FTP:
        {
            return _ftpRootRepository;
        }
        default:
        {
            throw runtime_error(string("Wrong argument")
                    + ", rtRepositoryType: " + to_string(static_cast<int>(rtRepositoryType))
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

    if (rtRepositoryType == RepositoryType::CMSREP_REPOSITORYTYPE_DONE ||
            rtRepositoryType == RepositoryType::CMSREP_REPOSITORYTYPE_STAGING ||
            rtRepositoryType == RepositoryType::CMSREP_REPOSITORYTYPE_ERRORS) 
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

        if (rtRepositoryType == RepositoryType::CMSREP_REPOSITORYTYPE_DONE) 
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
        string destinationAssetFileName,
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
            throw runtime_error(string("Wrong argument")
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

        cmsAssetPathName.append(destinationAssetFileName);
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
        throw runtime_error(string("Wrong argument")
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

