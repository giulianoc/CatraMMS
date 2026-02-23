
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

	MMSStorage(
		bool noFileSystemAccess, bool noDatabaseAccess, const std::shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade,
		const nlohmann::json& configuration
	);

	~MMSStorage();

	fs::path getWorkspaceIngestionRepository(const std::shared_ptr<Workspace>& workspace);

	static fs::path getMMSRootRepository(const fs::path& storage);
	fs::path getMMSRootRepository();

	static fs::path getIngestionRootRepository(const fs::path& storage);

	static fs::path getStagingRootRepository(const fs::path& storage);

	static fs::path getTranscoderStagingRootRepository(const fs::path& storage);

	static std::string getDirectoryForLiveContents();

	static fs::path getLiveRootRepository(const fs::path& storage);

	static fs::path getFFMPEGArea(const fs::path& storage);

	static fs::path getFFMPEGEndlessRecursivePlaylistArea(const fs::path& storage);

	static fs::path getNginxArea(const fs::path& storage);

	std::tuple<int64_t, fs::path, int, std::string, std::string, int64_t, std::string> getPhysicalPathDetails(int64_t mediaItemKey,
		int64_t encodingProfileKey, bool warningIfMissing, bool fromMaster);

	std::tuple<fs::path, int, std::string, std::string, int64_t, std::string> getPhysicalPathDetails(int64_t physicalPathKey, bool fromMaster);

	std::tuple<std::string, int, std::string, std::string> getVODDeliveryURI(int64_t physicalPathKey, bool save,
		const std::shared_ptr<Workspace>& requestWorkspace) const;

	std::tuple<std::string, int, int64_t, std::string, std::string> getVODDeliveryURI(int64_t mediaItemKey, int64_t encodingProfileKey,
		bool save, const std::shared_ptr<Workspace>& requestWorkspace) const;

	fs::path getLiveDeliveryAssetPath(const std::string &directoryId, const std::shared_ptr<Workspace> &requestWorkspace);

	fs::path getLiveDeliveryAssetPathName(
		const std::string &directoryId, const std::string &liveFileExtension, const std::shared_ptr<Workspace> &requestWorkspace
	);

	static std::tuple<fs::path, fs::path, std::string> getLiveDeliveryDetails(const std::string &directoryId,
		const std::string &liveFileExtension, const std::shared_ptr<Workspace> &requestWorkspace);

	void removePhysicalPath(int64_t physicalPathKey);

	void removeMediaItem(int64_t mediaItemKey);

	void refreshPartitionsFreeSizes();

	fs::path moveAssetInMMSRepository(
		int64_t ingestionJobKey, const fs::path &sourceAssetPathName, const std::string &workspaceDirectoryName,
		const std::string &destinationAssetFileName,
		const std::string &relativePath, int16_t& mmsPartitionIndexUsed // OUT
	);

	fs::path getMMSAssetPathName(
		bool externalReadOnlyStorage, int64_t partitionKey, const std::string &workspaceDirectoryName, const std::string &relativePath,
		// using '/'
		const std::string &fileName
	);

	// bRemoveLinuxPathIfExist: often this method is called
	// to get the path where the encoder put his output
	// (file or directory). In this case it is good
	// to clean/remove that path if already existing in order
	// to give to the encoder a clean place where to write
	fs::path getStagingAssetPathName(
		// neededForTranscoder=true uses a faster file system i.e. for recording
		bool neededForTranscoder, const std::string& workspaceDirectoryName, const std::string& directoryNamePrefix,
		const std::string& relativePath,
		const std::string& fileName,			 // may be empty ("")
		int64_t mediaItemKey,	 // used only if fileName is ""
		int64_t physicalPathKey, // used only if fileName is ""
		bool removeLinuxPathIfExist
	);

	unsigned long getWorkspaceStorageUsage(const std::string& workspaceDirectoryName);

	void deleteWorkspace(const std::shared_ptr<Workspace>& workspace);

	void manageTarFileInCaseOfIngestionOfSegments(
		int64_t ingestionJobKey, std::string tarBinaryPathName, std::string workspaceIngestionRepository, std::string sourcePathName
	);

	static int64_t move(int64_t ingestionJobKey, const fs::path& source, const fs::path& dest);

  private:
	bool _noFileSystemAccess;
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	nlohmann::json _configuration;

	std::string _hostName;

	fs::path _storage;

	int32_t _waitingNFSSync_maxMillisecondsToWait;
	int32_t _freeSpaceToLeaveInEachPartitionInMB;

	void removePhysicalPathFile(
		int64_t mediaItemKey, int64_t physicalPathKey, MMSEngineDBFacade::DeliveryTechnology deliveryTechnology, std::string fileName,
		bool externalReadOnlyStorage, int mmsPartitionNumber, std::string workspaceDirectoryName, std::string relativePath, uint64_t sizeInBytes
	);
};
