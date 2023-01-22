

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
		bool noFileSystemAccess,
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		Json::Value configuration,
		shared_ptr<spdlog::logger> logger);

    ~MMSStorage (void);

	static void createDirectories(
		Json::Value configuration,
		shared_ptr<spdlog::logger> logger);

    fs::path getWorkspaceIngestionRepository(shared_ptr<Workspace> workspace);

	static fs::path getMMSRootRepository (fs::path storage);
    fs::path getMMSRootRepository ();

    static fs::path getIngestionRootRepository (fs::path storage);
    
    static fs::path getStagingRootRepository (fs::path storage);

    static fs::path getTranscoderStagingRootRepository (fs::path storage);

	static string getDirectoryForLiveContents();

	static fs::path getLiveRootRepository(fs::path storage);

	static fs::path getFFMPEGArea(fs::path storage);

	static fs::path getFFMPEGEndlessRecursivePlaylistArea(fs::path storage);

	static fs::path getNginxArea(fs::path storage);

    fs::path getErrorRootRepository (void);

    fs::path getDoneRootRepository (void);

	tuple<int64_t, fs::path, int, string, string, int64_t, string>
		getPhysicalPathDetails(
		int64_t mediaItemKey, int64_t encodingProfileKey,
		bool warningIfMissing, bool fromMaster);

	tuple<fs::path, int, string, string, int64_t, string> getPhysicalPathDetails(
		int64_t physicalPathKey, bool fromMaster);

	tuple<string, int, string, string> getVODDeliveryURI(
		int64_t physicalPathKey, bool save, shared_ptr<Workspace> requestWorkspace);

	tuple<string, int, int64_t, string, string> getVODDeliveryURI(
		int64_t mediaItemKey,
		int64_t encodingProfileKey, bool save,
		shared_ptr<Workspace> requestWorkspace);

	fs::path getLiveDeliveryAssetPath(
		string directoryId,
		shared_ptr<Workspace> requestWorkspace);

	fs::path getLiveDeliveryAssetPathName(
		string directoryId,
		string liveFileExtension, shared_ptr<Workspace> requestWorkspace);

	tuple<fs::path, fs::path, string> getLiveDeliveryDetails(
		string directoryId, string liveFileExtension,
		shared_ptr<Workspace> requestWorkspace);

    void removePhysicalPath(int64_t physicalPathKey);
    
    void removeMediaItem(int64_t mediaItemKey);

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

    fs::path moveAssetInMMSRepository (
		int64_t ingestionJobKey,
        fs::path sourceAssetPathName,
        string workspaceDirectoryName,
        string destinationFileName,
        string relativePath,

        unsigned long *pulMMSPartitionIndexUsed,	// OUT
		// FileIO::DirectoryEntryType_p pSourceFileType,	// OUT: TOOLS_FILEIO_DIRECTORY or TOOLS_FILEIO_REGULARFILE

        bool deliveryRepositoriesToo,
        Workspace::TerritoriesHashMap& phmTerritories
    );

    fs::path getMMSAssetPathName (
		bool externalReadOnlyStorage,
		int partitionKey,
		string workspaceDirectoryName,
		string relativePath,		// using '/'
		string fileName);

    // bRemoveLinuxPathIfExist: often this method is called 
    // to get the path where the encoder put his output
    // (file or directory). In this case it is good
    // to clean/remove that path if already existing in order
    // to give to the encoder a clean place where to write
    fs::path getStagingAssetPathName (
		// neededForTranscoder=true uses a faster file system i.e. for recording
		bool neededForTranscoder,
		string workspaceDirectoryName,
        string directoryNamePrefix,
		string relativePath,
		string fileName,                // may be empty ("")
		long long llMediaItemKey,       // used only if fileName is ""
		long long llPhysicalPathKey,    // used only if fileName is ""
		bool removeLinuxPathIfExist);

    unsigned long getWorkspaceStorageUsage (
		string workspaceDirectoryName);

	void deleteWorkspace(
		shared_ptr<Workspace> workspace);

	void manageTarFileInCaseOfIngestionOfSegments(
		int64_t ingestionJobKey,
		string tarBinaryPathName, string workspaceIngestionRepository,
		string sourcePathName);

private:
	bool						_noFileSystemAccess;
	shared_ptr<MMSEngineDBFacade>	_mmsEngineDBFacade;
    shared_ptr<spdlog::logger>  _logger;
	Json::Value					_configuration;

    string                      _hostName;

	fs::path					_storage;

	int						_waitingNFSSync_maxMillisecondsToWait;
	int						_freeSpaceToLeaveInEachPartitionInMB;


	void contentInRepository (unsigned long ulIsCopyOrMove,
		string contentPathName,
		RepositoryType rtRepositoryType,
		string workspaceDirectoryName,
		bool addDateTimeToFileName);

	// string getRepository(RepositoryType rtRepositoryType);

	fs::path creatingDirsUsingTerritories (
		unsigned long ulCurrentMMSPartitionIndex,
		string relativePath,
		string workspaceDirectoryName,
		bool deliveryRepositoriesToo,
		Workspace::TerritoriesHashMap& phmTerritories);

	// void refreshPartitionFreeSizes(PartitionInfo& partitionInfo);

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

