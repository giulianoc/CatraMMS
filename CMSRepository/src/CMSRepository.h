
/*
#ifndef CMSRepository_h
#define CMSRepository_h

#include "catralibraries/ConfigurationFile.h"
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

private:
    Error contentInRepository (
            unsigned long ulIsCopyOrMove,
            const char *pFilePathName,
            RepositoryType_t rtRepositoryType,
            const char *pCustomerDirectoryName,
            Boolean_t bAddDateTimeToFileName);

    Error sanityCheck_CustomersDirectory (
            RepositoryType_t rtRepositoryType,
            const char *pCustomersDirectory,
            FileIO:: Directory_p pdDeliveryDirectory,
            SanityCheckContentInfo_p psciSanityCheckContentsInfo,
            long *plSanityCheckContentsInfoNumber,
            unsigned long *pulFileIndex,
            unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
            unsigned long *pulCurrentContentsRemovedNumberInThisSchedule,
            unsigned long *pulCurrentOtherFilesRemovedNumberInThisSchedule,
            unsigned long *pulDirectoryLevelIndexInsideCustomer,
            Boolean_p pbHasCustomerToBeResumed);

    Error sanityCheck_ContentsDirectory (
            const char *pCustomerDirectoryName, const char *pContentsDirectory,
            unsigned long ulRelativePathIndex,
            RepositoryType_t rtRepositoryType,
            SanityCheckContentInfo_p psciSanityCheckContentsInfo,
            long *plSanityCheckContentsInfoNumber,
            unsigned long *pulFileIndex,
            unsigned long *pulCurrentFileNumberProcessedInThisSchedule,
            unsigned long *pulCurrentContentsRemovedNumberInThisSchedule,
            unsigned long *pulCurrentOtherFilesRemovedNumberInThisSchedule,
            unsigned long *pulDirectoryLevelIndexInsideCustomer);

    Error sanityCheck_runOnContentsInfo (
            SanityCheckContentInfo_p psciSanityCheckContentsInfo,
            unsigned long ulSanityCheckContentsInfoNumber,
            RepositoryType_t rtRepositoryType,
            unsigned long *pulCurrentContentsRemovedNumberInThisSchedule,
            unsigned long *pulCurrentOtherFilesRemovedNumberInThisSchedule);

private:
    string                      _hostName;
    ConfigurationFile_p			_pcfConfiguration;
    Buffer_p					_pbRepositories [
            CMSREP_REPOSITORYTYPE_NUMBER];

    string                      _downloadReservedDirectoryName;
    string                      _downloadFreeDirectoryName;
    string                      _downloadiPhoneLiveDirectoryName;
    string                      _downloadSilverlightLiveDirectoryName;
    string                      _downloadAdobeLiveDirectoryName;
    string                      _streamingFreeDirectoryName;
    string                      _streamingMetaDirectoryName;
    string                      _streamingRecordingDirectoryName;
    string                      _iPhoneAliasForLive;

    unsigned long long          _freeSpaceToLeaveInEachPartition;
    bool                        _unexpectedFilesToBeRemoved;
    unsigned long               _retentionPeriodInSecondsForTemporaryFiles;
    unsigned long               _maxFilesToBeProcessedPerSchedule [
            CMSREP_REPOSITORYTYPE_NUMBER];
    SanityCheckLastProcessedContent         _lastProcessedContent [
            CMSREP_REPOSITORYTYPE_NUMBER];

    string                      _fTPRootRepository;
    string                      _cMSRootRepository;
    string                      _downloadRootRepository;
    string                      _streamingRootRepository;
    string                      _errorRootRepository;
    string                      _doneRootRepository;
    string                      _profilesRootRepository;
    string                      _profilesRootDirectoryFromXOEMachine;
    string                      _stagingRootRepository;
    string                      _stagingRootRepositoryFromXOEMachine;

    mutex                       _mtCMSPartitions;
    unsigned long               _cMSPartitionsNumber;
    unsigned long long          *_pullCMSPartitionsFreeSizeInMB;
    unsigned long               _ulCurrentCMSPartitionIndex;


    Error creatingDirsUsingTerritories (
            unsigned long ulCurrentCMSPartitionIndex,
            const char *pRelativePath,
            const char *pCustomerDirectoryName,
            Boolean_t bDeliveryRepositoriesToo,
            TerritoriesHashMap_p phmTerritories,
            Buffer_p pbCMSAssetPathName);

public:
    CMSRepository (void);

    ~CMSRepository (void);

    Error init (
            ConfigurationFile_p pcfConfiguration,
            LoadBalancer_p plbWebServerLoadBalancer,
            Tracer_p ptTracer);

    Error finish ();

    const char *getIPhoneAliasForLive (void);

    const char *getCMSRootRepository (void);

    const char *getStreamingRootRepository (void);

    const char *getDownloadRootRepository (void);

    const char *getFTPRootRepository (void);

    const char *getStagingRootRepository (void);

    const char *getErrorRootRepository (void);

    const char *getDoneRootRepository (void);

    Error refreshPartitionsFreeSizes (void);

    Error saveSanityCheckLastProcessedContent (
            const char *pFilePathName);

    Error readSanityCheckLastProcessedContent (
            const char *pFilePathName);

    Error moveContentInRepository (
            const char *pFilePathName,
            RepositoryType_t rtRepositoryType,
            const char *pCustomerDirectoryName,
            Boolean_t bAddDateTimeToFileName);

    Error copyFileInRepository (
            const char *pFilePathName,
            RepositoryType_t rtRepositoryType,
            const char *pCustomerDirectoryName,
            Boolean_t bAddDateTimeToFileName);

    Error moveAssetInCMSRepository (
            const char *pSourceAssetPathName,
            const char *pCustomerDirectoryName,
            const char *pDestinationFileName,
            const char *pRelativePath,

            Boolean_t bIsPartitionIndexToBeCalculated,
            unsigned long *pulCMSPartitionIndexUsed,

            Boolean_t bDeliveryRepositoriesToo,
            TerritoriesHashMap_p phmTerritories,

            Buffer_p pbCMSAssetPathName);

    Error sanityCheck_ContentsOnFileSystem (
            RepositoryType_t rtRepositoryType);

    Error getCMSAssetPathName (
            Buffer_p pbAssetPathName,
            unsigned long ulPartitionNumber,
            const char *pCustomerDirectoryName,
            const char *pRelativePath,
            const char *pFileName,
            Boolean_t bIsFromXOEMachine);

    Error getDownloadLinkPathName (
            Buffer_p pbLinkPathName,
            unsigned long ulPartitionNumber,
            const char *pCustomerDirectoryName,
            const char *pTerritoryName,
            const char *pRelativePath,
            const char *pFileName,
            Boolean_t bDownloadRepositoryToo = true);

    Error getStreamingLinkPathName (
            Buffer_p pbLinkPathName,
            unsigned long ulPartitionNumber,
            const char *pCustomerDirectoryName,
            const char *pTerritoryName,
            const char *pRelativePath,
            const char *pFileName);

//     * bRemoveLinuxPathIfExist: often this method is called 
//     * 		to get the path where the encoder put his output
//     * 		(file or directory). In this case it is good
//     * 		to clean/remove that path if already existing in order
//     * 		to give to the encoder a clean place where to write
    Error getStagingAssetPathName (
            Buffer_p pbAssetPathName,
            const char *pCustomerDirectoryName,
            const char *pRelativePath,
            const char *pFileName,
            long long llMediaItemKey,
            long long llPhysicalPathKey,
            Boolean_t bIsFromXOEMachine,
            Boolean_t bRemoveLinuxPathIfExist);

    Error getEncodingProfilePathName (
            Buffer_p pbEncodingProfilePathName,
            long long llEncodingProfileKey,
            const char *pProfileFileNameExtension,
            Boolean_t bIsFromXOEMachine);

    Error getFFMPEGEncodingProfilePathName (
            unsigned long ulContentType,
            Buffer_p pbEncodingProfilePathName,
            long long llEncodingProfileKey);

    Error getCustomerStorageUsage (
            const char *pCustomerDirectoryName,
            unsigned long *pulStorageUsageInMB);
} ;

#endif
*/
