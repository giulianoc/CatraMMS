

#ifndef MMSStorage_h
#define MMSStorage_h

#include <mutex>
#include <vector>
#include "spdlog/spdlog.h"
#include "catralibraries/FileIO.h"
#include "Workspace.h"
#include "MMSEngineDBFacade.h"


class MMSStorage
{
public:
    enum class RepositoryType
    {
        MMSREP_REPOSITORYTYPE_MMSCUSTOMER	= 0,
        MMSREP_REPOSITORYTYPE_DOWNLOAD,
        MMSREP_REPOSITORYTYPE_STREAMING,
        MMSREP_REPOSITORYTYPE_STAGING,
        MMSREP_REPOSITORYTYPE_INGESTION,

        MMSREP_REPOSITORYTYPE_NUMBER
    };

public:
    MMSStorage (
            Json::Value configuration,
            shared_ptr<spdlog::logger> logger);

    ~MMSStorage (void);

	static void createDirectories(
		Json::Value configuration,
		shared_ptr<spdlog::logger> logger);

    string getWorkspaceIngestionRepository(shared_ptr<Workspace> workspace);
    
    //    const char *getIPhoneAliasForLive (void);

    static string getMMSRootRepository (string storage);
    string getMMSRootRepository ();

    // string getStreamingRootRepository (void);

    // string getDownloadRootRepository (void);

    static string getIngestionRootRepository (string storage);
    
    static string getStagingRootRepository (string storage);

    static string getTranscoderStagingRootRepository (string storage);

	static string getDirectoryForLiveContents();

	static string getLiveRootRepository(string storage);

	static string getFFMPEGArea(string storage);

	static string getNginxArea(string storage);

    string getErrorRootRepository (void);

    string getDoneRootRepository (void);

	tuple<int64_t, string, int, string, string, int64_t, string>
		getPhysicalPathDetails(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		int64_t mediaItemKey, int64_t encodingProfileKey,
		bool warningIfMissing);

	tuple<string, int, string, string, int64_t, string> getPhysicalPathDetails(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, int64_t physicalPathKey);

	pair<string, string> getVODDeliveryURI(
			shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
			int64_t physicalPathKey, bool save, shared_ptr<Workspace> requestWorkspace);

	tuple<int64_t, string, string> getVODDeliveryURI(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, int64_t mediaItemKey,
		int64_t encodingProfileKey, bool save,
		shared_ptr<Workspace> requestWorkspace);

	string getLiveDeliveryAssetPath(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		string directoryId,
		shared_ptr<Workspace> requestWorkspace);

	string getLiveDeliveryAssetPathName(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		string directoryId,
		string liveFileExtension, shared_ptr<Workspace> requestWorkspace);

	tuple<string, string, string> getLiveDeliveryDetails(
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		string directoryId, string liveFileExtension,
		shared_ptr<Workspace> requestWorkspace);

    void removePhysicalPath(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		int64_t physicalPathKey);
    
    void removeMediaItem(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		int64_t mediaItemKey);

    void refreshPartitionsFreeSizes();

    void moveContentInRepository (
        string filePathName,
        RepositoryType rtRepositoryType,
        string workspaceDirectoryName,
        bool addDateTimeToFileName);

    void copyFileInRepository (
	string filePathName,
	RepositoryType rtRepositoryType,
	string workspaceDirectoryName,
	bool addDateTimeToFileName);

    string moveAssetInMMSRepository (
		int64_t ingestionJobKey,
        string sourceAssetPathName,
        string workspaceDirectoryName,
        string destinationFileName,
        string relativePath,

        bool isPartitionIndexToBeCalculated,
        unsigned long *pulMMSPartitionIndexUsed,	// OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

        bool deliveryRepositoriesToo,
        Workspace::TerritoriesHashMap& phmTerritories
    );

    string getMMSAssetPathName (
		bool externalReadOnlyStorage,
		unsigned long ulPartitionNumber,
		string workspaceDirectoryName,
		string relativePath,		// using '/'
		string fileName);

