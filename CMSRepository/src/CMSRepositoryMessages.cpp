
#include "CMSRepositoryMessages.h"


ErrMsgBase:: ErrMsgsInfo CMSRepositoryMessagesStr = {

    // CMSRepository
	CMSREP_CMSREPOSITORY_AVAILABLESPACE,
		"Available space for %s is: %lluMB",
	CMSREP_CMSREPOSITORY_PARTITIONSELECTED,
		"Customer: %s. Selected the %ld CMSPartition for the %s content. (Current partition index: %lu, Current partition space available: %lluMB)",
	CMSREP_CMSREPOSITORY_REMOVEFILE,
		"Customer: %s. Remove file: %s",
	CMSREP_CMSREPOSITORY_REMOVEDIRECTORY,
		"Customer: %s. Remove directory: %s",
	CMSREP_CMSREPOSITORY_CREATINGDIRS,
		"Calling creatingDirsUsingTerritories. CMSPartitionNumber: %lu, RelativePath: %s, DeliveryRepositoryToo: %ld",
	CMSREP_CMSREPOSITORY_CREATEDIRECTORY,
		"Customer: %s. Create directory: %s",
	CMSREP_CMSREPOSITORY_SANITYCHECKSTATUS,
		"Sanity check status. Repository: %s, FileIndex: %lu, CurrentFileNumberProcessedInThisSchedule: %lu",
	CMSREP_CMSREPOSITORY_PARTITIONFREESPACETOOLOW,
		"Partition space too low. Partition index: %lu, Partition free space (KB): %llu, Free space to leave in each partition (KB): %llu",
	CMSREP_CMSREPOSITORY_MOVEFILE,
		"Customer: %s. Moving file %s in %s",
	CMSREP_CMSREPOSITORY_MOVEDIRECTORY,
		"Customer: %s. Move directory %s in %s",
	CMSREP_CMSREPOSITORY_COPYFILE,
		"Customer: %s. Copy file %s in %s",
	CMSREP_CMSREPOSITORY_COPYDIRECTORY,
		"Customer: %s. Copy directory %s in %s",
	CMSREP_CMSREPOSITORY_CMSPARTITIONAVAILABLESPACE,
		"The available space for the %lu CMS partition is %llu MB",
	CMSREP_CMSREPOSITORY_STARTSANITYCHECKONREPOSITORY,
		"Start Sanity Check on repository: %s",
	CMSREP_CMSREPOSITORY_ENDSANITYCHECKONREPOSITORY,
		"End Sanity Check on repository: ____%s____. Elapsed seconds: %lu. LastProcessedContent: Partition %s, CustomerName %s, FilesNumberAlreadyProcessed %lu. Files removed: ____%lu____, Directories removed: ____%lu____",
	CMSREP_CMSREPOSITORY_SAVINGSANITYCHECKINFO,
		"Saving sanity check info. Repository: %ld, Partition: %s, CustomerName: %s, FilesNumberAlreadyProcessed: %lu",
	CMSREP_CMSREPOSITORY_READSANITYCHECKINFO,
		"Read sanity check info. Repository: %ld, Partition: %s, CustomerName: %s, FilesNumberAlreadyProcessed: %lu",
	CMSREP_CMSREPOSITORY_SANITYCHECKONCUSTOMERSDIRECTORY,
		"Sanity Check on customers directory: %s",
	CMSREP_CMSREPOSITORY_SANITYCHECKONDIRECTORY,
		"Sanity Check on directory. CustomerName: %s, Directory: %s, RelativePathIndex: %lu, DirectoryLevel: %lu, File index: %lu, FilesNumberAlreadyProcessed: %lu",

	// Common
	CMSREP_HTTPRESPONSE,
		"Customer: %s. Servlet name: %s, HEADER: %s, BODY: %s, Response time (MilliSecs): @%lu@",
	CMSREP_HTTPPOSTTHREAD,
		"Initialization of %s thread. WebServerIPAddress: %s, WebServerPort: %lu, LocalIPAddress: %s, URI: %s, URL parameters: %s, Body: %s, Timeout: %lu"

	// Insert here other errors...

} ;

