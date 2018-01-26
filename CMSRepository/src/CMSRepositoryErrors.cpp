
#include "CMSRepositoryErrors.h"



ErrMsgBase:: ErrMsgsInfo CMSRepositoryErrorsStr = {

	// CMSRepository
	{ CMSREP_CMSREPOSITORY_INIT_FAILED,
		"The method init of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_FINISH_FAILED,
		"The method finish of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_CREATINGDIRSUSINGTERRITORIES_FAILED,
		"The method creatingDirsUsingTerritories of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_REFRESHPARTITIONSFREESIZES_FAILED,
		"The method refreshPartitionsFreeSizes of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSONFILESYSTEM_FAILED,
		"The method sanityCheck_ContentsOnFileSystem of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECK_CUSTOMERSDIRECTORY_FAILED,
		"The method sanityCheck_CustomersDirectory of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECK_CONTENTSDIRECTORY_FAILED,
		"The method sanityCheck_ContentsDirectory of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECK_RUNONCONTENTSINFO_FAILED,
		"The method sanityCheck_runOnContentsInfo of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_GETCMSASSETPATHNAME_FAILED,
		"The method getCMSAssetPathName of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_GETDOWNLOADLINKPATHNAME_FAILED,
		"The method getDownloadLinkPathName of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_GETSTREAMINGLINKPATHNAME_FAILED,
		"The method getStreamingLinkPathName of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_GETSTAGINGASSETPATHNAME_FAILED,
		"The method getStagingAssetPathName of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_GETENCODINGPROFILEPATHNAME_FAILED,
		"The method getEncodingProfilePathName of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_GETIMAGEENCODINGPROFILETPATHNAME_FAILED,
		"The method getImageEncodingProfilePathName of the CMSRepository class failed" },
	{ CMS_CMSREPOSITORY_GETFFMPEGENCODINGPROFILETPATHNAME_FAILED,
		"The method getFFMPEGEncodingProfilePathName of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_MOVEASSETINCMSREPOSITORY_FAILED,
		"The method moveAssetInCMSRepository of the CMSRepository class failed. Path name: %s, Content Provider: %s" },
	{ CMSREP_CMSREPOSITORY_MOVECONTENTINREPOSITORY_FAILED,
		"The method moveContentInRepository of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_COPYFILEINREPOSITORY_FAILED,
		"The method copyFileInRepository of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_GETCUSTOMERSTORAGEUSAGE_FAILED,
		"The method getCustomerStorageUsage of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_READSANITYCHECKLASTPROCESSEDCONTENT_FAILED,
		"The method readSanityCheckLastProcessedContent of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_SAVESANITYCHECKLASTPROCESSEDCONTENT_FAILED,
		"The method saveSanityCheckLastProcessedContent of the CMSRepository class failed" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECKFILESYSTEMDBNOTCONSISTENT,
		"SanityCheck. File system and DB not consistent. Repository: %s, Customer: %s, TerritoryName: %s, RelativePath: %s, FileName: %s, DB ContentFound: %lu, DB PublishingStatus: %lu, Will be removed?: %ld" },
	{ CMSREP_CMSREPOSITORY_REACHEDMAXNUMBERTOBEPROCESSED,
		"SanityCheck. Repository: %s, reached the max number of files to be processed: %lu" },
	{ CMSREP_CMSREPOSITORY_NOCMSPARTITIONFOUND,
		"No CMS partitions (CMS_XXXX) found" },
	{ CMSREP_CMSREPOSITORY_NOMORESPACEINCMSPARTITIONS,
		"No more CMS space available to ingest the new file of %llu KB" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDFILE,
		"SanityCheck. Found unexpected file. Path: %s, Name: %s" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECKUNEXPECTEDDIRECTORY,
		"SanityCheck. Found unexpected directory. PathName: %s" },
	{ CMSREP_CMSREPOSITORY_SANITYCHECKFILETOOBIGTOBEREMOVED,
		"File too big (%lu KB) to be removed: %s" },
	{ CMSREP_CMSREPOSITORY_WRONGDIRECTORYENTRYTYPE,
		"Wrong directory entry type: %ld. Path name: %s" },
	{ CMSREP_CMSREPOSITORY_BUFFERNOTENOUGHBIG,
		"Buffer not enough big. Current length: %lu, Requested length: %lu" },
	{ CMSREP_CMSREPOSITORY_UNEXPECTEDFILEINSTAGING,
		"Found unexpected file in Staging: %s" },

	// Customer
	{ CMSREP_CUSTOMER_COPYFROM_FAILED,
		"The method copyFrom of the Customer class failed" },
	{ CMSREP_HASHMAP_INSERTWITHOUTDUPLICATION_FAILED,
		"HashMap -> InsertWithoutDuplication failed" },

	// libxml2
	{ CMSREP_LIBXML2_XMLPARSEMEMORY_FAILED,
		"%s servlet. xmlParseMemory failed. BODY: [%s]" },
	{ CMSREP_LIBXML2_XMLDOCROOTELEMENT_FAILED,
		"xmlDocGetRootElement failed" },
	{ CMSREP_XMLWRONG,
		"%s servlet. XML wrong. BODY: [%s]" },
	{ CMSREP_XMLPARAMETERUNKNOWN,
		"%s servlet. XML parameter unknown: '%s'" },

	// common
	{ CMSREP_ACTIVATION_WRONG,
		"Activation wrong" },
	{ CMSREP_SSCANF_FAILED,
		"sscanf failed" },
	{ CMSREP_NEW_FAILED,
		"new failed" },
	{ CMSREP_SERVLETFAILED,
		"%s servlet failed on the %s machine. Resource: %s, Body response: %s" }

	// Insert here other errors...

} ;

