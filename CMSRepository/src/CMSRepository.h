

#ifndef CMSRepository_h
#define CMSRepository_h

#include <mutex>
#include <vector>
#include "catralibraries/FileIO.h"
#include "Customer.h"


class CMSRepository
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

    /*
    struct SanityCheckContentInfo {
        string              _contentsDirectory;
        string              _customerDirectoryName;
        string              _territoryName;
        string              _relativePath;
        string              _fileName;

        // response
        unsigned long       _contentFound;
        unsigned long       _publishingStatus;

        SanityCheckContentInfo (void)
        {
            _contentFound		= 2;
            _publishingStatus		= 2;
        } ;

        ~SanityCheckContentInfo (void)
        {
        } ;

    } SanityCheckContentInfo_t, *SanityCheckContentInfo_p;

    struct SanityCheckLastProcessedContent {
        // i.e.: CMS_0010
        string          _partition;

        // i.e.: GMB
        string          _customerDirectoryName;

        // number of processed contents within the specified partition
        // and CustomerDirectoryName
        unsigned long		_filesNumberAlreadyProcessed;


        SanityCheckLastProcessedContent (void)
        {
//            memset (_pPartition, '\0',
//                    CMSREP_CMSREPOSITORY_MAXCMSPARTITIONNAMELENGTH);
//            memset (_pCustomerDirectoryName, '\0',
//                    CMSREP_CMSREPOSITORY_MAXCUSTOMERNAMELENGTH);

            _filesNumberAlreadyProcessed		= 0;
        } ;

        ~SanityCheckLastProcessedContent (void)
        {
        } ;

    };
     */

private:
    void contentInRepository (
	unsigned long ulIsCopyOrMove,
	string contentPathName,
	RepositoryType rtRepositoryType,
	string customerDirectoryName,
	bool addDateTimeToFileName);

//    Error sanityCheck_CustomersDirectory (
//            RepositoryType rtRepositoryType,
//            const char *pCustomersDirectory,
//            FileIO:: Directory_p pdDeliveryDirectory,
//            SanityCheckContentInfo_p psciSanityCheckContentsInfo,
//            long *plSanityCheckContentsInfoNumber,
//            unsigned long *pulFileIndex,
//            unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
//            unsigned long *pulCurrentContentsRemovedNumberInThisSchedule,
//            unsigned long *pulCurrentOtherFilesRemovedNumberInThisSchedule,
//            unsigned long *pulDirectoryLevelIndexInsideCustomer,
//            Boolean_p pbHasCustomerToBeResumed);
//
//    Error sanityCheck_ContentsDirectory (
//            const char *pCustomerDirectoryName, const char *pContentsDirectory,
//            unsigned long ulRelativePathIndex,
//            RepositoryType rtRepositoryType,
//            SanityCheckContentInfo_p psciSanityCheckContentsInfo,
//            long *plSanityCheckContentsInfoNumber,
//            unsigned long *pulFileIndex,
//            unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
//            unsigned long *pulCurrentContentsRemovedNumberInThisSchedule,
//            unsigned long *pulCurrentOtherFilesRemovedNumberInThisSchedule,
//            unsigned long *pulDirectoryLevelIndexInsideCustomer);
//
//    Error sanityCheck_runOnContentsInfo (
//            SanityCheckContentInfo_p psciSanityCheckContentsInfo,
//            unsigned long ulSanityCheckContentsInfoNumber,
//            RepositoryType rtRepositoryType,
//            unsigned long *pulCurrentContentsRemovedNumberInThisSchedule,
//            unsigned long *pulCurrentOtherFilesRemovedNumberInThisSchedule);

private:
    string                      _hostName;

    string                      _cmsRootRepository;
    string                      _downloadRootRepository;
    string                      _streamingRootRepository;
    string                      _stagingRootRepository;
    string                      _doneRootRepository;
    string                      _errorRootRepository;
    string                      _ftpRootRepository;

    string                      _profilesRootRepository;

//    string                      _iPhoneAliasForLive;
    
//    string                      _downloadReservedDirectoryName;
//    string                      _downloadFreeDirectoryName;
//    string                      _downloadiPhoneLiveDirectoryName;
//    string                      _downloadSilverlightLiveDirectoryName;
//    string                      _downloadAdobeLiveDirectoryName;
//    string                      _streamingFreeDirectoryName;
//    string                      _streamingMetaDirectoryName;
//    string                      _streamingRecordingDirectoryName;

    unsigned long long          _freeSpaceToLeaveInEachPartition;
//    bool                        _unexpectedFilesToBeRemoved;
//    unsigned long               _retentionPeriodInSecondsForTemporaryFiles;
//    unsigned long               _maxFilesToBeProcessedPerSchedule [
//            CMSREP_REPOSITORYTYPE_NUMBER];
//    SanityCheckLastProcessedContent         _lastProcessedContent [
//            CMSREP_REPOSITORYTYPE_NUMBER];


    recursive_mutex                 _mtCMSPartitions;
    vector<unsigned long long>      _cmsPartitionsFreeSizeInMB;
    unsigned long                   _ulCurrentCMSPartitionIndex;


    string getRepository(RepositoryType rtRepositoryType);

    string creatingDirsUsingTerritories (
	unsigned long ulCurrentCMSPartitionIndex,
	string relativePath,
	string customerDirectoryName,
	bool deliveryRepositoriesToo,
	Customer::TerritoriesHashMap& phmTerritories);

public:
    CMSRepository (void);

    ~CMSRepository (void);

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

//     * bRemoveLinuxPathIfExist: often this method is called 
//     * 		to get the path where the encoder put his output
//     * 		(file or directory). In this case it is good
//     * 		to clean/remove that path if already existing in order
//     * 		to give to the encoder a clean place where to write
    string getStagingAssetPathName (
	string customerDirectoryName,
	string relativePath,
	string fileName,            // may be empty ("")
	long long llMediaItemKey,
	long long llPhysicalPathKey,
	bool removeLinuxPathIfExist);

    string getEncodingProfilePathName (
	long long llEncodingProfileKey,
	string profileFileNameExtension);

    string getFFMPEGEncodingProfilePathName (
	unsigned long ulContentType,
	long long llEncodingProfileKey);

    unsigned long getCustomerStorageUsage (
	string customerDirectoryName);

//    Error saveSanityCheckLastProcessedContent (
//            const char *pFilePathName);
//
//    Error readSanityCheckLastProcessedContent (
//            const char *pFilePathName);
//
//    Error sanityCheck_ContentsOnFileSystem (
//            RepositoryType rtRepositoryType);

} ;

#endif

