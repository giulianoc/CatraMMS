

#pragma once

#include <mutex>
#include <vector>
#include "MMSEngineDBFacade.h"
#include "Workspace.h"
#include "spdlog/spdlog.h"

class MMSStorage
{
  public:
	enum class RepositoryType
	{
		MMSREP_REPOSITORYTYPE_MMSCUSTOMER = 0,
		MMSREP_REPOSITORYTYPE_DOWNLOAD,
		MMSREP_REPOSITORYTYPE_STREAMING,
		MMSREP_REPOSITORYTYPE_STAGING,
		MMSREP_REPOSITORYTYPE_INGESTION,

		MMSREP_REPOSITORYTYPE_NUMBER
	};

  public:
	MMSStorage(
		bool noFileSystemAccess, bool noDatabaseAccess, std::shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, nlohmann::json configuration,
		std::shared_ptr<spdlog::logger> logger
	);

	~MMSStorage(void);

	/*
	static void createDirectories(
		json configuration,
		std::shared_ptr<spdlog::logger> logger);
	*/

	fs::path getWorkspaceIngestionRepository(std::shared_ptr<Workspace> workspace);

	static fs::path getMMSRootRepository(fs::path storage);
	fs::path getMMSRootRepository();

	static fs::path getIngestionRootRepository(fs::path storage);

	static fs::path getStagingRootRepository(fs::path storage);

	static fs::path getTranscoderStagingRootRepository(fs::path storage);

	static std::string getDirectoryForLiveContents();

	static fs::path getLiveRootRepository(fs::path storage);

	static fs::path getFFMPEGArea(fs::path storage);

	static fs::path getFFMPEGEndlessRecursivePlaylistArea(fs::path storage);

	static fs::path getNginxArea(fs::path storage);

	fs::path getErrorRootRepository(void);

	fs::path getDoneRootRepository(void);

	std::tuple<int64_t, fs::path, int, std::string, std::string, int64_t, std::string>
	getPhysicalPathDetails(int64_t mediaItemKey, int64_t encodingProfileKey, bool warningIfMissing, bool fromMaster);

	std::tuple<fs::path, int, std::string, std::string, int64_t, std::string> getPhysicalPathDetails(int64_t physicalPathKey, bool fromMaster);

	std::tuple<std::string, int, std::string, std::string> getVODDeliveryURI(int64_t physicalPathKey, bool save, std::shared_ptr<Workspace> requestWorkspace);

	std::tuple<std::string, int, int64_t, std::string, std::string>
	getVODDeliveryURI(int64_t mediaItemKey, int64_t encodingProfileKey, bool save, std::shared_ptr<Workspace> requestWorkspace);

	fs::path getLiveDeliveryAssetPath(std::string directoryId, std::shared_ptr<Workspace> requestWorkspace);

	fs::path getLiveDeliveryAssetPathName(std::string directoryId, std::string liveFileExtension, std::shared_ptr<Workspace> requestWorkspace);

	std::tuple<fs::path, fs::path, std::string> getLiveDeliveryDetails(std::string directoryId, std::string liveFileExtension, std::shared_ptr<Workspace> requestWorkspace);

	void removePhysicalPath(int64_t physicalPathKey);

	void removeMediaItem(int64_t mediaItemKey);

	void refreshPartitionsFreeSizes();

	void moveContentInRepository(std::string filePathName, RepositoryType rtRepositoryType, std::string workspaceDirectoryName, bool addDateTimeToFileName);

	void copyFileInRepository(std::string filePathName, RepositoryType rtRepositoryType, std::string workspaceDirectoryName, bool addDateTimeToFileName);

	fs::path moveAssetInMMSRepository(
		int64_t ingestionJobKey, fs::path sourceAssetPathName, std::string workspaceDirectoryName, std::string destinationFileName, std::string relativePath,

		unsigned long *pulMMSPartitionIndexUsed, // OUT
		// FileIO::DirectoryEntryType_p pSourceFileType,	// OUT: TOOLS_FILEIO_DIRECTORY or TOOLS_FILEIO_REGULARFILE

		bool deliveryRepositoriesToo, Workspace::TerritoriesHashMap &phmTerritories
	);

	fs::path getMMSAssetPathName(
		bool externalReadOnlyStorage, int partitionKey, std::string workspaceDirectoryName,
		std::string relativePath, // using '/'
		std::string fileName
	);

	// bRemoveLinuxPathIfExist: often this method is called
	// to get the path where the encoder put his output
	// (file or directory). In this case it is good
	// to clean/remove that path if already existing in order
	// to give to the encoder a clean place where to write
	fs::path getStagingAssetPathName(
		// neededForTranscoder=true uses a faster file system i.e. for recording
		bool neededForTranscoder, std::string workspaceDirectoryName, std::string directoryNamePrefix, std::string relativePath,
		std::string fileName,			 // may be empty ("")
		long long llMediaItemKey,	 // used only if fileName is ""
		long long llPhysicalPathKey, // used only if fileName is ""
		bool removeLinuxPathIfExist
	);

	unsigned long getWorkspaceStorageUsage(std::string workspaceDirectoryName);

	void deleteWorkspace(std::shared_ptr<Workspace> workspace);

	void manageTarFileInCaseOfIngestionOfSegments(
		int64_t ingestionJobKey, std::string tarBinaryPathName, std::string workspaceIngestionRepository, std::string sourcePathName
	);

	static int64_t move(int64_t ingestionJobKey, fs::path source, fs::path dest);

  private:
	bool _noFileSystemAccess;
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	std::shared_ptr<spdlog::logger> _logger;
	nlohmann::json _configuration;

	std::string _hostName;

	fs::path _storage;

	int _waitingNFSSync_maxMillisecondsToWait;
	int _freeSpaceToLeaveInEachPartitionInMB;

	void contentInRepository(
		unsigned long ulIsCopyOrMove, std::string contentPathName, RepositoryType rtRepositoryType, std::string workspaceDirectoryName,
		bool addDateTimeToFileName
	);

	// std::string getRepository(RepositoryType rtRepositoryType);

	fs::path creatingDirsUsingTerritories(
		unsigned long ulCurrentMMSPartitionIndex, std::string relativePath, std::string workspaceDirectoryName, bool deliveryRepositoriesToo,
		Workspace::TerritoriesHashMap &phmTerritories
	);

	// void refreshPartitionFreeSizes(PartitionInfo& partitionInfo);

	void removePhysicalPathFile(
		int64_t mediaItemKey, int64_t physicalPathKey, MMSEngineDBFacade::DeliveryTechnology deliveryTechnology, std::string fileName,
		bool externalReadOnlyStorage, int mmsPartitionNumber, std::string workspaceDirectoryName, std::string relativePath, uint64_t sizeInBytes
	);
};
