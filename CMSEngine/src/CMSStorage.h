

#ifndef CMSStorage_h
#define CMSStorage_h

#include <mutex>
#include <vector>
#include "spdlog/spdlog.h"
#include "catralibraries/FileIO.h"
#include "Customer.h"
#include "CMSEngineDBFacade.h"


class CMSStorage
{
public:
    enum class RepositoryType
    {
        CMSREP_REPOSITORYTYPE_CMSCUSTOMER	= 0,
        CMSREP_REPOSITORYTYPE_DOWNLOAD,
        CMSREP_REPOSITORYTYPE_STREAMING,
        CMSREP_REPOSITORYTYPE_STAGING,
        CMSREP_REPOSITORYTYPE_DONE,
        CMSREP_REPOSITORYTYPE_ERRORS,
        CMSREP_REPOSITORYTYPE_FTP,

        CMSREP_REPOSITORYTYPE_NUMBER
    };

public:
    CMSStorage (
            string storage, 
            unsigned long freeSpaceToLeaveInEachPartitionInMB,
            shared_ptr<spdlog::logger> logger);

    ~CMSStorage (void);

    string getCustomerFTPRepository(shared_ptr<Customer> customer);
    
    string moveFTPRepositoryEntryToWorkingArea(
        shared_ptr<Customer> customer,
        string entryFileName);

    string moveFTPRepositoryWorkingEntryToErrorArea(
        shared_ptr<Customer> customer,
        string entryFileName);

    string getCustomerFTPWorkingMetadataPathName(
        shared_ptr<Customer> customer,
        string metadataFileName);

    string getCustomerFTPMediaSourcePathName(
        shared_ptr<Customer> customer,
        string mediaSourceFileName);

    //    const char *getIPhoneAliasForLive (void);

    string getCMSRootRepository (void);

    string getStreamingRootRepository (void);

    string getDownloadRootRepository (void);

    string getFTPRootRepository (void);
    
    string getStagingRootRepository (void);

    string getErrorRootRepository (void);

    string getDoneRootRepository (void);

    void refreshPartitionsFreeSizes (void);

    void moveContentInRepository (
        string filePathName,
        RepositoryType rtRepositoryType,
        string customerDirectoryName,
        bool addDateTimeToFileName);

    void copyFileInRepository (
	string filePathName,
	RepositoryType rtRepositoryType,
	string customerDirectoryName,
	bool addDateTimeToFileName);

    string moveAssetInCMSRepository (
        string sourceAssetPathName,
        string customerDirectoryName,
        string destinationFileName,
        string relativePath,

        bool isPartitionIndexToBeCalculated,
        unsigned long *pulCMSPartitionIndexUsed,	// OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

        bool deliveryRepositoriesToo,
        Customer::TerritoriesHashMap& phmTerritories
    );

    string getCMSAssetPathName (
	unsigned long ulPartitionNumber,
	string customerDirectoryName,
	string relativePath,		// using '/'
	string fileName);

    string getDownloadLinkPathName (
	unsigned long ulPartitionNumber,
	string customerDirectoryName,
	string territoryName,
	string relativePath,
	string fileName,
	bool downloadRepositoryToo);

    string getStreamingLinkPathName (
	unsigned long ulPartitionNumber,	// IN
	string customerDirectoryName,	// IN
	string territoryName,	// IN
	string relativePath,	// IN
	string fileName);	// IN

    // bRemoveLinuxPathIfExist: often this method is called 
    // to get the path where the encoder put his output
    // (file or directory). In this case it is good
    // to clean/remove that path if already existing in order
    // to give to the encoder a clean place where to write
    string getStagingAssetPathName (
	string customerDirectoryName,
	string relativePath,
	string fileName,                // may be empty ("")
	long long llMediaItemKey,       // used only if fileName is ""
	long long llPhysicalPathKey,    // used only if fileName is ""
	bool removeLinuxPathIfExist);

    string getEncodingProfilePathName (
	long long llEncodingProfileKey,
	string profileFileNameExtension);

    string getFFMPEGEncodingProfilePathName(
        CMSEngineDBFacade::ContentType contentType,
        long long llEncodingProfileKey);

    unsigned long getCustomerStorageUsage (
	string customerDirectoryName);

private:
    shared_ptr<spdlog::logger>  _logger;

    string                      _hostName;

    string                      _storage;
    string                      _cmsRootRepository;
    string                      _downloadRootRepository;
    string                      _streamingRootRepository;
    string                      _stagingRootRepository;
    string                      _doneRootRepository;
    string                      _errorRootRepository;
    string                      _ftpRootRepository;
    string                      _profilesRootRepository;

    unsigned long long          _freeSpaceToLeaveInEachPartitionInMB;

    recursive_mutex                 _mtCMSPartitions;
    vector<unsigned long long>      _cmsPartitionsFreeSizeInMB;
    unsigned long                   _ulCurrentCMSPartitionIndex;

    
    void contentInRepository (
	unsigned long ulIsCopyOrMove,
	string contentPathName,
	RepositoryType rtRepositoryType,
	string customerDirectoryName,
	bool addDateTimeToFileName);

    string getRepository(RepositoryType rtRepositoryType);

    string creatingDirsUsingTerritories (
	unsigned long ulCurrentCMSPartitionIndex,
	string relativePath,
	string customerDirectoryName,
	bool deliveryRepositoriesToo,
	Customer::TerritoriesHashMap& phmTerritories);


} ;

#endif