	/*
    string getDownloadLinkPathName (
		unsigned long ulPartitionNumber,
		string workspaceDirectoryName,
		string territoryName,
		string relativePath,
		string fileName,
		bool downloadRepositoryToo);

    string getStreamingLinkPathName (
		unsigned long ulPartitionNumber,	// IN
		string workspaceDirectoryName,	// IN
		string territoryName,	// IN
		string relativePath,	// IN
		string fileName);	// IN
	*/

    // bRemoveLinuxPathIfExist: often this method is called 
    // to get the path where the encoder put his output
    // (file or directory). In this case it is good
    // to clean/remove that path if already existing in order
    // to give to the encoder a clean place where to write
    string getStagingAssetPathName (
		// neededForTranscoder=true uses a faster file system i.e. for recording
		bool neededForTranscoder,
		string workspaceDirectoryName,
        string directoryNamePrefix,
		string relativePath,
		string fileName,                // may be empty ("")
		long long llMediaItemKey,       // used only if fileName is ""
		long long llPhysicalPathKey,    // used only if fileName is ""
		bool removeLinuxPathIfExist);

	/*
	string getDeliveryFreeAssetPathName(
		string workspaceDirectoryName,
		string liveProxyAssetName,
		string assetExtension);
	*/

	/*
    string getEncodingProfilePathName (
		long long llEncodingProfileKey,
		string profileFileNameExtension);

    string getFFMPEGEncodingProfilePathName(
        MMSEngineDBFacade::ContentType contentType,
        long long llEncodingProfileKey);
	*/

    unsigned long getWorkspaceStorageUsage (
		string workspaceDirectoryName);

	void deleteWorkspace(
		shared_ptr<Workspace> workspace);

private:
    shared_ptr<spdlog::logger>  _logger;
	Json::Value					_configuration;

    string                      _hostName;

    string                      _storage;
    // string                      _mmsRootRepository;
    // string                      _downloadRootRepository;
    // string                      _streamingRootRepository;
    // string                      _stagingRootRepository;
	// string						_transcoderStagingRootRepository;
	// string						_deliveryFreeRootRepository;
	// string						_directoryForLiveContents;
	// string						_liveRootRepository;
    // string                      _ingestionRootRepository;
    // string                      _profilesRootRepository;

	int								_freeSpaceToLeaveInEachPartitionInMB;

	struct PartitionInfo {
		// it is without / at the end
		string			_partitionPathName;

		// getFileSystemInfo (default and more performance) or getDirectoryUsage (less performance)
		string			_partitionUsageType;

		// this is in case we have to use a subset of the partition
		int64_t			_maxStorageUsageInKB;


		// real free size got from file system
		int64_t			_currentFreeSizeInBytes;

		chrono::system_clock::time_point	_lastUpdateFreeSize;
	};
    recursive_mutex                 _mtMMSPartitions;
    vector<PartitionInfo>			_mmsPartitionsInfo;
    unsigned long                   _ulCurrentMMSPartitionIndex;

    
    void contentInRepository (
	unsigned long ulIsCopyOrMove,
	string contentPathName,
	RepositoryType rtRepositoryType,
	string workspaceDirectoryName,
	bool addDateTimeToFileName);

    string getRepository(RepositoryType rtRepositoryType);

    string creatingDirsUsingTerritories (
		unsigned long ulCurrentMMSPartitionIndex,
		string relativePath,
		string workspaceDirectoryName,
		bool deliveryRepositoriesToo,
		Workspace::TerritoriesHashMap& phmTerritories);

	void refreshPartitionFreeSizes(PartitionInfo& partitionInfo);

	void removePhysicalPathFile(
		int64_t mediaItemKey,
		int64_t physicalPathKey,
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology,
		string fileName,
		bool externalReadOnlyStorage,
		int mmsPartitionNumber,
		string workspaceDirectoryName,
		string relativePath,
		int64_t sizeInBytes);

} ;

#endif

